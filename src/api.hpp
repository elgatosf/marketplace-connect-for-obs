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

#define DEFAULT_GATEWAY_URL "https://mp-gateway-dev.elgato.com"
#define DEFAULT_API_URL "https://mp-dev.elgato.com"
#define API_URLS_FILE "api-urls.json"

namespace elgatocloud {
class MarketplaceApi {
public:
	static MarketplaceApi *getInstance();
	inline std::string gatewayUrl() const { return _gatewayUrl; }
	inline std::string apiUrl() const { return _apiUrl; }

private:
	MarketplaceApi();
	MarketplaceApi(const MarketplaceApi &cpy) = delete;

	std::string _gatewayUrl;
	std::string _apiUrl;
	static MarketplaceApi *_api;
	static std::mutex _mtx;
};

} // namespace elgatocloud