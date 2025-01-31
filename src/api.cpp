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
#include "plugin-support.h"

#include <fstream>

#include <obs-module.h>
#include <util/platform.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace elgatocloud {

MarketplaceApi *MarketplaceApi::_api = nullptr;
std::mutex MarketplaceApi::_mtx;

MarketplaceApi::MarketplaceApi()
	: _gatewayUrl(DEFAULT_GATEWAY_URL),
	  _apiUrl(DEFAULT_API_URL),
	  _storeUrl(DEFAULT_STORE_URL)
{
	std::string dataPath = obs_get_module_data_path(obs_current_module());
	dataPath += "/" + std::string(API_URLS_FILE);
	if (os_file_exists(dataPath.c_str())) {
		std::ifstream f(dataPath);
		try {
			json data = json::parse(f);
			_gatewayUrl = data.at("gateway_url");
			_apiUrl = data.at("api_url");
			_storeUrl = data.at("store_url");
			obs_log(LOG_INFO, "Url file loaded.");
		} catch (...) {
			obs_log(LOG_WARNING,
				"Urls file found, but could not be loaded.");
		}
	} else {
		obs_log(LOG_INFO, "No url file found.");
	}
	obs_log(LOG_INFO, "Gateway Url: %s", _gatewayUrl.c_str());
	obs_log(LOG_INFO, "API Url: %s", _apiUrl.c_str());
	obs_log(LOG_INFO, "Store Url: %s", _storeUrl.c_str());
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

void MarketplaceApi::setUserDetails(nlohmann::json& data)
{
	try {
		_firstName = data.at("first_name").template get<std::string>();
		_lastName = data.at("last_name").template get<std::string>();
		std::string color = data.at("default_avatar_color").template get<std::string>();
		_avatarColor = avatarColors.at(color);
		_loggedIn = true;
	}
	catch (...) {

	}
}

} // namespace elgatocloud
