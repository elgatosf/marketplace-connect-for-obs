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
#include <QFileDialog>
#include <QMainWindow>

#include <scene-bundle.hpp>
#include <export-wizard.hpp>
#include <elgato-product.hpp>
#include <util.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

namespace elgatocloud {
extern void InitElgatoCloud(obs_module_t *);
extern obs_data_t* GetElgatoCloudConfig();
extern void OpenExportWizard();
}

void save_pack()
{
	elgatocloud::OpenExportWizard();
}

void export_collection(void *)
{
	save_pack();
}

void import_collection(void*)
{
	const auto mainWindow =
		static_cast<QMainWindow*>(obs_frontend_get_main_window());
	QString fileName = QFileDialog::getOpenFileName(mainWindow,
		"Select Bundle", "", "Deeplink File (*.elgatoscene)");
	if (fileName.size() == 0) {
		return;
	}
	elgatocloud::ElgatoProduct product("Bundle Name");
	product.Install(fileName.toStdString(), &product, false);
}

bool obs_module_load(void)
{
	obs_log(LOG_INFO, "plugin loaded successfully (version %s)",
		PLUGIN_VERSION);
	elgatocloud::InitElgatoCloud(obs_current_module());
	auto config = elgatocloud::GetElgatoCloudConfig();
	bool makerTools = obs_data_get_bool(config, "MakerTools");
	obs_data_release(config);
	if (makerTools || true) {
		obs_frontend_add_tools_menu_item("Export Marketplace Scene",
			export_collection, NULL);
		obs_frontend_add_tools_menu_item("Import Marketplace Scene",
			import_collection, NULL);
	}
	return true;
}

void obs_module_unload(void)
{
	obs_log(LOG_INFO, "plugin unloaded");
}
