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

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>

#include <QWidget>
#include <QString>
#include <QDialog>

#include <scene-bundle.hpp>
#include <export-wizard.hpp>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

namespace elgatocloud {
extern void InitElgatoCloud(obs_module_t *);
extern void OpenExportWizard();
}

void save_pack()
{
	elgatocloud::OpenExportWizard();
	// First, get the json data for the current scene collection
	// 
	//char *current_collection = obs_frontend_get_current_scene_collection();
	//std::string collection_name = current_collection;
	//bfree(current_collection);

	//SceneBundle bundle;
	//if (!bundle.FromCollection(collection_name)) {
	//	return;
	//}

	//if (!bundle.FileCheckDialog()) {
	//	return;
	//}

	//QWidget *window = (QWidget *)obs_frontend_get_main_window();
	//QString filename = QFileDialog::getSaveFileName(
	//	window, "Save As...", QString(), "*.elgatoscene");
	//if (filename == "") {
	//	return;
	//}
	//if (!filename.endsWith(".elgatoscene")) {
	//	filename += ".elgatoscene";
	//}

	//std::string filename_utf8 = filename.toUtf8().constData();

	//bundle.ToElgatoCloudFile(filename_utf8);

	//blog(LOG_INFO, "Saved to %s", filename_utf8.c_str());
}

void export_collection(void *)
{
	save_pack();
}

bool obs_module_load(void)
{
	obs_log(LOG_INFO, "plugin loaded successfully (version %s)",
		PLUGIN_VERSION);
	elgatocloud::InitElgatoCloud(obs_current_module());
	obs_frontend_add_tools_menu_item("Elgato Cloud Plugin Save",
					 export_collection, NULL);
	return true;
}

void obs_module_unload(void)
{
	obs_log(LOG_INFO, "plugin unloaded");
}
