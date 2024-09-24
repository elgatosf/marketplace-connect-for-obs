/*
Elgato Deep - Linking OBS Plug - In
Copyright(C) 2024 Corsair Memory Inc.oss.elgato@corsair.com

This program is free software; you can redistribute it and /or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program.If not, see < https://www.gnu.org/licenses/>
*/

#pragma once

#include <map>
#include <string>

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <obs.hpp>
#include <obs-module.h>

#include <QDialog>
#include <QWidget>
#include <QLabel>
#include <QComboBox>
#include <QStackedWidget>

#include "scene-bundle.hpp"

namespace elgatocloud {

class FileCollectionCheck : public QWidget {
	Q_OBJECT

public:
	FileCollectionCheck(QWidget *parent, std::vector<std::string> files);

signals:
	void continuePressed();
	void cancelPressed();

private:
	std::vector<std::string> _files;
};

class VideoSourceLabels : public QWidget {
	Q_OBJECT
public:
	VideoSourceLabels(QWidget *parent,
			  std::map<std::string, std::string> devices);
	inline std::map<std::string, std::string> Labels() { return _labels; }
signals:
	void continuePressed();
	void backPressed();

private:
	std::map<std::string, std::string> _labels;
};

class RequiredPlugins : public QWidget {
	Q_OBJECT
public:
	RequiredPlugins(QWidget *parent,
			std::vector<obs_module_t *> installedPlugins);
	std::vector<std::string> RequiredPluginList();
signals:
	void continuePressed();
	void backPressed();

private:
	std::map<std::string, bool> _pluginStatus;
};

class ExportComplete : public QWidget {
	Q_OBJECT

public:
	ExportComplete(QWidget *parent);

signals:
	void closePressed();
};

class StreamPackageExportWizard : public QDialog {
	Q_OBJECT

public:
	StreamPackageExportWizard(QWidget *parent);
	~StreamPackageExportWizard();

	static void AddModule(void *data, obs_module_t *module);

private:
	QStackedWidget *_steps;
	SceneBundle _bundle;
	std::vector<obs_module_t *> _modules;
};

void OpenExportWizard();

} // namespace elgatocloud
