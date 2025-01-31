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

#include <obs-module.h>

#include <mutex>
#include <thread>
#include <memory>
#include <QThread>
#include <QRandomGenerator>
#include <string>
#include <nlohmann/json.hpp>

#include "elgato-product.hpp"

namespace elgatocloud {
class ElgatoCloudThread;

class ElgatoCloud;
extern ElgatoCloud *elgatoCloud;

class ElgatoCloudWindow;

ElgatoCloud *GetElgatoCloud();

class ElgatoCloud {
public:
	ElgatoCloudThread *th = nullptr;
	std::mutex m;
	std::unique_lock<std::mutex> *mainLoopLock = nullptr;
	std::vector<std::unique_ptr<ElgatoProduct>> products;

	ElgatoCloud(obs_module_t *m);
	~ElgatoCloud();
	void StartLogin();
	void LogOut();
	void LoadPurchasedProducts();
	obs_data_t *GetConfig();
	void SaveConfig();
	nlohmann::json GetPurchaseDownloadLink(std::string variantId);

	obs_module_t *GetModule();
	bool loggedIn = false;
	bool loading = false;
	bool authorizing = false;
	bool connectionError = false;

	void Thread();
	bool mainWindowOpen = false;
	ElgatoCloudWindow *window = nullptr;

private:
	void _Initialize();
	void _Listen();
	void _ProcessLogin(nlohmann::json &loginData, bool loadData = true);
	void _SaveState();
	void _GetSavedState();
	void _TokenRefresh(bool loadData, bool loadUserDetails = true);
	void _LoadUserData(bool loadData = false);

	obs_module_t *_modulePtr = nullptr;
	//translateFunc _translate = nullptr;
	QRandomGenerator _securerand;
	std::string _last_code_verifier;
	std::thread _listenThread;

	std::string _accessToken;
	std::string _refreshToken;
	int64_t _accessTokenExpiration;
	int64_t _refreshTokenExpiration;
	obs_data_t *_config;
};

class ElgatoCloudThread : public QThread {
public:
	explicit ElgatoCloudThread() {};
	void run() { elgatoCloud->Thread(); }
};
} // namespace elgatocloud
