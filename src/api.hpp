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
#include <QObject>

#define DEFAULT_GATEWAY_URL "https://mp-gateway.elgato.com"
#define DEFAULT_STORE_URL "https://marketplace.elgato.com"
#define DEFAULT_AUTH_URL "https://account.elgato.com"
#define API_URLS_FILE "api-urls.json"

#define USERAGENT "elgatolink"
#define ID_KEY "client_id"
#define ID "elgatolink"
#define CC_KEY "code_challenge_method"
#define CC_METHOD "S256"
#define REDIRECT_KEY "redirect_uri"
#define REDIRECT "https://oauth2-redirect.elgato.com/" ID "/auth"
#define RESPONSE_TYPE_KEY "response_type"
#define RESPONSE_TYPE_CODE "code"
#define REFRESH_KEY "refresh_token"
#define GRANT_KEY "grant_type"
#define GRANT_REFRESH "refresh_token"

#define CHALLENGE_KEY "code_challenge"
#define CODE_VERIFIER_KEY "code_verifier"

namespace elgatocloud {

const std::map<std::string, std::string> avatarColors = {
	{"orange", "#BE5900"}, {"magenta", "#C93BA1"}, {"green", "#2A863E"},
	{"teal", "#22837D"},   {"cyan", "#0F7EAD"},    {"purple", "#A638FE"},
	{"gray", "#767676"}};

const std::vector<std::string> authEndpointSegments = {
	"auth", "realms", "mp", "protocol", "openid-connect", "auth"
};

const std::vector<std::string> tokenEndpointSegments = {
	"auth", "realms", "mp", "protocol", "openid-connect", "token"
};

const std::vector<std::string> logoutEndpointSegments = {
	"auth", "realms", "mp", "protocol", "openid-connect", "logout"
};


class MarketplaceApi : public QObject {
	Q_OBJECT
public:
	static MarketplaceApi *getInstance();
	inline std::string gatewayUrl() const { return _gatewayUrl; }
	inline std::string storeUrl() const { return _storeUrl; }
	inline std::string authUrl() const { return _authUrl; }
	inline std::string firstName() const { return _firstName; }
	inline std::string lastName() const { return _lastName; }
	inline std::string id() const { return _id; }
	inline std::string avatarColor() const { return _avatarColor; }
	inline bool hasAvatar() const { return _hasAvatar; }
	inline std::string avatarUrl() const { return _avatarUrl; }
	inline bool avatarReady() const { return _avatarReady; }
	inline std::string avatarPath() const { return _avatarPath; }
	static void AvatarProgress(void* ptr, bool finished,
		bool downloading, uint64_t fileSize,
		uint64_t chunkSize, uint64_t downloaded);
	static void AvatarDownloadComplete(std::string filename, void* data);
	void setUserDetails(nlohmann::json &data);
	void logOut();
	void OpenStoreInBrowser() const;
	std::string getAuthUrl(std::vector<std::string> const& segments, std::map<std::string, std::string> const& queryParams);
	std::string getGatewayUrl(
		std::vector<std::string> const& segments,
		std::map<std::string, std::string> const& queryParams
	);

signals:
	void AvatarDownloaded();

private:
	MarketplaceApi();
	MarketplaceApi(const MarketplaceApi &cpy) = delete;

	void _downloadAvatar();

	std::string _gatewayUrl;
	std::string _storeUrl;
	std::string _authUrl;
	bool _loggedIn;
	bool _hasAvatar;
	bool _avatarReady;
	bool _avatarDownloading;
	std::string _firstName;
	std::string _lastName;
	std::string _avatarColor;
	std::string _avatarUrl;
	std::string _avatarPath;
	std::string _id;

	static MarketplaceApi *_api;
	static std::mutex _mtx;
};

} // namespace elgatocloud
