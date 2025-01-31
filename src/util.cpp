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

#include <QDir>
#include <obs-module.h>
#include <util/config-file.h>
#include <util/platform.h>
#include "obs-frontend-api.h"
#include <util/platform.h>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>
#include <stdio.h>

#include <plugin-support.h>

#include <functional>

#include <thread>
#include <QMessageBox>
#include <QEventLoop>
#include <QFileSystemWatcher>
#include <curl/curl.h>

#include "platform.h"
#include "util.h"

obs_data_t *get_module_config()
{
	const auto confPath = obs_module_config_path("config.json");
	obs_data_t *config =
		obs_data_create_from_json_file_safe(confPath, "bak");
	bfree(confPath);
	if (!config) {
		config = obs_data_create();
		blog(LOG_WARNING, "Configuration file not found");
	} else {
		blog(LOG_INFO, "Loaded configuration file");
	}

	obs_data_set_default_string(config, "AccessToken", "");
	obs_data_set_default_string(config, "RefreshToken", "");
	obs_data_set_default_int(config, "AccessTokenExpiration", 0);
	obs_data_set_default_int(config, "RefreshTokenExpiration", 0);

	std::string path = QDir::homePath().toStdString();
	path += "/AppData/Local/Elgato/DeepLinking/SceneCollections";
	os_mkdirs(path.c_str());
	obs_data_set_default_string(config, "InstallLocation", path.c_str());
	obs_data_set_default_bool(config, "MakerTools", false);

	obs_data_set_default_string(config, "DefaultAudioCaptureSettings", "");
	obs_data_set_default_string(config, "DefaultVideoCaptureSettings", "");

	return config;
}

void save_module_config(obs_data_t *config)
{
	char *configPath = obs_module_config_path("config.json");
	if (!configPath)
		return;
	std::string path = configPath;
	bfree(configPath);

	size_t pos = path.rfind('/');
	if (pos == std::string::npos) {
		blog(LOG_ERROR, "Settings NOT saved.");
		return;
	}
	std::string configDir = path.substr(0, pos);

	os_mkdirs(configDir.c_str());
	if (obs_data_save_json_safe(config, path.c_str(), "tmp", "bak")) {
		blog(LOG_INFO, "Settings saved");
	} else {
		blog(LOG_ERROR, "Settings NOT saved.");
	}
}

int get_major_version()
{
	std::string v = obs_get_version_string();
	size_t pos = v.find('.');
	if (pos != std::string::npos) {
		std::string major_version = v.substr(0, pos);
		return std::stoi(major_version);
	}
	return -1;
}

bool filename_json(std::string &filename)
{
	const std::string suffix = ".json";
	if (filename.size() < suffix.size()) {
		return false;
	}
	return filename.compare(filename.size() - suffix.size(), suffix.size(),
				suffix) == 0;
}

std::string get_current_scene_collection_filename()
{
	int v = get_major_version();
	std::string filename;

	// TODO: Convert this to use GetUserConfig() from obs-utils.cpp
	if (v < 31) { // Get the filename from global.ini
		// also in pre-31, the filename in the config file did not
		// have a filetype suffix.
		// so we need to add .json
#pragma warning(disable : 4996)
		filename = config_get_string(obs_frontend_get_global_config(),
					     "Basic", "SceneCollectionFile");
#pragma warning(default : 4996)
	} else { // get the filename from user.ini
		// in 31+ the filename stored in user.ini *does* have a filetype
		// suffix.
		void *obs_frontend_dll = os_dlopen("obs-frontend-api.dll");
		void *sym = os_dlsym(obs_frontend_dll,
				     "obs_frontend_get_user_config");
		config_t *(*get_user_config)() = (config_t * (*)()) sym;
		config_t *user_config = get_user_config();
		os_dlclose(obs_frontend_dll);
		filename = config_get_string(user_config, "Basic",
					     "SceneCollectionFile");
	}
	// OBS is inconsistent with adding .json to SceneCollectionFile value.
	if (!filename_json(filename)) {
		filename += ".json";
	}
	blog(LOG_INFO, "COLLECTION FILENAME: %s", filename.c_str());
	return filename;
}

