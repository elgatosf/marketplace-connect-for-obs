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

#include <string>
#include <vector>
#include <obs-module.h>
#include "obs-frontend-api.h"
#include "plugin-support.h"
#include <util/platform.h>
#include <nlohmann/json.hpp>

#ifdef WIN32
#include <shlobj_core.h>
#include <direct.h>
#include <windows.h>
#include <io.h>
#endif

#include "util.h"

std::string get_scene_collections_path()
{
	// Get path to current module (elgatocloud plugin)
	char *path = obs_module_config_path("");
	std::string path_str = std::string(path);
	bfree(path);

	// Remove path from /plugin_config on, and add /basic/scenes/
	std::string resolved_path =
		path_str.substr(0, path_str.rfind("/plugin_config")) +
		"/basic/scenes/";

	return resolved_path;
}

#ifdef WIN32
#ifdef UNICODE
typedef std::wstring TString;
/*static TString stringToTString(const std::string &str)
{
	TString result;
	result.resize(str.size());
	auto len = os_utf8_to_wcs(str.c_str(), str.size(), &result[0],
				  result.size());
	result.resize(len);
	return result;
}
 static std::string TStringToString(const TString &str)
{
	std::string result;
	result.resize(str.size() * 2);
	auto len = os_wcs_to_utf8(str.c_str(), str.size(), &result[0],
				  result.size());
	result.resize(len);
	return result;
}*/
static TString makeLongPath(const std::string &str)
{
	TString result = L"\\\\?\\";
	result.resize(str.size() + 4);
	auto len = os_utf8_to_wcs(str.c_str(), str.size(), &result[4],
				  result.size() - 4);
	result.resize(len + 4);
	return result;
}

#else
typedef std::string TString;
static TString stringToTString(const std::string &str)
{
	return str;
}
static std::string TStringToString(const TString &str)
{
	return str;
}
static TString makeLongPath(const std::string &str)
{
	return "\\\\?\\" + str;
}
#endif

#endif

bool is_symlink(std::string path)
{
#ifdef WIN32
	// TODO: Use lstat on Mac
	TString tpath = makeLongPath(path);
	auto attributes = GetFileAttributes(tpath.c_str());
	return attributes != INVALID_FILE_ATTRIBUTES &&
	       0 != (attributes & FILE_ATTRIBUTE_REPARSE_POINT);
#endif
}

bool is_directory(std::string path)
{
#ifdef WIN32
	// TODO: Use lstat on Mac
	TString tpath = makeLongPath(path);

	auto attributes = GetFileAttributes(tpath.c_str());
	return attributes != INVALID_FILE_ATTRIBUTES &&
	       0 != (attributes & FILE_ATTRIBUTE_DIRECTORY);
#endif
}

std::string get_plugin_data_path()
{
#ifdef WIN32
	TCHAR tpath[MAX_PATH];
	std::string appdata_path = "";
	if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0,
				      tpath))) {
#ifdef UNICODE
		char utf8path[MAX_PATH * 4];
		os_wcs_to_utf8(tpath, lstrlenW(tpath), utf8path,
			       sizeof(utf8path));
		appdata_path = utf8path;
#else
		appdata_path = tpath;
#endif
		appdata_path += "/obs-studio/plugins";
	}

	return appdata_path;
#endif
}

// Case insensitive path prefix matching on Windows. Simple char-wise compare on other platforms.
// TODO: Do users expect Mac to be case insensitive?
bool path_begins_with(const std::string &haystack, const std::string &needle)
{
#ifdef WIN32
	for (size_t i = 0; i < haystack.size() && i < needle.size(); ++i) {
		char a, b;
		a = haystack[i];
		b = needle[i];
		// TODO: Make unicode aware?
		a = toupper_ascii(a);
		b = toupper_ascii(b);
		if (a == '/' || a == '\\') {
			a = '/';
		}
		if (b == '/' || b == '\\') {
			b = '/';
		}
		if (a != b) {
			return false;
		}
	}
	return true;
#else
	return haystack.find(needle) == 0;
#endif
}

