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
#include <QVersionNumber>
#include <QMainWindow>

#include <plugin-support.h>
#include "elgato-cloud-window.hpp"
#include "elgato-cloud-data.hpp"
#include "elgato-update-modal.hpp"
#include "scene-collection-info.hpp"
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
	_openOnLaunch = false;
	_obsReady = false;
	_modulePtr = m;
	//_translate = t;
	_securerand = QRandomGenerator::securelySeeded();
	obs_frontend_add_event_callback(ElgatoCloud::FrontEndEventHandler, this);
	obs_frontend_add_save_callback(ElgatoCloud::FrontEndSaveLoadHandler, this);
	_Initialize();
	_Listen();
}

ElgatoCloud::~ElgatoCloud()
{
	obs_frontend_remove_event_callback(ElgatoCloud::FrontEndEventHandler, this);
	obs_frontend_remove_save_callback(ElgatoCloud::FrontEndSaveLoadHandler, this);
	obs_data_release(_config);
}

void ElgatoCloud::FrontEndEventHandler(enum obs_frontend_event event, void* data)
{
	auto ec = static_cast<ElgatoCloud*>(data);
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		ec->_obsReady = true;
		if (ec->_openOnLaunch) {
			ec->_openOnLaunch = false;
			QMetaObject::invokeMethod(
				QCoreApplication::instance()
				->thread(),
				[ec]() {
					OpenElgatoCloudWindow();
				});
		}
		break;
	}
}

