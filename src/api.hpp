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

#pragma once
#include <string>
#include <mutex>
#include <nlohmann/json.hpp>

#define DEFAULT_GATEWAY_URL "https://mp-gateway.elgato.com"
#define DEFAULT_STORE_URL "https://marketplace.elgato.com"
#define DEFAULT_AUTH_URL "https://account.elgato.com"
#define API_URLS_FILE "api-urls.json"

namespace elgatocloud {

const std::map<std::string, std::string> avatarColors = {
	{"orange", "#BE5900"}, {"magenta", "#C93BA1"}, {"green", "#2A863E"},
	{"teal", "#22837D"},   {"cyan", "#0F7EAD"},    {"purple", "#A638FE"},
	{"gray", "#767676"}};

class MarketplaceApi {
public:
	static MarketplaceApi *getInstance();
	inline std::string gatewayUrl() const { return _gatewayUrl; }
	inline std::string storeUrl() const { return _storeUrl; }
	inline std::string authUrl() const { return _authUrl; }
	inline std::string firstName() const { return _firstName; }
	inline std::string lastName() const { return _lastName; }
	inline std::string avatarColor() const { return _avatarColor; }

	void setUserDetails(nlohmann::json &data);

private:
	MarketplaceApi();
	MarketplaceApi(const MarketplaceApi &cpy) = delete;

	std::string _gatewayUrl;
	std::string _storeUrl;
	std::string _authUrl;
	bool _loggedIn;
	std::string _firstName;
	std::string _lastName;
	std::string _avatarColor;

	static MarketplaceApi *_api;
	static std::mutex _mtx;
};

} // namespace elgatocloud
