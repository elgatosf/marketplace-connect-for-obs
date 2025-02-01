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
#include "plugins.hpp"
#include <fstream>
#include <algorithm>
#include <nlohmann/json.hpp>

namespace elgatocloud {

PluginInfo::PluginInfo()
{
	_loadInstalled();
	_loadApproved();
}

PluginInfo::~PluginInfo() {}

void PluginInfo::addModule(void *param, obs_module_t *module)
{
	auto pi = static_cast<PluginInfo *>(param);
	pi->_installedPlugins.push_back(obs_get_module_file_name(module));
}

std::vector<PluginDetails> PluginInfo::installed() const
{
	std::vector<PluginDetails> res;
	std::copy_if(_approvedPlugins.begin(), _approvedPlugins.end(),
		     std::back_inserter(res),
		     [](const PluginDetails &pi) { return pi.installed; });
	std::sort(res.begin(), res.end(),
		  [](const PluginDetails &i, const PluginDetails &j) {
			  return (i.name < j.name);
		  });
	return res;
}

std::vector<PluginDetails>
PluginInfo::missing(std::vector<std::string> required) const
{
	std::vector<PluginDetails> res;
	std::copy_if(
		_approvedPlugins.begin(), _approvedPlugins.end(),
		std::back_inserter(res), [required](const PluginDetails &pi) {
			bool req = (std::find(required.begin(), required.end(),
					      pi.files[0]) != required.end());
			return req && !pi.installed;
		});
	return res;
}

void PluginInfo::_loadApproved()
{
	std::string dataPath = obs_get_module_data_path(obs_current_module());
	std::ifstream f(dataPath + "/plugins.json");
	nlohmann::json plugins = nlohmann::json::parse(f);
	for (auto &plugin : plugins["supported_plugins"]) {
		std::string filename = plugin["files"][0];
		std::string url = plugin["url"];
		std::string name = plugin["name"];
		std::vector<std::string> files = plugin["files"];
		bool installed = std::find(_installedPlugins.begin(),
					   _installedPlugins.end(),
					   filename) != _installedPlugins.end();
		_approvedPlugins.push_back({installed, name, url, files});
	}
}

void PluginInfo::_loadInstalled()
{
	obs_enum_modules(PluginInfo::addModule, this);
}

} // namespace elgatocloud
