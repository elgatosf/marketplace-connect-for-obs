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
#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <obs.hpp>

#include <QDialog>
#include <QWidget>
#include <QLabel>
#include <QComboBox>
#include <QStackedWidget>

#include "elgato-cloud-config.hpp"
#include "qt-display.hpp"

namespace elgatocloud {

class ElgatoProduct;

enum class InstallTypes {
	Unset,
	NewCollection,
	AddToCollection,
	ReplaceCollection
};

struct Setup {
	InstallTypes installType;
	std::string collectionName;
	std::string videoSettings;
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
	QLineEdit *_nameField = nullptr;
	QPushButton *_proceedButton = nullptr;
signals:
	void proceedPressed(std::string name);
};

class AudioVideoSetup : public QWidget {
	Q_OBJECT
public:
	AudioVideoSetup(QWidget *parent, std::string name,
			std::string thumbnailPath);
	~AudioVideoSetup();

	void SetupVolMeter();
	void OpenConfigVideoSource();
	void OpenConfigAudioSource();
	static void DrawVideoPreview(void *data, uint32_t cx, uint32_t cy);

	static void OBSVolumeLevel(void *data,
				   const float magnitude[MAX_AUDIO_CHANNELS],
				   const float peak[MAX_AUDIO_CHANNELS],
				   const float inputPeak[MAX_AUDIO_CHANNELS]);

	static void DefaultVideoUpdated(void *data, calldata_t *params);
	static void DefaultAudioUpdated(void *data, calldata_t *params);

private:
	void _setupTempSources();
	SimpleVolumeMeter *_levelsWidget = nullptr;
	obs_source_t *_videoCaptureSource = nullptr;
	obs_source_t *_audioCaptureSource = nullptr;
	obs_volmeter_t *_volmeter = nullptr;
	OBSQTDisplay *_videoPreview = nullptr;
	QComboBox *_videoSources = nullptr;
	QComboBox *_audioSources = nullptr;
	std::vector<std::string> _videoSourceIds;
	std::vector<std::string> _audioSourceIds;

signals:
	void proceedPressed(std::string videoSettings,
			    std::string audioSettings);
};

class StreamPackageSetupWizard : public QDialog {
	Q_OBJECT

public:
	StreamPackageSetupWizard(QWidget *parent, ElgatoProduct *product,
				 std::string filename);
	~StreamPackageSetupWizard();
	void install();

private:
	std::string _productName;
	std::string _thumbnailPath;
	std::string _filename;
	QStackedWidget *_steps;
	Setup _setup;
};

StreamPackageSetupWizard *GetSetupWizard();

} // namespace elgatocloud
