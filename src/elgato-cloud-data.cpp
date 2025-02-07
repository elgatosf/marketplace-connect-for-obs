/*
Elgato Deep-Linking OBS Plug-In
Copyright (C) 2024 Corsair Memory Inc. oss.elgato@corsair.com

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <random>
#include <chrono>

#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <util/platform.h>

#include <curl/curl.h>
#include <QCryptographicHash>
#include <QApplication>
#include <QThread>
#include <QMetaObject>
#include <QDir>

#include <plugin-support.h>
#include "elgato-cloud-window.hpp"
#include "elgato-cloud-data.hpp"
#include "platform.h"
#include "util.h"
#include "api.hpp"

namespace elgatocloud {
ElgatoCloud *elgatoCloud = nullptr;

ElgatoCloud *GetElgatoCloud()
{
	return elgatoCloud;
}

std::mutex *GetElgatoCloudMutex()
{
	return elgatoCloud ? &elgatoCloud->m : nullptr;
}

std::unique_lock<std::mutex> *GetElgatoCloudLoopLock()
{
	return elgatoCloud ? elgatoCloud->mainLoopLock : nullptr;
}

ElgatoCloud::ElgatoCloud(obs_module_t *m)
{
	_modulePtr = m;
	//_translate = t;
	_securerand = QRandomGenerator::securelySeeded();
	_Initialize();
	_Listen();
}

ElgatoCloud::~ElgatoCloud()
{
	obs_data_release(_config);
}

obs_data_t *ElgatoCloud::GetConfig()
{
	obs_data_addref(_config);
	return _config;
}

void ElgatoCloud::SaveConfig()
{
	save_module_config(_config);
}

obs_module_t *ElgatoCloud::GetModule()
{
	return _modulePtr;
}

void ElgatoCloud::Thread()
{
	// Here is our Elgato Cloud loop
}

void ElgatoCloud::_TokenRefresh(bool loadData, bool loadUserDetails)
{
	const auto now = std::chrono::system_clock::now();
	const auto epoch = now.time_since_epoch();
	const auto seconds =
		std::chrono::duration_cast<std::chrono::seconds>(epoch);
	obs_log(LOG_INFO, "Token Refresh? %i < %i", seconds.count(),
		_accessTokenExpiration);
	if (seconds.count() < _accessTokenExpiration) {
		loggedIn = true;
		loading = loadData;
		if (loadUserDetails) {
			_LoadUserData();
		}
		if (loadData) {
			LoadPurchasedProducts();
		}
		return;
	}
	auto api = MarketplaceApi::getInstance();
	std::string encodeddata;
	encodeddata += "grant_type=refresh_token";
	encodeddata += "&refresh_token=" + _refreshToken;
	encodeddata += "&client_id=elgatolink";
	std::string url = api->authUrl();
	url += "/auth/realms/mp/protocol/openid-connect/token?";
	url += encodeddata;
	obs_log(LOG_INFO, "Token Refresh Called");
	auto response = fetch_string_from_post(url, encodeddata);
	try {
		auto responseJson = nlohmann::json::parse(response);
		_ProcessLogin(responseJson, loadData);
	} catch (...) {
		loggedIn = false;
		loading = false;
		authorizing = false;
		loginError = true;
		if (mainWindowOpen && window) {
			QMetaObject::invokeMethod(
				QCoreApplication::instance()->thread(),
				[this]() {
					window->setLoggedIn();
				});
		}
	}
}

void ElgatoCloud::_Listen()
{
	_listenThread = std::thread([this]() {
		listen_on_pipe("elgato_cloud", [this](std::string d) {
			obs_log(LOG_INFO, "Got Deeplink %s", d.c_str());
			if (d.find("elgatolink://auth") == 0) {
				if (mainWindowOpen && window) {
					QMetaObject::invokeMethod(
						QCoreApplication::instance()
							->thread(),
						[this]() {
							window->setLoading();
							window->show();
							window->raise();
							window->activateWindow();
						});
				}

				obs_log(LOG_INFO, "auth detected");
				std::unique_lock lock(m);
				if (!authorizing) {
					return;
				}
				size_t offset = d.find("&code=");
				if (offset == std::string::npos) {
					offset = d.find("?code=");
				}
				if (offset == std::string::npos) {
					return;
				}
				offset += 6;
				size_t end_offset = d.find("&", offset);

				std::string code;
				if (end_offset != std::string::npos) {
					code = d.substr(offset,
							end_offset - offset);
				} else {
					code = d.substr(offset);
				}

				std::string encodeddata;
				encodeddata += "grant_type=authorization_code";
				encodeddata += "&code=" + code;
				encodeddata +=
					"&redirect_uri=https%3A%2F%2Foauth2-redirect.elgato.com%2Felgatolink%2Fauth";
				encodeddata +=
					"&code_verifier=" + _last_code_verifier;
				encodeddata += "&client_id=elgatolink";

				auto api = MarketplaceApi::getInstance();
				std::string url = api->authUrl();

				url += "/auth/realms/mp/protocol/openid-connect/token?";
				url += encodeddata;

				auto response = fetch_string_from_post(
					url, encodeddata);

				auto responseJson = nlohmann::json::parse(response);

				if (responseJson.contains("error")) {
					if (mainWindowOpen && window) {
						QMetaObject::invokeMethod(
							QCoreApplication::instance()->thread(),
							[this]() {
								loginError = true;
								window->setLoggedIn();
							});
					}
				} else {
					loginError = false;
					_ProcessLogin(responseJson);
				}
				authorizing = false;
				return;
			}
		});
	});

	_listenThread.detach();
}

void ElgatoCloud::_Initialize()
{
	_config = get_module_config();
	bool makerTools = obs_data_get_bool(_config, "MakerTools");
	_makerToolsOnStart = makerTools;

	_GetSavedState();

	const auto now = std::chrono::system_clock::now();
	const auto epoch = now.time_since_epoch();
	const auto seconds =
		std::chrono::duration_cast<std::chrono::seconds>(epoch);
	if (_refreshToken == "" || _refreshTokenExpiration < seconds.count()) {
		loggedIn = false;
	} else {
		_TokenRefresh(false);
		LoadPurchasedProducts();
	}
}

void ElgatoCloud::StartLogin()
{
	std::unique_lock lock(m);
	union {
		uint32_t uint[8];
		uint8_t bytes[32];
	} data;
	_securerand.fillRange(data.uint);
	QByteArray code_verifier((const char *)data.bytes, sizeof(data.bytes));
	code_verifier = code_verifier.toBase64(QByteArray::Base64UrlEncoding |
					       QByteArray::OmitTrailingEquals);
	_last_code_verifier = code_verifier.toStdString();

	auto hash = QCryptographicHash::hash(code_verifier,
					     QCryptographicHash::Sha256);
	auto stringhash = hash.toBase64(QByteArray::Base64UrlEncoding |
					QByteArray::OmitTrailingEquals)
				  .toStdString();

	auto api = MarketplaceApi::getInstance();
	std::string url =
		api->authUrl() +
		"/auth/realms/mp/protocol/openid-connect/auth?response_type=code&client_id=elgatolink&redirect_uri=https%3A%2F%2Foauth2-redirect.elgato.com%2Felgatolink%2Fauth&code_challenge=" +
		stringhash + "&code_challenge_method=S256";

	authorizing = true;
	obs_log(LOG_INFO, "Starting login process...");
	obs_log(LOG_INFO, "Opening in browser:\n%s", url.c_str());
	ShellExecuteA(NULL, NULL, url.c_str(), NULL, NULL, SW_SHOW);
}

void ElgatoCloud::LogOut()
{
	_accessToken = "";
	_refreshToken = "";
	_accessTokenExpiration = 0;
	_refreshTokenExpiration = 0;
	_SaveState();
	loggedIn = false;
	if (mainWindowOpen && window) {
		window->setLoggedIn();
	}
}

void ElgatoCloud::LoadPurchasedProducts()
{
	if (!loggedIn) {
		return;
	}
	loading = true;
	// Todo- only refresh token if it needs refreshing
	_TokenRefresh(false);

	if (mainWindowOpen && window) {
		QMetaObject::invokeMethod(
			QCoreApplication::instance()->thread(),
			[this]() { window->setLoading(); });
	}

	auto api = MarketplaceApi::getInstance();
	std::string api_url = api->gatewayUrl();
	api_url +=
		"/my-products?extension=scene-collections&offset=0&limit=100";
	auto productsResponse = fetch_string_from_get(api_url, _accessToken);
	obs_log(LOG_INFO, "url: %s", api_url.c_str());
	obs_log(LOG_INFO, "Products: %s", productsResponse.c_str());
	products.clear();
	try {
		auto productsJson = nlohmann::json::parse(productsResponse);
		if (productsJson["results"].is_array()) {
			for (auto &pdat : productsJson["results"]) {
				//auto ep = new ElgatoProduct(pdat);
				products.emplace_back(
					std::make_unique<ElgatoProduct>(pdat));
			}
		}
		loading = false;
		if (mainWindowOpen && window) {
			QMetaObject::invokeMethod(
				QCoreApplication::instance()->thread(),
				[this]() {
					window->setLoggedIn();
					window->setupOwnedProducts();
				});
		}

	} catch (...) {
		loading = false;
		connectionError = true;
		if (mainWindowOpen && window) {
			QMetaObject::invokeMethod(
				QCoreApplication::instance()->thread(),
				[this]() { window->setLoggedIn(); });
		}
	}
}

nlohmann::json ElgatoCloud::GetPurchaseDownloadLink(std::string variantId)
{
	if (!loggedIn) {
		return nlohmann::json::parse("{\"Error\": \"Not Logged In\"}");
	}

	_TokenRefresh(false, false);

	auto api = MarketplaceApi::getInstance();
	std::string api_url = api->gatewayUrl();
	api_url += "/items/" + variantId + "/direct-link";
	auto response = fetch_string_from_get(api_url, _accessToken);
	// Todo- Error checking
	try {
		blog(LOG_INFO,
		     "============= DOWNLOAD REQUEST API RESPONSE =============\nURL: %s\nResponse: %s",
		     api_url.c_str(), response.c_str());
		auto responseJson = nlohmann::json::parse(response);
		return responseJson;
	} catch (...) {
		return nlohmann::json::parse(
			"{\"Error\": \"Invalid Response\"}");
	}
}

void ElgatoCloud::_ProcessLogin(nlohmann::json &loginData, bool loadData)
{
	try {
		connectionError = false;
		loginError = false;
		const auto now = std::chrono::system_clock::now();
		const auto epoch = now.time_since_epoch();
		const auto seconds =
			std::chrono::duration_cast<std::chrono::seconds>(epoch);
		const auto expiresIn =
			loginData.at("expires_in").template get<long long>();
		const auto refreshExpiresIn =
			loginData.at("refresh_expires_in")
				.template get<long long>();
		_accessToken =
			loginData.at("access_token").template get<std::string>();
		_refreshToken = loginData.at("refresh_token")
					.template get<std::string>();
		_accessTokenExpiration = expiresIn + seconds.count() - 10;
		_refreshTokenExpiration =
			refreshExpiresIn + seconds.count() - 10;

		_SaveState();

		obs_log(LOG_INFO, "Access and refresh token stored.");
		obs_log(LOG_INFO, "%s", _accessToken.c_str());

		loggedIn = true;
		loading = true;
	} catch (const nlohmann::json::out_of_range &e) {
		obs_log(LOG_INFO, "Bad Login, %i not found", e.id);
		connectionError = true;
		return;
	} catch (...) {
		obs_log(LOG_INFO, "Some other issue occurred");
		connectionError = true;
		return;
	}
	_LoadUserData(loadData);
}

void ElgatoCloud::_LoadUserData(bool loadData)
{
	try {
		auto api = MarketplaceApi::getInstance();
		std::string api_url = api->gatewayUrl();
		api_url += "/user";
		auto userResponse =
			fetch_string_from_get(api_url, _accessToken);
		auto userData = nlohmann::json::parse(userResponse);
		api->setUserDetails(userData);
		blog(LOG_INFO, "User Response:\n%s", userResponse.c_str());
		if (mainWindowOpen && window) {
			QMetaObject::invokeMethod(
				QCoreApplication::instance()->thread(),
				[this, loadData]() {
					if (loadData) {
						loading = true;
					}
					window->setLoggedIn();
					if (loadData) {
						LoadPurchasedProducts();
					}
				});
		}
	} catch (...) {
		obs_log(LOG_INFO, "Invalid response from server");
		connectionError = true;
	}
}

void ElgatoCloud::_SaveState()
{
	obs_data_set_string(_config, "AccessToken", _accessToken.c_str());
	obs_data_set_string(_config, "RefreshToken", _refreshToken.c_str());
	obs_data_set_int(_config, "AccessTokenExpiration",
			 _accessTokenExpiration);
	obs_data_set_int(_config, "RefreshTokenExpiration",
			 _refreshTokenExpiration);
	SaveConfig();
}

void ElgatoCloud::_GetSavedState()
{
	_accessToken = obs_data_get_string(_config, "AccessToken");
	_refreshToken = obs_data_get_string(_config, "RefreshToken");
	_accessTokenExpiration =
		obs_data_get_int(_config, "AccessTokenExpiration");
	_refreshTokenExpiration =
		obs_data_get_int(_config, "RefreshTokenExpiration");
}

} // namespace elgatocloud
