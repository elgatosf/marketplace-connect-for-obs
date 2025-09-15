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

struct SceneCollectionInfo;
struct SDFileDetails;

namespace elgatocloud {

class ElgatoProduct;
class ElgatoCloudWindow;

enum class InstallTypes {
	Unset,
	NewCollection,
	AddToCollection,
	ReplaceCollection
};

struct OutputScene {
	std::string id;
	std::string name;
	bool enabled;
};

struct Setup {
	InstallTypes installType;
	std::string collectionName;
	std::map<std::string, std::string> videoSettings;
	std::string audioSettings;
	std::vector<std::string> scenesToMerge;
};

class StepsSideBar : public QWidget {
	Q_OBJECT
public:
	StepsSideBar(std::vector<std::string> const& steps, std::string name, std::string thumbnailPath, QWidget* parent);
	void setStep(int step);
	void incrementStep();
	void decrementStep();

private:
	Stepper* _stepper;
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

class MissingSourceClone : public QWidget {
	Q_OBJECT
public:
	MissingSourceClone(std::string name, std::string thumbnailPath, QWidget* parent=nullptr);
signals:
	void backPressed();
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

class StartInstall : public QWidget {
	Q_OBJECT
public:
	StartInstall(QWidget* parent, std::string name,
		std::string thumbnailPath);
signals:
	void newCollectionPressed();
	void mergeCollectionPressed();
};

class NewCollectionName : public QWidget {
	Q_OBJECT
public:
	NewCollectionName(
		std::string titleText, std::string subTitleText,
		std::vector<std::string> const& steps, int step,
		std::string name, std::string thumbnailPath,
		QWidget* parent);

private:
	std::vector<std::string> _existingCollections;
	QLineEdit *_nameField = nullptr;
	QPushButton *_proceedButton = nullptr;
signals:
	void proceedPressed(std::string name);
	void backPressed();
};

class VideoSetup : public QWidget {
	Q_OBJECT
public:
	VideoSetup(std::vector<std::string> const& steps, int step, std::string name, std::string thumbnailPath,
		   std::map<std::string, std::string> videoSourceLabels, QWidget* parent);
	~VideoSetup();

	void OpenConfigVideoSource();
	static void DefaultVideoUpdated(void *data, calldata_t *params);
	void DisableTempSources();
	void EnableTempSources();

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
	AudioSetup(std::vector<std::string> const& steps, int step, std::string name,
		   std::string thumbnailPath, QWidget* parent);
	~AudioSetup();

	void SetupVolMeter();
	void OpenConfigAudioSource();

	static void OBSVolumeLevel(void *data,
				   const float magnitude[MAX_AUDIO_CHANNELS],
				   const float peak[MAX_AUDIO_CHANNELS],
				   const float inputPeak[MAX_AUDIO_CHANNELS]);

	static void DefaultAudioUpdated(void *data, calldata_t *params);

private:
	void _setupTempSources(obs_data_t *audioSettings);
	SimpleVolumeMeter *_levelsWidget = nullptr;
	obs_source_t *_audioCaptureSource = nullptr;
	obs_volmeter_t *_volmeter = nullptr;
	QComboBox *_audioSources = nullptr;
	std::vector<std::string> _audioSourceIds;

signals:
	void proceedPressed(std::string settings);
	void backPressed();
};

class MergeSelectScenes : public QWidget {
	Q_OBJECT
public:
	MergeSelectScenes(std::vector<OutputScene>& outputScenes, std::vector<std::string> const& steps, int step, std::string name,
		std::string thumbnailPath, QWidget* parent);
	~MergeSelectScenes();
	std::vector<std::string> getSelectedScenes();
signals:
	void proceedPressed();
	void backPressed();

private:
	std::vector<OutputScene> _outputScenes;
};

class Loading : public QWidget {
	Q_OBJECT
public:
	Loading(QWidget *parent);

private:
	QMovie *_indicator;
};

class StreamPackageSetupWizard : public QDialog {
	Q_OBJECT

public:
	StreamPackageSetupWizard(QWidget *parent, ElgatoProduct *product,
				 std::string filename, bool deleteOnClose);
	~StreamPackageSetupWizard();
	void OpenArchive();
	static bool DisableVideoCaptureSources(void *data,
					       obs_source_t *source);
	static bool EnableVideoCaptureSourcesActive(void* data,
						   obs_source_t* source);

private:
	void _buildBaseUI();
	void _buildMissingPluginsUI(std::vector<PluginDetails> &missing);
	void
	_buildSetupUI(
		std::map<std::string, std::string> &videoSourceLabels,
		std::vector<OutputScene> &outputScenes,
		std::vector<SDFileDetails> &streamDeckActions,
		std::vector<SDFileDetails> &streamDeckProfiles
	);
	void _buildNewCollectionUI(
		std::map<std::string, std::string> &videoSourceLabels,
		std::vector<SDFileDetails> &streamDeckActions,
		std::vector<SDFileDetails> &streamDeckProfiles);
	void _buildMergeCollectionUI(std::map<std::string, std::string>& videoSourceLabels, std::vector<OutputScene>& outputScenes);
	std::string _productName;
	std::string _productId;
	std::string _productSlug;
	std::string _thumbnailPath;
	std::string _filename;
	std::string _curCollectionFileName;
	bool _deleteOnClose;
	bool _installStarted;
	QStackedWidget* _newCollectionSteps;
	QStackedWidget* _container;
	QStackedWidget* _mergeCollectionSteps;
	Setup _setup;
	std::vector<std::string> _toEnable;
	VideoSetup *_vSetup;
	VideoSetup* _vSetupMerge;
	VideoSetup* _vSetupSubMerge;
	std::string sdFilesPath_;
	QFuture<void> _future;
};

void installStreamPackage(Setup setup, std::string filename, bool deleteOnClose,
			  std::vector<std::string> toEnable,
			  std::string productName, std::string productId,
			  std::string productSlug);
void mergeStreamPackage(Setup setup, std::string filename, bool deleteOnClose, std::vector<std::string> toEnable);

bool EnableVideoCaptureSourcesActive(void* data, obs_source_t* source);
void EnableVideoCaptureSourcesJson(std::vector<std::string> sourceIds, std::string curFileName);

StreamPackageSetupWizard *GetSetupWizard();
SceneCollectionInfo GetBundleInfo(std::string filename);

} // namespace elgatocloud