void ElgatoCloud::FrontEndSaveLoadHandler(obs_data_t* save_data, bool saving, void* data)
{
	auto ec = static_cast<ElgatoCloud*>(data);
	if (!saving) { // We are loading
		auto settings = obs_data_get_obj(save_data, "elgato_marketplace_connect");
		if (!settings) { // Not an elgato mp installed scene collection
			ec->SetElgatoCollectionActive(false);
			return;
		}
		std::string jsonStr = obs_data_get_json(save_data);
		try {
			nlohmann::json modulesJson = nlohmann::json::parse(jsonStr);
			auto settingsJson = modulesJson["elgato_marketplace_connect"];
			ec->SetScData(settingsJson);
			ec->SetElgatoCollectionActive(true);
			bool firstRun = settingsJson.contains("first_run") && settingsJson["first_run"];
			bool hasSdActions =
				settingsJson.contains("stream_deck_actions") &&
				settingsJson["stream_deck_actions"].size() > 0;
			bool hasSdProfiles =
				settingsJson.contains("stream_deck_profiles") &&
				settingsJson["stream_deck_profiles"].size() > 0;
			bool hasThirdParty =
				settingsJson.contains("third_party") &&
				settingsJson["third_party"].size() > 0;
			bool shouldOpen = (hasSdActions || hasSdProfiles ||
					   hasThirdParty);
			if (shouldOpen) {
				const auto mainWindow = static_cast<QMainWindow*>(obs_frontend_get_main_window());
				SceneCollectionInfo* dialog = nullptr;
				dialog = new SceneCollectionInfo(settingsJson, mainWindow);
				dialog->setAttribute(Qt::WA_DeleteOnClose);
				dialog->show();
			}
		} catch (...) {
		
		}
		


		//bool firstRun = obs_data_get_bool(settings, "first_run");

		//auto  thirdParty = obs_data_get_array(settings, "third_party");
		//size_t tpSize = thirdParty ? obs_data_array_count(thirdParty) : 0;
		//if (firstRun && tpSize > 0) {
		//	std::vector<SceneCollectionLineItem> rows;
		//	for (size_t i = 0; i < tpSize; i++) {
		//		obs_data_t* item = obs_data_array_item(thirdParty, i);
		//		std::string name = obs_data_get_string(item, "name");
		//		std::string url = obs_data_get_string(item, "url");
		//		rows.push_back({ name, url });
		//		obs_data_release(item);
		//	}

		//	const auto mainWindow = static_cast<QMainWindow*>(obs_frontend_get_main_window());
		//	SceneCollectionInfo* dialog = nullptr;
		//	dialog = new SceneCollectionInfo(rows, mainWindow);
		//	dialog->setAttribute(Qt::WA_DeleteOnClose);
		//	dialog->show();
		//}
		obs_data_release(settings);
		//if(thirdParty)
		//	obs_data_array_release(thirdParty);
	} else {
		auto settings = obs_data_get_obj(save_data, "elgato_marketplace_connect");
		if (!settings) { // Not an elgato mp installed scene collection
			return;
		}
		obs_data_set_bool(settings, "first_run", false);
		obs_data_set_obj(save_data, "elgato_marketplace_connect", settings);
		obs_data_release(settings);
	}
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

std::string ElgatoCloud::GetAccessToken()
{
	if (!loggedIn) {
		return "";
	}
	_TokenRefresh(false, false);
	if (loggedIn) {
		return _accessToken;
	} else { // Our refresh token has expired, we're not really logged in.
		return "";
	}
}

std::string ElgatoCloud::GetRefreshToken()
{
	if (!loggedIn) {
		return "";
	}
	_TokenRefresh(false, false);
	if (loggedIn) {
		return _refreshToken;
	}
	else { // Our refresh token has expired, we're not really logged in.
		return "";
	}
}

void ElgatoCloud::_TokenRefresh(bool loadData, bool loadUserDetails)
{
	const auto now = std::chrono::system_clock::now();
	const auto epoch = now.time_since_epoch();
	const auto seconds =
		std::chrono::duration_cast<std::chrono::seconds>(epoch);
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
	obs_log(LOG_INFO, "Access Token has expired. Fetching a new token.");
	auto api = MarketplaceApi::getInstance();

	std::map<std::string, std::string> queryParams = {
		{GRANT_KEY, GRANT_REFRESH},
		{REFRESH_KEY, _refreshToken},
		{ID_KEY, ID}
	};

	std::string url = api->getAuthUrl(tokenEndpointSegments, queryParams);
	std::string encodeddata = queryString(queryParams);
	auto response = fetch_string_from_post(url, encodeddata);
	try {
		auto responseJson = nlohmann::json::parse(response);
		_ProcessLogin(responseJson, loadData);
	} catch (...) {
		obs_log(LOG_INFO, "There was a problem with the refresh token.  Try logging in again.");
		loggedIn = false;
		loading = false;
		authorizing = false;
		loginError = true;
		if (mainWindowOpen && window) {
			QMetaObject::invokeMethod(
				QCoreApplication::instance()->thread(),
				[this]() { window->setLoggedIn(); });
		}
	}
}

void ElgatoCloud::_Listen()
{
	_listenThread = std::thread([this]() {
		listen_on_pipe("elgato_cloud", [this](std::string d) {
			obs_log(LOG_INFO, "Pipe received: %s", d.c_str());
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

				std::map<std::string, std::string> queryParams = {
					{"grant_type", "authorization_code"},
					{"code", code},
					{REDIRECT_KEY, REDIRECT},
					{CODE_VERIFIER_KEY, _last_code_verifier},
					{ID_KEY, ID}
				};

				std::string encodeddata = queryString(queryParams);
				auto api = MarketplaceApi::getInstance();
				std::string url = api->getAuthUrl(tokenEndpointSegments, queryParams);

				auto response = fetch_string_from_post(
					url, encodeddata);

				auto responseJson =
					nlohmann::json::parse(response);

				if (responseJson.contains("error")) {
					if (mainWindowOpen && window) {
						QMetaObject::invokeMethod(
							QCoreApplication::instance()
								->thread(),
							[this]() {
								loginError =
									true;
								loggingIn = false;
								window->setLoggedIn();
							});
					}
				} else {
					loginError = false;
					loggingIn = false;
					_ProcessLogin(responseJson);
				}
				authorizing = false;
				return;
			}
			else if (d.find("elgatolink://open") == 0)
			{
				obs_log(LOG_INFO, "OPEN COMMAND RECEIEVED!");
				if (!_obsReady) {
					_openOnLaunch = true;
					return;
				}
				if (mainWindowOpen && window) {
					QMetaObject::invokeMethod(
						QCoreApplication::instance()
						->thread(),
						[this]() {
							//window->setLoading();
							window->show();
							window->raise();
							window->activateWindow();
							LoadPurchasedProducts();
						});
				} else {
					QMetaObject::invokeMethod(
						QCoreApplication::instance()
						->thread(),
						[this]() {
							OpenElgatoCloudWindow();
						});
				}
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
	loggingIn = true;
	if (mainWindowOpen && window) {
		window->setLoggedIn();
	}
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

	std::map<std::string, std::string> queryParams = {
		{RESPONSE_TYPE_KEY, RESPONSE_TYPE_CODE},
		{ID_KEY, ID},
		{REDIRECT_KEY, REDIRECT},
		{CHALLENGE_KEY, stringhash},
		{CC_KEY, CC_METHOD}
	};

	std::string url = api->getAuthUrl(authEndpointSegments, queryParams);

	authorizing = true;
	ShellExecuteA(NULL, NULL, url.c_str(), NULL, NULL, SW_SHOW);
}

void ElgatoCloud::LogOut()
{
	auto api = MarketplaceApi::getInstance();
	api->logOut();

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

void ElgatoCloud::CheckUpdates(bool forceCheck)
{
	try {
		std::string updateUrl =
			"https://gc-updates.elgato.com/windows/marketplace-plugin-for-obs/final/app-version-check.json.php";
		auto response = fetch_string_from_get(updateUrl, "");
		auto responseJson = nlohmann::json::parse(response);
		if (responseJson.contains("Automatic")) {
			auto details = responseJson["Automatic"];
			std::string version = details["Version"];
			std::string downloadUrl = details["downloadURL"];
			auto updateVersion =
				QVersionNumber::fromString(version);
			auto currentVersion =
				QVersionNumber::fromString(PLUGIN_VERSION);
			std::string skip = _skipUpdate == "" ? "0.0.0"
							     : _skipUpdate;
			auto skipVersion = QVersionNumber::fromString(skip);
			if ((forceCheck || skipVersion != updateVersion) &&
			    updateVersion > currentVersion) {
				// Reset the "skip this update" flag because we now have a
				// new update.
				_skipUpdate = !forceCheck ? "" : _skipUpdate;
				openUpdateModal(version, downloadUrl);
			}
		} else {
			throw("Error");
		}
	} catch (...) {
		blog(LOG_INFO, "Unable to contact update server.");
	}
}

void ElgatoCloud::SetSkipVersion(std::string version)
{
	_skipUpdate = version;
	_SaveState();
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
	//std::string api_url = api->gatewayUrl();
	//api_url +=
	//	"/my-products?extension=scene-collections&offset=0&limit=100";
	std::vector<std::string> segments = { "my-products" };
	std::map<std::string, std::string> queryParams = {
		{"extension", "scene-collections"},
		{"offset", "0"},
		{"limit", "100"}
	};

	std::string api_url = api->getGatewayUrl(segments, queryParams);

	auto productsResponse = fetch_string_from_get(api_url, _accessToken);
	products.clear();
	try {
		auto productsJson = nlohmann::json::parse(productsResponse);
		if (productsJson["results"].is_array()) {
			for (auto &pdat : productsJson["results"]) {
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
		loginError = true;
	}
}

void ElgatoCloud::_SaveState()
{
	std::string accessTokenEncrypted = "";
	std::string refreshTokenEncrypted = "";
	if (_accessToken != "") {
		accessTokenEncrypted = encryptString(_accessToken);
	}
	if (_refreshToken != "") {
		refreshTokenEncrypted = encryptString(_refreshToken);
	}

	obs_data_set_string(_config, "AccessToken", accessTokenEncrypted.c_str());
	obs_data_set_string(_config, "RefreshToken", refreshTokenEncrypted.c_str());
	obs_data_set_int(_config, "AccessTokenExpiration",
			 _accessTokenExpiration);
	obs_data_set_int(_config, "RefreshTokenExpiration",
			 _refreshTokenExpiration);
	obs_data_set_string(_config, "SkipUpdate", _skipUpdate.c_str());
	SaveConfig();
}

void ElgatoCloud::_GetSavedState()
{
	std::string accessTokenEncrypted = obs_data_get_string(_config, "AccessToken");
	std::string refreshTokenEncrypted = obs_data_get_string(_config, "RefreshToken");
	if (accessTokenEncrypted.size() > 25) {
		_accessToken = decryptString(accessTokenEncrypted);
		_accessToken = _accessToken.substr(0, _accessToken.length() - 1);
	} else {
		_accessToken = "";
	}

	if (refreshTokenEncrypted.size() > 25) {
		_refreshToken = decryptString(refreshTokenEncrypted);
		_refreshToken = _refreshToken.substr(0, _refreshToken.length() - 1);
	} else {
		_refreshToken = "";
	}

	_accessTokenExpiration =
		obs_data_get_int(_config, "AccessTokenExpiration");
	_refreshTokenExpiration =
		obs_data_get_int(_config, "RefreshTokenExpiration");
	_skipUpdate = obs_data_get_string(_config, "SkipUpdate");
}

} // namespace elgatocloud
