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

#include <algorithm>

#include <util/platform.h>

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QListWidget>
#include <QScrollArea>
#include <QDesktopServices>
#include <QPixmap>
#include <QPainter>
#include <QString>
#include <QPalette>
#include <QPainterPath>
#include <QLineEdit>
#include <QApplication>
#include <QThread>
#include <QMetaObject>

#include "elgato-styles.hpp"
#include "plugin-support.h"
#include "setup-wizard.hpp"
#include "elgato-product.hpp"
#include "scene-bundle.hpp"
#include "obs-utils.hpp"
#include "util.h"

namespace elgatocloud {

StreamPackageSetupWizard *setupWizard = nullptr;

StreamPackageSetupWizard *GetSetupWizard()
{
	return setupWizard;
}

StreamPackageHeader::StreamPackageHeader(QWidget *parent, std::string name,
					 std::string thumbnailPath = "")
	: QWidget(parent)
{
	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setSpacing(16);
	layout->setContentsMargins(0, 0, 0, 0);
	QLabel *nameLabel = new QLabel(this);
	nameLabel->setText(name.c_str());
	nameLabel->setSizePolicy(QSizePolicy::Expanding,
				 QSizePolicy::Preferred);
	nameLabel->setStyleSheet("QLabel {font-size: 18pt;}");
	layout->addWidget(nameLabel);
	if (thumbnailPath != "") {
		auto thumbnail = _thumbnail(thumbnailPath);
		thumbnail->setAlignment(Qt::AlignCenter);
		layout->addWidget(thumbnail);
	}
}

QLabel *StreamPackageHeader::_thumbnail(std::string thumbnailPath)
{
	int targetHeight = 140;
	int cornerRadius = 8;

	QPixmap img;
	img.load(thumbnailPath.c_str());

	int width = img.width();
	int height = img.height();
	int targetWidth =
		int((double)targetHeight * (double)width / (double)height);
	QPixmap target(targetWidth, targetHeight);
	target.fill(Qt::transparent);
	QPainter painter(&target);

	painter.setRenderHint(QPainter::Antialiasing, true);
	painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

	QPainterPath path = QPainterPath();
	path.addRoundedRect(0, 0, targetWidth, targetHeight, cornerRadius,
			    cornerRadius);
	painter.setClipPath(path);
	painter.drawPixmap(0, 0, img.scaledToHeight(targetHeight));

	QLabel *labelImg = new QLabel(this);
	labelImg->setPixmap(target);
	labelImg->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	return labelImg;
}

MissingPlugins::MissingPlugins(QWidget *parent, std::string name,
			       std::string thumbnailPath,
			       std::vector<PluginDetails> &missing)
	: QWidget(parent)
{
	QVBoxLayout *layout = new QVBoxLayout(this);
	StreamPackageHeader *header = new StreamPackageHeader(parent, name);
	layout->addWidget(header);
	QLabel *title = new QLabel(this);
	title->setText("Missing Plugins");
	title->setAlignment(Qt::AlignCenter);
	title->setStyleSheet("QLabel {font-size: 14pt;}");
	layout->addWidget(title);

	QLabel *subTitle = new QLabel(this);
	subTitle->setText(
		"The following plugins are required to use this scene collection, "
		"but are not installed. Please install these plugins, restart "
		"OBS and attempt to install again.");
	subTitle->setAlignment(Qt::AlignCenter);
	subTitle->setWordWrap(true);
	subTitle->setStyleSheet("QLabel {font-size: 12pt;}");
	layout->addWidget(subTitle);

	auto pluginList = new QListWidget(this);
	for (auto &plugin : missing) {
		auto item = new QListWidgetItem();
		item->setSizeHint(QSize(0, 60));
		auto itemWidget = new MissingPluginItem(
			this, plugin.name.c_str(), plugin.url.c_str());
		pluginList->addItem(item);
		pluginList->setItemWidget(item, itemWidget);
	}

	pluginList->setStyleSheet(EMissingPluginsStyle);
	pluginList->setSpacing(4);
	pluginList->setSelectionMode(QAbstractItemView::NoSelection);
	pluginList->setFocusPolicy(Qt::FocusPolicy::NoFocus);

	layout->addWidget(pluginList);
}

MissingPluginItem::MissingPluginItem(QWidget *parent, std::string label,
				     std::string url)
	: QWidget(parent)
{
	auto layout = new QHBoxLayout(this);
	auto itemLabel = new QLabel(this);
	itemLabel->setText(label.c_str());
	itemLabel->setStyleSheet("QLabel {font-size: 12pt;}");
	layout->addWidget(itemLabel);

	auto spacer = new QWidget(this);
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	layout->addWidget(spacer);

	auto downloadButton = new QPushButton(this);
	downloadButton->setText("Download");
	downloadButton->setStyleSheet(EPushButtonStyle);

	connect(downloadButton, &QPushButton::released, this,
		[url]() { QDesktopServices::openUrl(QUrl(url.c_str())); });
	layout->addWidget(downloadButton);
}

InstallType::InstallType(QWidget *parent, std::string name,
			 std::string thumbnailPath)
	: QWidget(parent)
{
	QVBoxLayout *layout = new QVBoxLayout(this);
	StreamPackageHeader *header =
		new StreamPackageHeader(parent, name, thumbnailPath);
	layout->addWidget(header);
	QLabel *selectInstall = new QLabel(this);
	selectInstall->setText("Select Install");
	selectInstall->setAlignment(Qt::AlignCenter);
	selectInstall->setStyleSheet("QLabel {font-size: 14pt;}");
	layout->addWidget(selectInstall);

	// Setup New and Existing buttons.
	QHBoxLayout *buttons = new QHBoxLayout(this);
	QPushButton *newButton = new QPushButton(this);
	newButton->setText("New");
	newButton->setStyleSheet(EPushButtonDarkStyle);

	connect(newButton, &QPushButton::released, this,
		[this]() { emit newCollectionPressed(); });
	QPushButton *existingButton = new QPushButton(this);
	existingButton->setText("Existing");
	existingButton->setStyleSheet(EPushButtonDarkStyle);
	existingButton->setDisabled(true);
	connect(existingButton, &QPushButton::released, this,
		[this]() { emit existingCollectionPressed(); });
	buttons->addWidget(newButton);
	buttons->addWidget(existingButton);

	layout->addLayout(buttons);
}

NewCollectionName::NewCollectionName(QWidget *parent, std::string name,
				     std::string thumbnailPath)
	: QWidget(parent)
{
	QVBoxLayout *layout = new QVBoxLayout(this);
	StreamPackageHeader *header =
		new StreamPackageHeader(parent, name, thumbnailPath);
	layout->addWidget(header);
	QLabel *nameCollection = new QLabel(this);
	nameCollection->setText("Create Scene Collection");
	nameCollection->setStyleSheet(
		"QLabel {font-size: 12pt; margin-top: 16px;}");
	layout->addWidget(nameCollection);

	_nameField = new QLineEdit(this);
	_nameField->setPlaceholderText("Collection Name");
	_nameField->setStyleSheet(ELineEditStyle);
	layout->addWidget(_nameField);
	connect(_nameField, &QLineEdit::textChanged,
		[this](const QString text) {
			_proceedButton->setEnabled(text.length() > 0);
		});

	QWidget *spacer = new QWidget(this);
	spacer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	layout->addWidget(spacer);
	auto buttons = new QHBoxLayout();
	auto hSpacer = new QWidget(this);
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	buttons->addWidget(hSpacer);
	_proceedButton = new QPushButton(this);
	_proceedButton->setText("Next");
	_proceedButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	_proceedButton->setStyleSheet(EPushButtonStyle);
	_proceedButton->setDisabled(true);
	connect(_proceedButton, &QPushButton::released, this, [this]() {
		QString qName = _nameField->text();
		emit proceedPressed(qName.toStdString());
	});
	buttons->addWidget(_proceedButton);
	layout->addLayout(buttons);
}

VideoSetup::VideoSetup(QWidget *parent, std::string name,
		       std::string thumbnailPath,
		       std::map<std::string, std::string> videoSourceLabels)
	: QWidget(parent)
{
	auto layout = new QVBoxLayout(this);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	//StreamPackageHeader *header = new StreamPackageHeader(this, name, "");
	//layout->addWidget(header);
	layout->setContentsMargins(0, 0, 0, 0);

	auto sourceWidget = new QWidget(this);
	sourceWidget->setSizePolicy(QSizePolicy::Expanding,
				    QSizePolicy::Preferred);
	sourceWidget->setContentsMargins(0, 0, 0, 0);
	auto sourceGrid = new QGridLayout(sourceWidget);
	sourceGrid->setContentsMargins(0, 0, 0, 0);
	sourceGrid->setSpacing(18);

	config_t* const global_config = obs_frontend_get_global_config();

	config_set_default_string(global_config, "ElgatoCloud",
		"DefaultAudioCaptureSettings", "");
	config_set_default_string(global_config, "ElgatoCloud",
		"DefaultVideoCaptureSettings", "");

	std::string videoSettingsJson = config_get_string(
		global_config, "ElgatoCloud", "DefaultVideoCaptureSettings");

	obs_data_t* videoData =
		videoSettingsJson != ""
		? obs_data_create_from_json(videoSettingsJson.c_str())
		: nullptr;

	int i = 0;
	int j = 0;
	bool first = true;
	for (auto const &[sourceName, label] : videoSourceLabels) {


		auto vSource = new VideoCaptureSourceSelector(
			sourceWidget, label, sourceName, first ? videoData : nullptr);
		sourceGrid->addWidget(vSource, i, j);
		i += j % 2;
		j += 1;
		j %= 2;
		_videoSelectors.push_back(vSource);
		first = false;
	}

	obs_data_release(videoData);

	auto scrollArea = new QScrollArea(this);
	scrollArea->setContentsMargins(0, 0, 0, 0);
	scrollArea->setWidget(sourceWidget);
	scrollArea->setSizePolicy(QSizePolicy::Expanding,
				  QSizePolicy::Expanding);
	scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scrollArea->setStyleSheet("QScrollArea { border: none; }");
	layout->addWidget(scrollArea);

	auto buttons = new QHBoxLayout();

	auto spacer = new QWidget(this);
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	QPushButton *backButton = new QPushButton(this);
	backButton->setText("Back");
	backButton->setStyleSheet(EPushButtonStyle);

	QPushButton *proceedButton = new QPushButton(this);
	proceedButton->setText("Next");
	proceedButton->setStyleSheet(EPushButtonStyle);

	buttons->addWidget(spacer);
	buttons->addWidget(backButton);
	buttons->addWidget(proceedButton);
	connect(proceedButton, &QPushButton::released, this, [this]() {
		std::map<std::string, std::string> res;
		int i = 0;
		for (auto const &source : this->_videoSelectors) {
			std::string settings = source->GetSettings();
			std::string sourceName = source->GetSourceName();
			res[sourceName] = settings;
		}
		emit proceedPressed(res);
	});
	connect(backButton, &QPushButton::released, this,
		[this]() { emit backPressed(); });

	layout->addLayout(buttons);
}

VideoSetup::~VideoSetup() {}

void VideoSetup::DefaultVideoUpdated(void *data, calldata_t *params)
{
	UNUSED_PARAMETER(params);
	UNUSED_PARAMETER(data);
	// auto config = static_cast<VideoSetup*>(data);
	// config_t *const global_config = obs_frontend_get_global_config();
	// auto settings = obs_source_get_settings(config->_video_capture_source);
	// auto dataStr = obs_data_get_json(settings);
	// config_set_string(global_config, "ElgatoCloud", "DefaultVideoCaptureSettings", dataStr);

	// obs_data_release(settings);
}

void VideoSetup::OpenConfigVideoSource()
{
	if (!_videoCaptureSource) {
		return;
	}
	obs_frontend_open_source_properties(_videoCaptureSource);

	obs_properties_t *props = obs_source_properties(_videoCaptureSource);
	obs_property_t *devices = obs_properties_get(props, "video_device_id");
	for (size_t i = 0; i < obs_property_list_item_count(devices); i++) {
		std::string name = obs_property_list_item_name(devices, i);
		std::string id = obs_property_list_item_string(devices, i);
		obs_log(LOG_INFO, "--- VIDEO: %s [%s]", name.c_str(),
			id.c_str());
	}
	obs_properties_destroy(props);
}

AudioSetup::AudioSetup(QWidget *parent, std::string name,
		       std::string thumbnailPath)
	: QWidget(parent)
{
	auto layout = new QVBoxLayout(this);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	layout->setContentsMargins(0, 0, 0, 0);

	QLabel *audioDeviceLabel = new QLabel(this);
	audioDeviceLabel->setText("Audio Device");
	_audioSources = new QComboBox(this);
	_audioSources->setStyleSheet(EComboBoxStyle);

	_levelsWidget = new SimpleVolumeMeter(this, _volmeter);
	// Add Dropdown and meter
	QWidget *spacer = new QWidget(this);
	spacer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

	layout->addWidget(audioDeviceLabel);
	layout->addWidget(_audioSources);
	layout->addWidget(_levelsWidget);
	layout->addWidget(spacer);

	// Get settings
	config_t *const global_config = obs_frontend_get_global_config();
	config_set_default_string(global_config, "ElgatoCloud",
				  "DefaultAudioCaptureSettings", "");

	std::string audioSettingsJson = config_get_string(
		global_config, "ElgatoCloud", "DefaultAudioCaptureSettings");

	obs_data_t *audioSettings =
		audioSettingsJson != ""
			? obs_data_create_from_json(audioSettingsJson.c_str())
			: nullptr;

	_audioCaptureSource = nullptr;
	_setupTempSources(audioSettings);
	SetupVolMeter();

	obs_data_release(audioSettings);

	connect(_audioSources, &QComboBox::currentIndexChanged, this,
		[this](int index) {
			obs_data_t *aSettings =
				obs_source_get_settings(_audioCaptureSource);
			std::string id = _audioSourceIds[index];
			obs_data_set_string(aSettings, "device_id", id.c_str());

			if (_volmeter) {
				obs_volmeter_remove_callback(_volmeter,
					AudioSetup::OBSVolumeLevel, this);
				obs_volmeter_destroy(_volmeter);
				_volmeter = nullptr;
			}

			// For some reason, we need to completely deconstruct the temporary
			// audio capture source in order to connect the new device vol meter
			// to the volume meter in the UI. It should be possible to inject
			// the newly selected device, and re-connect the _volmeter callbacks
			_setupTempSources(aSettings);
			obs_data_release(aSettings);
			SetupVolMeter();
		});

	auto buttons = new QHBoxLayout();

	auto bSpacer = new QWidget(this);
	bSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	QPushButton *backButton = new QPushButton(this);
	backButton->setText("Back");
	backButton->setStyleSheet(EPushButtonStyle);

	QPushButton *proceedButton = new QPushButton(this);
	proceedButton->setText("Next");
	proceedButton->setStyleSheet(EPushButtonStyle);

	buttons->addWidget(bSpacer);
	buttons->addWidget(backButton);
	buttons->addWidget(proceedButton);
	connect(proceedButton, &QPushButton::released, this, [this]() {
		obs_data_t *aSettings =
			obs_source_get_settings(_audioCaptureSource);
		std::string aJson = obs_data_get_json(aSettings);
		obs_data_release(aSettings);
		emit proceedPressed(aJson);
	});
	connect(backButton, &QPushButton::released, this,
		[this]() { emit backPressed(); });
	layout->addLayout(buttons);
}

void AudioSetup::_setupTempSources(obs_data_t* audioSettings)
{
	if (_audioCaptureSource) {
		obs_source_release(_audioCaptureSource);
		obs_source_remove(_audioCaptureSource);
	}

	const char *audioSourceId = "wasapi_input_capture";
	const char *aId = obs_get_latest_input_type_id(audioSourceId);
	_audioCaptureSource = obs_source_create_private(
		aId, "elgato-cloud-audio-config", audioSettings);

	obs_properties_t *aProps = obs_source_properties(_audioCaptureSource);
	obs_property_t *aDevices = obs_properties_get(aProps, "device_id");
	for (size_t i = 0; i < obs_property_list_item_count(aDevices); i++) {
		std::string name = obs_property_list_item_name(aDevices, i);
		std::string id = obs_property_list_item_string(aDevices, i);
		_audioSourceIds.push_back(id);
		_audioSources->addItem(name.c_str());
	}
	obs_properties_destroy(aProps);

	obs_data_t *aSettings = obs_source_get_settings(_audioCaptureSource);
	std::string aDevice = obs_data_get_string(aSettings, "device_id");
	if (aDevice != "") {
		auto it = std::find(_audioSourceIds.begin(),
				    _audioSourceIds.end(), aDevice);
		if (it != _audioSourceIds.end()) {
			_audioSources->setCurrentIndex(
				static_cast<int>(it - _audioSourceIds.begin()));
		}
	}
	obs_data_release(aSettings);

	_levelsWidget->show();
	//signal_handler_t *audioSigHandler =
	//	obs_source_get_signal_handler(_audioCaptureSource);
	//signal_handler_connect_ref(audioSigHandler, "update",
	//			   AudioSetup::DefaultAudioUpdated, this);
}

void AudioSetup::SetupVolMeter()
{
	obs_log(LOG_INFO, "SetupVolMeter");
	//if (_volmeter) {
	//	obs_volmeter_remove_callback(_volmeter,
	//				     AudioSetup::OBSVolumeLevel, this);
	//	obs_volmeter_destroy(_volmeter);
	//	_volmeter = nullptr;
	//}
	_volmeter = obs_volmeter_create(OBS_FADER_LOG);
	obs_volmeter_attach_source(_volmeter, _audioCaptureSource);
	obs_volmeter_add_callback(_volmeter, AudioSetup::OBSVolumeLevel, this);
}

void AudioSetup::DefaultAudioUpdated(void *data, calldata_t *params)
{
	UNUSED_PARAMETER(params);
	auto config = static_cast<AudioSetup *>(data);
	config->SetupVolMeter();
	// auto config = static_cast<VideoSetup*>(data);
	// config_t *const global_config = obs_frontend_get_global_config();
	// auto settings = obs_source_get_settings(config->_audio_capture_source);
	// auto dataStr = obs_data_get_json(settings);
	// config_set_string(global_config, "ElgatoCloud", "DefaultAudioCaptureSettings", dataStr);

	// obs_data_release(settings);
}

void AudioSetup::OpenConfigAudioSource()
{
	if (!_audioCaptureSource) {
		return;
	}
	obs_frontend_open_source_properties(_audioCaptureSource);
	obs_properties_t *props = obs_source_properties(_audioCaptureSource);
	obs_property_t *devices = obs_properties_get(props, "device_id");
	for (size_t i = 0; i < obs_property_list_item_count(devices); i++) {
		std::string name = obs_property_list_item_name(devices, i);
		std::string id = obs_property_list_item_string(devices, i);
		obs_log(LOG_INFO, "--- MIC: %s [%s]", name.c_str(), id.c_str());
	}
	obs_properties_destroy(props);
}

void AudioSetup::OBSVolumeLevel(void *data,
				const float magnitude[MAX_AUDIO_CHANNELS],
				const float peak[MAX_AUDIO_CHANNELS],
				const float inputPeak[MAX_AUDIO_CHANNELS])
{
	UNUSED_PARAMETER(peak);
	UNUSED_PARAMETER(inputPeak);
	//obs_log(LOG_INFO, "OBS Volume Level");
	auto config = static_cast<AudioSetup *>(data);
	float mag = magnitude[0];
	float pk = peak[0];
	float ip = inputPeak[0];
	config->_levelsWidget->setLevel(mag, pk, ip);
	QMetaObject::invokeMethod(QCoreApplication::instance()->thread(),
				  [config]() {
					  if (!setupWizard) {
						  return;
					  }
					  config->_levelsWidget->update();
				  });
}

AudioSetup::~AudioSetup()
{
	if (_volmeter) {
		obs_volmeter_detach_source(_volmeter);
		obs_volmeter_remove_callback(_volmeter,
					     AudioSetup::OBSVolumeLevel, this);
		obs_volmeter_destroy(_volmeter);
	}
	if (_audioCaptureSource) {
		obs_source_release(_audioCaptureSource);
		obs_source_remove(_audioCaptureSource);
		//signal_handler_t *audioSigHandler =
		//	obs_source_get_signal_handler(_audioCaptureSource);
		//signal_handler_disconnect(audioSigHandler, "update",
		//			  AudioSetup::DefaultAudioUpdated,
		//			  this);
	}
}

StreamPackageSetupWizard::StreamPackageSetupWizard(QWidget *parent,
						   ElgatoProduct *product,
						   std::string filename,
						   bool deleteOnClose)
	: QDialog(parent),
	  _thumbnailPath(product->thumbnailPath),
	  _productName(product->name),
	  _filename(filename),
	  _deleteOnClose(deleteOnClose)
{
	setModal(true);
	SceneBundle bundle;
	auto bundleInfoStr = bundle.ExtractBundleInfo(filename);
	nlohmann::json bundleInfo;
	bool error = false;
	try {
		bundleInfo = nlohmann::json::parse(bundleInfoStr);
	} catch (const nlohmann::json::parse_error &e) {
		obs_log(LOG_ERROR, "Parsing Error.\n  message: %s\n  id: %i",
			e.what(), e.id);
		error = true;
	}

	std::map<std::string, std::string> videoSourceLabels =
		bundleInfo["video_devices"];
	std::vector<std::string> requiredPlugins =
		bundleInfo["plugins_required"];

	setupWizard = this;
	setWindowTitle("Install Scene Collection");
	setStyleSheet("background-color: #232323");

	// Disable all video capture sources so that single-thread
	// capture sources, such as the Elgato Facecam, can be properly
	// selected in the wizard.  Will re-enable any disabled sources
	// in the wizard destructor.
	obs_enum_sources(&StreamPackageSetupWizard::DisableVideoCaptureSources,
			 this);

	PluginInfo pi;
	std::vector<PluginDetails> missing = pi.missing(requiredPlugins);
	if (missing.size() > 0) {
		_buildMissingPluginsUI(missing);
	} else {
		_buildSetupUI(videoSourceLabels);
	}
}

StreamPackageSetupWizard::~StreamPackageSetupWizard()
{
	if (_deleteOnClose) {
		// Delete the scene collection file
		os_unlink(_filename.c_str());
	}
	EnableVideoCaptureSources();
	setupWizard = nullptr;
}

bool StreamPackageSetupWizard::DisableVideoCaptureSources(void *data,
							  obs_source_t *source)
{
	auto wizard = static_cast<StreamPackageSetupWizard *>(data);
	std::string sourceType = obs_source_get_id(source);
	if (sourceType == "dshow_input") {
		auto settings = obs_source_get_settings(source);
		bool active = obs_data_get_bool(settings, "active");
		obs_data_release(settings);
		if (active) {
			calldata_t cd = {};
			calldata_set_bool(&cd, "active", false);
			proc_handler_t *ph =
				obs_source_get_proc_handler(source);
			proc_handler_call(ph, "activate", &cd);
			calldata_free(&cd);
			auto wkSource = obs_source_get_weak_source(source);
			wizard->_toEnable.push_back(wkSource);
		}
	}
	return true;
}

void StreamPackageSetupWizard::EnableVideoCaptureSources()
{
	for (auto weakSource : _toEnable) {
		auto source = obs_weak_source_get_source(weakSource);
		if (source) {
			calldata_t cd = {};
			calldata_set_bool(&cd, "active", true);
			proc_handler_t *ph =
				obs_source_get_proc_handler(source);
			proc_handler_call(ph, "activate", &cd);
			calldata_free(&cd);
			obs_source_release(source);
		}
		obs_weak_source_release(weakSource);
	}
}

void StreamPackageSetupWizard::_buildMissingPluginsUI(
	std::vector<PluginDetails> &missing)
{
	setFixedSize(500, 350);
	QVBoxLayout *layout = new QVBoxLayout(this);

	auto missingPlugins =
		new MissingPlugins(this, _productName, _thumbnailPath, missing);
	layout->addWidget(missingPlugins);
}

void StreamPackageSetupWizard::_buildSetupUI(
	std::map<std::string, std::string> &videoSourceLabels)
{
	setFixedSize(320, 350);
	QVBoxLayout *layout = new QVBoxLayout(this);
	_steps = new QStackedWidget(this);

	// Step 1- select a new or existing install (step index: 0)
	InstallType *installerType =
		new InstallType(this, _productName, _thumbnailPath);
	connect(installerType, &InstallType::newCollectionPressed, this,
		[this]() {
			_setup.installType = InstallTypes::NewCollection;
			_steps->setCurrentIndex(1);
			setFixedSize(320, 384);
		});
	connect(installerType, &InstallType::existingCollectionPressed, this,
		[this]() {
			_setup.installType = InstallTypes::AddToCollection;
			_steps->setCurrentIndex(2);
			setFixedSize(320, 384);
		});
	_steps->addWidget(installerType);

	// Step 2a- Provide a name for the new collection (step index: 1)
	auto *newName =
		new NewCollectionName(this, _productName, _thumbnailPath);
	connect(newName, &NewCollectionName::proceedPressed, this,
		[this](std::string name) {
			_setup.collectionName = name;
			_steps->setCurrentIndex(3);
			setFixedSize(554, 440);
		});
	_steps->addWidget(newName);

	// Step 2b- Select an existing collection (step index: 2)
	_steps->addWidget(new QWidget(this));

	// Step 3- Set up Video inputs (step index: 3)
	auto vSetup = new VideoSetup(this, _productName, _thumbnailPath,
				     videoSourceLabels);
	_steps->addWidget(vSetup);
	connect(vSetup, &VideoSetup::proceedPressed, this,
		[this](std::map<std::string, std::string> settings) {
			//blog(LOG_INFO, "%s", settings.c_str());
			_setup.videoSettings = settings;
			_steps->setCurrentIndex(4);
			setFixedSize(320, 384);
		});
	connect(vSetup, &VideoSetup::backPressed, this, [this]() {
		_steps->setCurrentIndex(1);
		setFixedSize(320, 384);
	});
	// Step 4- Setup Audio Inputs (step index: 4)

	auto aSetup = new AudioSetup(this, _productName, _thumbnailPath);
	_steps->addWidget(aSetup);
	connect(aSetup, &AudioSetup::proceedPressed, this,
		[this](std::string settings) {
			_setup.audioSettings = settings;
			install();
		});
	connect(aSetup, &AudioSetup::backPressed, this, [this]() {
		_steps->setCurrentIndex(3);
		setFixedSize(554, 440);
	});

	layout->addWidget(_steps);
}

void StreamPackageSetupWizard::install()
{
	obs_log(LOG_INFO, "Elgato Req File: %s", _filename.c_str());

	// TODO: Clean up this mess of setting up the pack install path.
	config_t* const global_config = obs_frontend_get_global_config();
	std::string path = config_get_string(global_config, "ElgatoCloud", "InstallLocation");
	os_mkdirs(path.c_str());
	std::string packDirName(_setup.collectionName);
	std::replace(packDirName.begin(), packDirName.end(), ' ', '_');
	std::string pack_path = path + "/" + packDirName;
	obs_log(LOG_INFO, "Elgato Install Path: %s", pack_path.c_str());

	// TODO: Add dialog with progress indicator.  Put unzipping and
	//       scene collection loading code into new threads to stop
	//       from blocking.
	SceneBundle bundle;
	if (!bundle.FromElgatoCloudFile(_filename, pack_path)) {
		obs_log(LOG_WARNING, "Elgato Install: Could not install %s",
			_filename.c_str());
		return;
	}
	bundle.ToCollection(_setup.collectionName, _setup.videoSettings,
			    _setup.audioSettings, this);
}

} // namespace elgatocloud