std::string fetch_string_from_get(std::string url, std::string token)
{
	std::string result;
	CURL *curl_instance = curl_easy_init();
	curl_easy_setopt(curl_instance, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl_instance, CURLOPT_WRITEFUNCTION,
			 write_data<std::string>);
	curl_easy_setopt(curl_instance, CURLOPT_WRITEDATA,
			 static_cast<void *>(&result));
	curl_easy_setopt(curl_instance, CURLOPT_USERAGENT, "elgato-cloud 0.0");
	curl_easy_setopt(curl_instance, CURLOPT_XOAUTH2_BEARER, token.c_str());
	curl_easy_setopt(curl_instance, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
	curl_easy_setopt(curl_instance, CURLOPT_CONNECTTIMEOUT, 3);
	curl_easy_setopt(curl_instance, CURLOPT_TIMEOUT, 5);
	CURLcode res = curl_easy_perform(curl_instance);

	curl_easy_cleanup(curl_instance);
	if (res == CURLE_OK) {
		return result;
	} else if (res == CURLE_OPERATION_TIMEDOUT) {
		return "{\"error\": \"Connection Timed Out\"}";
	}
	return "{\"error\": \"Unspecified Error\"}";
}

std::string fetch_string_from_post(std::string url, std::string postdata)
{
	std::string result;
	CURL *curl_instance = curl_easy_init();
	curl_easy_setopt(curl_instance, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl_instance, CURLOPT_WRITEFUNCTION,
			 write_data<std::string>);
	curl_easy_setopt(curl_instance, CURLOPT_WRITEDATA,
			 static_cast<void *>(&result));
	curl_easy_setopt(curl_instance, CURLOPT_POSTFIELDS, postdata.c_str());
	curl_easy_setopt(curl_instance, CURLOPT_USERAGENT, "elgato-cloud 0.0");
	CURLcode res = curl_easy_perform(curl_instance);

	curl_easy_cleanup(curl_instance);
	if (res == CURLE_OK) {
		return result;
	}
	return "";
}

std::vector<char> fetch_bytes_from_url(std::string url)
{
	std::vector<char> result;
	CURL *curl_instance = curl_easy_init();
	curl_easy_setopt(curl_instance, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl_instance, CURLOPT_WRITEFUNCTION,
			 write_data<std::vector<char>>);
	curl_easy_setopt(curl_instance, CURLOPT_WRITEDATA,
			 static_cast<void *>(&result));
	curl_easy_setopt(curl_instance, CURLOPT_USERAGENT, "elgato-cloud 0.0");
	CURLcode res = curl_easy_perform(curl_instance);

	curl_easy_cleanup(curl_instance);
	if (res == CURLE_OK) {
		return result;
	}
	return std::vector<char>();
}

// Replaces all instances of needle in haystack with word.
void replace_all(std::string &haystack, std::string needle, std::string word)
{
	size_t p = haystack.find(needle);

	while (p != std::string::npos) {
		haystack.replace(p, needle.size(), word);
		p = haystack.find(needle, p + word.size());
	}
}

// Monitors a directory for changes and calls `callback` with `directory` any time there is.
void monitor_for_files(std::string directory,
		       std::function<void(std::string)> callback)
{
	// Waits for the UI thread to become responsive, then waits an additional 1/10th of a second, then waits for the UI thread one more time.
	// This ensures that the UI is actively processing events before we start loading scene collections.
	obs_queue_task(
		OBS_TASK_UI,
		[](void *) {
			// Do nothing
		},
		NULL, true);

	os_sleep_ms(100);

	obs_queue_task(
		OBS_TASK_UI,
		[](void *) {
			// Do nothing
		},
		NULL, true);

	// Force a check on launch, just in case anything was added while we weren't monitoring the directory
	callback(directory);

	QEventLoop loop;

	obs_log(LOG_INFO, "Watching %s", directory.c_str());
	QWidget *window = (QWidget *)obs_frontend_get_main_window();
	QFileSystemWatcher watcher(window);
	watcher.addPath(directory.c_str());
	QObject::connect(&watcher, &QFileSystemWatcher::directoryChanged,
			 [directory, callback]() { callback(directory); });

	loop.exec();
}

// Call monitor_for_files in a detached thread
void monitor_for_files_thread(const std::string &directory,
			      std::function<void(std::string)> callback)
{
	struct params_t {
		std::string directory;
		std::function<void(std::string)> callback;
	};
	auto params = new params_t{directory, callback};
	auto fn = [](params_t *params) {
		monitor_for_files(params->directory, params->callback);

		delete params;
	};

	std::thread t(fn, params);
	t.detach();
}

// Recursively deletes a directory, ignoring symlinks/soft links
void clear_dir(std::string path)
{
	auto dir = os_opendir(path.c_str());
	if (!dir) {
		return;
	}

	auto ent = os_readdir(dir);

	while (ent) {
		std::string ent_name = ent->d_name;
		if (ent_name == ".." || ent_name == ".") {
			ent = os_readdir(dir);
			continue;
		}

		std::string target = path + "/" + ent->d_name;
		if (ent->directory) {
			if (!is_symlink(target)) {
				clear_dir(target);
				os_rmdir(target.c_str());
			} else {
				obs_log(LOG_ERROR,
					"Refusing to delete symlink directory: %s",
					target.c_str());
			}
		} else {
			os_unlink(target.c_str());
		}
		ent = os_readdir(dir);
	}

	os_closedir(dir);
}

// Converts OBS data object to a JSON object
nlohmann::json data_to_json(obs_data_t *data)
{
	auto result = nlohmann::json::object();
	if (!data) {
		return result;
	}

	return nlohmann::json::parse(obs_data_get_json(data));
}

// Converts an OBS data array to a JSON array
nlohmann::json data_to_json(obs_data_array_t *data)
{
	auto result = nlohmann::json::array();
	if (!data) {
		return result;
	}

	size_t count = obs_data_array_count(data);

	for (size_t i = 0; i < count; ++i) {
		auto d = obs_data_array_item(data, i);
		result.push_back(data_to_json(d));
		obs_data_release(d);
	}
	return result;
}

// Converts a JSON array to an OBS data array
obs_data_array_t *data_array_from_json(const nlohmann::json &j)
{
	if (!j.is_array()) {
		return nullptr;
	}
	auto res = obs_data_array_create();
	for (const auto &item : j) {
		std::string s = item.dump();
		obs_data_t *d = obs_data_create_from_json(s.c_str());
		obs_data_array_push_back(res, d);
		obs_data_release(d);
	}
	return res;
}

// Converts a JSON object to an OBS data object
obs_data_t *data_from_json(const nlohmann::json &j)
{
	if (!j.is_object()) {
		return obs_data_create();
	}
	std::string dumped = j.dump();
	auto res = obs_data_create_from_json(dumped.c_str());
	return res;
}

// Ensures that only ASCII characters get sent through toupper
char toupper_ascii(char c)
{
	if (c > 0 && c <= 127) {
		return (char)toupper(c);
	}
	return c;
}

#include <random>

std::mt19937 rand_gen;

std::string gen_uuid()
{
	union {
		uint8_t bytes[16];
		uint32_t numbers[4];
	} uuid;
	uuid.numbers[0] = rand_gen();
	uuid.numbers[1] = rand_gen();
	uuid.numbers[2] = rand_gen();
	uuid.numbers[3] = rand_gen();
	// v4 specifics
	uuid.bytes[6] &= 0xF;
	uuid.bytes[6] |= 0x40;
	uuid.bytes[8] &= 0x3F;
	uuid.bytes[8] |= 0x80;

	//4 2 2 2 6
	auto high = [](uint8_t byte) {
		return "0123456789ABCDEF"[byte >> 4];
	};
	auto low = [](uint8_t byte) {
		return "0123456789ABCDEF"[byte & 0xF];
	};
	std::string uuid_str;
	uuid_str.resize(32);
	for (auto i = 0; i < 16; ++i) {
		uuid_str[i * 2] = high(uuid.bytes[i]);
		uuid_str[i * 2 + 1] = low(uuid.bytes[i]);
	}
	uuid_str.insert(8, 1, '-');
	uuid_str.insert(13, 1, '-');
	uuid_str.insert(18, 1, '-');
	uuid_str.insert(23, 1, '-');

	return uuid_str;
}

static const char random_options[] = "0123456789abcdefghijklmnopqrstuvwxyz";
std::string random_name(size_t length)
{
	std::string result;
	result.reserve(length);
	while (length-- > 0) {
		result += random_options[rand_gen() %
					 (sizeof(random_options) - 1)];
	}
	return result;
}

bool generate_safe_path(std::string unsafe, std::string &safe)
{
	const size_t base_length = unsafe.length();
	size_t length = os_utf8_to_wcs(unsafe.c_str(), base_length, nullptr, 0);
	std::wstring wfile;

	if (!length) {
		return false;
	}

	wfile.resize(length);
	os_utf8_to_wcs(unsafe.c_str(), base_length, &wfile[0], length + 1);

	for (size_t i = wfile.size(); i > 0; i--) {
		size_t prev = i - 1;
		if (iswspace(wfile[prev])) {
			wfile[prev] = '_';
		} else if (wfile[prev] != '_' && !iswalnum(wfile[prev])) {
			wfile.erase(prev, 1);
		}
	}

	if (wfile.size() == 0) {
		wfile = L"chars_only";
	}

	length = os_wcs_to_utf8(wfile.c_str(), wfile.size(), nullptr, 0);
	if (!length)
		return false;
	safe.resize(length);
	os_wcs_to_utf8(wfile.c_str(), wfile.size(), &safe[0], length + 1);
	return true;
}

std::string versionNoBuild() {
	std::string fullVersion = PLUGIN_VERSION;
	size_t lastPos = fullVersion.find_last_of('.');
	if (lastPos == std::string::npos) {
		return fullVersion;
	}
	return fullVersion.substr(0, lastPos);
}

std::string buildNumber()
{
	std::string fullVersion = PLUGIN_VERSION;
	size_t lastPos = fullVersion.find_last_of('.');
	if (lastPos == std::string::npos) {
		return fullVersion;
	}
	return fullVersion.substr(lastPos + 1);
}

std::string releaseType()
{
	std::string rt = PLUGIN_RELEASE_TYPE;
	for (auto& c : rt) // convert to lowercase
	{
		c = tolower(c);
	}
	return rt == "release" ? "" : " " + rt;
}