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

#include <map>
#include <string>
#include <vector>

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <obs.hpp>

#include <QDialog>
#include <QWidget>
#include <QLabel>
#include <QComboBox>
#include <QStackedWidget>
#include <QMovie>
#include <QTConcurrent>

#include <nlohmann/json.hpp>

#include "elgato-cloud-config.hpp"
#include "elgato-widgets.hpp"
#include "elgato-cloud-data.hpp"
#include "qt-display.hpp"
#include "plugins.hpp"

namespace elgatocloud {

class ElgatoProduct;
class ElgatoCloudWindow;


enum class InstallTypes {
	Unset,
	NewCollection,
	AddToCollection,
	ReplaceCollection
};

struct Setup {
	InstallTypes installType;
	std::string collectionName;
	std::map<std::string, std::string> videoSettings;
	std::string audioSettings;
};

class StreamPackageHeader : public QWidget {
	Q_OBJECT
public:
	StreamPackageHeader(QWidget *parent, std::string name,
			    std::string thumbnailPath);

private:
	QLabel *_thumbnail(std::string thumbnailPath);
};

class MissingPlugins : public QWidget {
	Q_OBJECT
public:
	MissingPlugins(QWidget *parent, std::string name,
		       std::string thumbnailPath,
		       std::vector<PluginDetails> &missing);
};

class MissingPluginItem : public QWidget {
	Q_OBJECT
public:
	MissingPluginItem(QWidget *parent, std::string label, std::string url);
};

class InstallType : public QWidget {
	Q_OBJECT
public:
	InstallType(QWidget *parent, std::string name,
		    std::string thumbnailPath);
signals:
	void newCollectionPressed();
	void existingCollectionPressed();
};

class NewCollectionName : public QWidget {
	Q_OBJECT
public:
	NewCollectionName(QWidget *parent, std::string name,
			  std::string thumbnailPath);

private:
	std::vector<std::string> _existingCollections;
	QLineEdit *_nameField = nullptr;
	QPushButton *_proceedButton = nullptr;
signals:
	void proceedPressed(std::string name);
};

class VideoSetup : public QWidget {
	Q_OBJECT
public:
	VideoSetup(QWidget *parent, std::string name, std::string thumbnailPath,
		   std::map<std::string, std::string> videoSourceLabels);
	~VideoSetup();

	void OpenConfigVideoSource();
	static void DefaultVideoUpdated(void *data, calldata_t *params);

private:
	obs_source_t *_videoCaptureSource = nullptr;
	OBSQTDisplay *_videoPreview = nullptr;
	QComboBox *_videoSources = nullptr;
	std::vector<std::string> _videoSourceIds;
	std::vector<VideoCaptureSourceSelector *> _videoSelectors;

signals:
	void proceedPressed(std::map<std::string, std::string> settings);
	void backPressed();
};

class AudioSetup : public QWidget {
	Q_OBJECT
public:
	AudioSetup(QWidget *parent, std::string name,
		   std::string thumbnailPath);
	~AudioSetup();

	void SetupVolMeter();
	void OpenConfigAudioSource();

	static void OBSVolumeLevel(void *data,
				   const float magnitude[MAX_AUDIO_CHANNELS],
				   const float peak[MAX_AUDIO_CHANNELS],
				   const float inputPeak[MAX_AUDIO_CHANNELS]);

	static void DefaultAudioUpdated(void *data, calldata_t *params);

private:
	void _setupTempSources(obs_data_t* audioSettings);
	SimpleVolumeMeter *_levelsWidget = nullptr;
	obs_source_t *_audioCaptureSource = nullptr;
	obs_volmeter_t *_volmeter = nullptr;
	QComboBox *_audioSources = nullptr;
	std::vector<std::string> _audioSourceIds;

signals:
	void proceedPressed(std::string settings);
	void backPressed();
};

class Loading : public QWidget {
	Q_OBJECT
public:
	Loading(QWidget* parent);

private:
	QMovie* _indicator;
};

class StreamPackageSetupWizard : public QDialog {
	Q_OBJECT

public:
	StreamPackageSetupWizard(QWidget *parent, ElgatoProduct *product,
				 std::string filename, bool deleteOnClose);
	~StreamPackageSetupWizard();
	void install();
	void OpenArchive();
	static bool DisableVideoCaptureSources(void *data,
					       obs_source_t *source);
	void EnableVideoCaptureSources();

private:
	void _buildBaseUI();
	void _buildMissingPluginsUI(std::vector<PluginDetails> &missing);
	void
	_buildSetupUI(std::map<std::string, std::string> &videoSourceLabels);
	std::string _productName;
	std::string _thumbnailPath;
	std::string _filename;
	bool _deleteOnClose;
	QStackedWidget *_steps;
	Setup _setup;
	std::vector<obs_weak_source_t *> _toEnable;
	QFuture<void> _future;
};

StreamPackageSetupWizard *GetSetupWizard();
std::string GetBundleInfo(std::string filename);

} // namespace elgatocloud