// Just blindly listens on a named pipe waiting for a string, and submits it to the callback
bool listen_on_pipe(const std::string &pipe_name,
		    std::function<void(std::string)> callback)
{
	int pipe_number = 0;
	std::string base_name = "\\\\.\\pipe\\" + pipe_name;
	std::string attempt_name;
	while (true) {

		obs_log(LOG_INFO, "Creating pipe...");
		HANDLE pipe = INVALID_HANDLE_VALUE;

		while (pipe_number < 10) {
			attempt_name = base_name + std::to_string(pipe_number);
			pipe = CreateNamedPipeA(
				attempt_name.c_str(), PIPE_ACCESS_INBOUND,
				PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE |
					PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
				1, 1024, 1024, 0, NULL);
			if (pipe != INVALID_HANDLE_VALUE) {
				break;
			}
			pipe_number++;
		}
		if (pipe == INVALID_HANDLE_VALUE) {
			obs_log(LOG_ERROR, "Could not open named pipe!");
			return false;
		}

		Sleep(100);
		obs_log(LOG_INFO, "Connecting...");
		bool connected = ConnectNamedPipe(pipe, NULL);
		if (!connected && GetLastError() == ERROR_PIPE_CONNECTED) {
			connected = true;
		}

		std::string buffer;

		if (connected) {
			obs_log(LOG_INFO, "Connected");
			while (true) {
				buffer.resize(2048);
				DWORD read_count = 0;
				obs_log(LOG_INFO, "Reading");
				auto status = ReadFile(pipe, &buffer[0],
						       (DWORD)buffer.size(),
						       &read_count, NULL);
				if (!status || read_count == 0) {
					obs_log(LOG_INFO, "Failed to read");
					break;
				}
				buffer.resize(read_count);
				callback(buffer);
			}
		}
		CloseHandle(pipe);
		obs_log(LOG_INFO, "Restarting");
	}

	obs_log(LOG_INFO, "Ended");
}

FILE *open_tmp_file(const char *mode, std::string &outFilename)
{
	std::string fname = "elgatocloud-" + random_name();
#ifdef WIN32
	TCHAR tpath[MAX_PATH + 1];
	size_t len = GetTempPath(MAX_PATH, tpath);
	size_t totalLen = len;
	auto remaining = MAX_PATH - len;
	HANDLE file = INVALID_HANDLE_VALUE;
	size_t loops = 0;
	while (loops++ < 20) {
		if (remaining < fname.size()) {
			return nullptr;
		}

#ifdef UNICODE
		totalLen += os_utf8_to_wcs(fname.c_str(), fname.size(),
					   tpath + len, MAX_PATH);
#else
		// Pray the UTF8 is ASCII
		memcpy(tpath + len, path.c_str(), path.size() + 1);
#endif

		file = CreateFile(tpath, GENERIC_READ | GENERIC_WRITE,
				  FILE_SHARE_READ, NULL, CREATE_NEW,
				  FILE_ATTRIBUTE_TEMPORARY |
					  FILE_FLAG_SEQUENTIAL_SCAN,
				  NULL);
		if (file != INVALID_HANDLE_VALUE) {
			break;
		}
		switch (GetLastError()) {
		case ERROR_FILE_EXISTS:
			fname = "elgatocloud-" + random_name();
			break;
		default:
			return nullptr;
		}
	}
	if (loops >= 20) {
		return nullptr;
	}
	int fhandle = _open_osfhandle((intptr_t)file, 0);
	if (fhandle == -1) {
		CloseHandle(file);
		return nullptr;
	}
#ifdef UNICODE
	outFilename.resize(MAX_PATH + 1);
	len = os_wcs_to_utf8(tpath, totalLen, &outFilename[0], MAX_PATH);
	outFilename.resize(len);
#else
	outFilename = tpath;
#endif

	return fdopen(fhandle, mode);
#else
	// TODO: Mac
#endif
}

bool move_file(const std::string &from, const std::string &to)
{
#ifdef WIN32
	TString TFrom = makeLongPath(from);
	TString TTo = makeLongPath(to);

	return MoveFile(TFrom.c_str(), TTo.c_str());
#else
#error "Unimplemented"
#endif
}

std::string move_file_safe(const std::string &from, const std::string &to)
{
	if (move_file(from, to)) {
		return to;
	}
	size_t baseNameStart = to.find_last_of("/\\");
	size_t extensionStart = to.find_first_of('.', baseNameStart);
	std::string baseName = to.substr(0, extensionStart);

	std::string extension = "";
	if (extensionStart != std::string::npos) {
		extension = to.substr(extensionStart);
	}
	size_t tries = 1;
	std::string newDst;
	do {
		tries++;
		newDst = baseName + "(" + std::to_string(tries) + ")" +
			 extension;
	} while (!move_file(from, newDst));

	return newDst;
}