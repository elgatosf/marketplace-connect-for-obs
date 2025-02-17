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

#include "api.hpp"
#include "downloader.h"
#include <plugin-support.h>

#include <fstream>

#include <obs-module.h>
#include <util/platform.h>
#include <nlohmann/json.hpp>
#include <QDir>

using json = nlohmann::json;

namespace elgatocloud {

MarketplaceApi *MarketplaceApi::_api = nullptr;
std::mutex MarketplaceApi::_mtx;

MarketplaceApi::MarketplaceApi()
	: _gatewayUrl(DEFAULT_GATEWAY_URL),
	  _authUrl(DEFAULT_AUTH_URL),
	  _storeUrl(DEFAULT_STORE_URL),
	  _loggedIn(false),
	  _hasAvatar(false),
	  _avatarReady(false),
	  _avatarDownloading(false)
{
	std::string dataPath = obs_get_module_data_path(obs_current_module());
	dataPath += "/" + std::string(API_URLS_FILE);
	if (os_file_exists(dataPath.c_str())) {
		std::ifstream f(dataPath);
		try {
			json data = json::parse(f);
			_gatewayUrl = data.at("gateway_url");
			_authUrl = data.at("auth_url");
			_storeUrl = data.at("store_url");
			obs_log(LOG_INFO, "Url file loaded.");
		} catch (...) {
			obs_log(LOG_WARNING,
				"Urls file found, but could not be loaded.");
		}
	}
}

MarketplaceApi *MarketplaceApi::getInstance()
{
	if (_api == nullptr) {
		std::lock_guard<std::mutex> lock(_mtx);
		if (_api == nullptr) {
			_api = new MarketplaceApi();
		}
	}
	return _api;
}

void MarketplaceApi::setUserDetails(nlohmann::json &data)
{
	_hasAvatar = false;
	try {
		_firstName = data.at("first_name").template get<std::string>();
		_lastName = data.at("last_name").template get<std::string>();
		std::string color = data.at("default_avatar_color")
					    .template get<std::string>();
		_avatarColor = avatarColors.at(color);
		if (data.contains("avatar_resolutions")) {
			for (auto it : data["avatar_resolutions"]) {
				if (it.contains("resolution") && it["resolution"] == "180x180") {
					_avatarUrl = it["asset_cdn"];
					_hasAvatar = true;
					std::string avatarPath = QDir::homePath().toStdString();
					avatarPath += "/AppData/Local/Elgato/DeepLinking/Thumbnails";
					os_mkdirs(avatarPath.c_str());

					auto found = _avatarUrl.find_last_of("/");
					if (found == std::string::npos) {
						return;
					}
					auto filename = _avatarUrl.substr(found + 1);
					avatarPath = avatarPath + "/" + filename;
					std::lock_guard<std::mutex> lock(_mtx);
					if (!_avatarDownloading) {
						if (!os_file_exists(avatarPath.c_str())) {
							obs_log(LOG_INFO, "Downloading avatar %s", filename.c_str());
							_downloadAvatar();
						}
						else {
							obs_log(LOG_INFO, "Avatar Exists %s", filename.c_str());
							_avatarPath = avatarPath;
							_avatarReady = true;
						}
					}
				}
			}
		}
		_loggedIn = true;
	} catch (...) {
		obs_log(LOG_ERROR, "There was a problem processing the user response data.");
	}
}

void MarketplaceApi::_downloadAvatar()
{
	_avatarDownloading = true;
	_avatarReady = false;
	std::string savePath = QDir::homePath().toStdString();
	savePath += "/AppData/Local/Elgato/DeepLinking/Thumbnails/";
	std::shared_ptr<Downloader> dl = Downloader::getInstance("");
	dl->Enqueue(_avatarUrl, savePath, MarketplaceApi::AvatarProgress, MarketplaceApi::AvatarDownloadComplete,
		this);
}

void MarketplaceApi::AvatarDownloadComplete(std::string filename, void* data)
{
	obs_log(LOG_INFO, "Avatar Downloaded! %s", filename.c_str());
	auto api = static_cast<MarketplaceApi*>(data);
	api->_avatarReady = true;
	api->_avatarPath = filename;
	api->_avatarDownloading = false;
	emit api->AvatarDownloaded();
}

void MarketplaceApi::AvatarProgress(void* ptr, bool finished,
	bool downloading, uint64_t fileSize,
	uint64_t chunkSize, uint64_t downloaded)
{
	UNUSED_PARAMETER(ptr);
	//UNUSED_PARAMETER(finished);
	UNUSED_PARAMETER(downloading);
	UNUSED_PARAMETER(fileSize);
	UNUSED_PARAMETER(chunkSize);
	UNUSED_PARAMETER(downloaded);
	if (finished) {
		obs_log(LOG_INFO, "Download of avatar finished.");
	}
}

} // namespace elgatocloud
