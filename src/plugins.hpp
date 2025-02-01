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
#include <string>
#include <vector>

namespace elgatocloud {

struct PluginDetails {
	bool installed;
	std::string name;
	std::string url;
	std::vector<std::string> files;
};

class PluginInfo {
public:
	PluginInfo();
	~PluginInfo();
	static void addModule(void *param, obs_module_t *module);
	std::vector<PluginDetails> installed() const;
	std::vector<PluginDetails>
	missing(std::vector<std::string> required) const;

private:
	void _loadInstalled();
	void _loadApproved();
	std::vector<PluginDetails> _approvedPlugins;
	std::vector<std::string> _installedPlugins;
};

} // namespace elgatocloud
