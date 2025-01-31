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

#pragma once
#include <QWindow>
#include <obs.hpp>
#include <obs-module.h>
#include <obs-frontend-api.h>

bool QTToGSWindow(QWindow *window, gs_window &gswindow);
void GetScaleAndCenterPos(int baseCX, int baseCY, int windowCX, int windowCY,
			  int &x, int &y, float &scale);
bool GetFileSafeName(const char* name, std::string& file);
bool GetClosestUnusedFileName(std::string& path, const char* extension);
std::vector<std::string> GetSceneCollectionNames();
config_t* GetUserConfig();
