/******************************************************************************
    Copyright (C) 2023 by Lain Bailey <lain@obsproject.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/
#include <algorithm>
#include <filesystem>
#include "obs-utils.hpp"
#include <util/platform.h>

bool QTToGSWindow(QWindow *window, gs_window &gswindow)
{
	bool success = true;

#ifdef _WIN32
	gswindow.hwnd = (HWND)window->winId();
#elif __APPLE__
	gswindow.view = (id)window->winId();
#else
	switch (obs_get_nix_platform()) {
	case OBS_NIX_PLATFORM_X11_EGL:
		gswindow.id = window->winId();
		gswindow.display = obs_get_nix_platform_display();
		break;
#ifdef ENABLE_WAYLAND
	case OBS_NIX_PLATFORM_WAYLAND: {
		QPlatformNativeInterface *native =
			QGuiApplication::platformNativeInterface();
		gswindow.display =
			native->nativeResourceForWindow("surface", window);
		success = gswindow.display != nullptr;
		break;
	}
#endif
	default:
		success = false;
		break;
	}
#endif
	return success;
}

void GetScaleAndCenterPos(int baseCX, int baseCY, int windowCX, int windowCY,
			  int &x, int &y, float &scale)
{
	int newCX, newCY;

	const double windowAspect = double(windowCX) / double(windowCY);
	const double baseAspect = double(baseCX) / double(baseCY);

	if (windowAspect > baseAspect) {
		scale = float(windowCY) / float(baseCY);
		newCX = int(double(windowCY) * baseAspect);
		newCY = windowCY;
	} else {
		scale = float(windowCX) / float(baseCX);
		newCX = windowCX;
		newCY = int(float(windowCX) / baseAspect);
	}

	x = windowCX / 2 - newCX / 2;
	y = windowCY / 2 - newCY / 2;
}

bool GetFileSafeName(const char *name, std::string &file)
{
	size_t base_len = strlen(name);
	size_t len = os_utf8_to_wcs(name, base_len, nullptr, 0);
	std::wstring wfile;

	if (!len)
		return false;

	wfile.resize(len);
	os_utf8_to_wcs(name, base_len, &wfile[0], len + 1);

	for (size_t i = wfile.size(); i > 0; i--) {
		size_t im1 = i - 1;

		if (iswspace(wfile[im1])) {
			wfile[im1] = '_';
		} else if (wfile[im1] != '_' && !iswalnum(wfile[im1])) {
			wfile.erase(im1, 1);
		}
	}

	if (wfile.size() == 0)
		wfile = L"characters_only";

	len = os_wcs_to_utf8(wfile.c_str(), wfile.size(), nullptr, 0);
	if (!len)
		return false;

	file.resize(len);
	os_wcs_to_utf8(wfile.c_str(), wfile.size(), &file[0], len + 1);
	return true;
}

bool GetClosestUnusedFileName(std::string &path, const char *extension)
{
	size_t len = path.size();
	if (extension) {
		path += ".";
		path += extension;
	}

	if (!os_file_exists(path.c_str()))
		return true;

	int index = 1;

	do {
		path.resize(len);
		path += std::to_string(++index);
		if (extension) {
			path += ".";
			path += extension;
		}
	} while (os_file_exists(path.c_str()));

	return true;
}

std::vector<std::string> GetSceneCollectionNames()
{
	char **collections = obs_frontend_get_scene_collections();
	char **name = collections;
	std::vector<std::string> ret;

	while (name && *name) {
		ret.push_back(*name);
		name++;
	}
	bfree(collections);
	return ret;
}

#if LIBOBS_API_VER < MAKE_SEMANTIC_VERSION(31, 0, 0)
static config_t *(*get_user_config_func)(void) = nullptr;
static config_t *user_config = nullptr;
#endif

config_t *GetUserConfig()
{
#if LIBOBS_API_VER < MAKE_SEMANTIC_VERSION(31, 0, 0)
	if (user_config)
		return user_config;
	if (!get_user_config_func) {
		if (obs_get_version() < MAKE_SEMANTIC_VERSION(31, 0, 0)) {
			get_user_config_func = obs_frontend_get_global_config;
		} else {
#ifdef __APPLE__
			auto handle = os_dlopen("obs-frontend-api.dylib");
#else
			auto handle = os_dlopen("obs-frontend-api");
#endif
			if (handle) {
				get_user_config_func = (config_t * (*)(void))
					os_dlsym(
						handle,
						"obs_frontend_get_user_config");
				os_dlclose(handle);
			}
		}
	}
	if (get_user_config_func)
		return get_user_config_func();
	user_config = obs_frontend_get_global_config();
	return user_config;
#else
	return obs_frontend_get_user_config();
#endif
}

std::string GetDataPath()
{
	std::string basePath = obs_get_module_data_path(obs_current_module());
	size_t pos = 0;
	std::string from = "\\";
	std::string to = "/";
	while ((pos = basePath.find(from, pos)) != std::string::npos) {
		basePath.replace(pos, from.length(), to);
		pos += to.length();
	}
	return basePath;
}
