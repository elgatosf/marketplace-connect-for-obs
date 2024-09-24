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
#include <QPixmap>
#include <QPainter>
#include <QString>
#include <QPalette>
#include <QPainterPath>
#include <QLineEdit>
#include <QApplication>
#include <QThread>
#include <QMetaObject>

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
	QHBoxLayout *titleLayout = new QHBoxLayout(this);
	QLabel *nameLabel = new QLabel(this);
	nameLabel->setText(name.c_str());
	nameLabel->setSizePolicy(QSizePolicy::Expanding,
				 QSizePolicy::Preferred);
	QLabel *installType = new QLabel(this);
	installType->setText("Stream Package");
	installType->setSizePolicy(QSizePolicy::Expanding,
				   QSizePolicy::Preferred);
	installType->setAlignment(Qt::AlignRight);
	titleLayout->addWidget(nameLabel);
	titleLayout->addWidget(installType);
	layout->addLayout(titleLayout);
	if (thumbnailPath != "") {
		layout->addWidget(_thumbnail(thumbnailPath));
	}
}

QLabel *StreamPackageHeader::_thumbnail(std::string thumbnailPath)
{
	int targetHeight = 120;
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
	layout->addWidget(selectInstall);

	// Setup New and Existing buttons.
	QHBoxLayout *buttons = new QHBoxLayout(this);
	QPushButton *newButton = new QPushButton(this);
	newButton->setText("New");
	connect(newButton, &QPushButton::released, this,
		[this]() { emit newCollectionPressed(); });
	QPushButton *existingButton = new QPushButton(this);
	existingButton->setText("Existing");
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
	nameCollection->setText("Name Scene Collection");
	layout->addWidget(nameCollection);

	_nameField = new QLineEdit(this);
	_nameField->setPlaceholderText("Name");
	layout->addWidget(_nameField);
	connect(_nameField, &QLineEdit::textChanged,
		[this](const QString text) {
			_proceedButton->setEnabled(text.length() > 0);
		});

	QWidget *spacer = new QWidget(this);
	spacer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

	_proceedButton = new QPushButton(this);
	_proceedButton->setText("Proceed");
	_proceedButton->setDisabled(true);
	connect(_proceedButton, &QPushButton::released, this, [this]() {
		QString qName = _nameField->text();
		emit proceedPressed(qName.toStdString());
	});
	layout->addWidget(_proceedButton);
}

AudioVideoSetup::AudioVideoSetup(QWidget *parent, std::string name,
				 std::string thumbnailPath)
{
	std::string imageBaseDir =
		obs_get_module_data_path(obs_current_module());
	imageBaseDir += "/images/";

	QVBoxLayout *layout = new QVBoxLayout(this);
	StreamPackageHeader *header = new StreamPackageHeader(parent, name, "");
	layout->addWidget(header);

	QHBoxLayout *inputs = new QHBoxLayout(this);
	QVBoxLayout *leftLayout = new QVBoxLayout(this);
	QLabel *videoDeviceLabel = new QLabel(this);
	videoDeviceLabel->setText("Video Device");
	_videoSources = new QComboBox(this);

	_videoPreview = new OBSQTDisplay(this);
	_videoPreview->setMinimumHeight(200);
	_videoPreview->hide();

	QWidget *spacerLeft = new QWidget(this);
	spacerLeft->setSizePolicy(QSizePolicy::Preferred,
				  QSizePolicy::Expanding);
	leftLayout->addWidget(videoDeviceLabel);
	auto videoSettings = new QHBoxLayout(this);
	videoSettings->addWidget(_videoSources);

	auto configButton = new QPushButton(this);
	std::string settingsIconPath = imageBaseDir + "icon-settings.svg";
	QIcon settingsIcon = QIcon();
	settingsIcon.addFile(settingsIconPath.c_str(), QSize(), QIcon::Normal,
			     QIcon::Off);
	configButton->setIcon(settingsIcon);
	configButton->setIconSize(QSize(22, 22));
	configButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	configButton->setStyleSheet("QPushButton {background: transparent; }");
	connect(configButton, &QPushButton::released, this, [this]() {
		obs_frontend_open_source_properties(_videoCaptureSource);
	});
	videoSettings->addWidget(configButton);

	leftLayout->addLayout(videoSettings);
	leftLayout->addWidget(_videoPreview);
	leftLayout->addWidget(spacerLeft);

	QVBoxLayout *rightLayout = new QVBoxLayout(this);
	QLabel *audioDeviceLabel = new QLabel(this);
	audioDeviceLabel->setText("Audio Device");
	_audioSources = new QComboBox(this);

	_levelsWidget = new SimpleVolumeMeter(this, _volmeter);
	// Add Dropdown and meter
	QWidget *spacerRight = new QWidget(this);
	spacerRight->setSizePolicy(QSizePolicy::Preferred,
				   QSizePolicy::Expanding);
	QPushButton *proceedButton = new QPushButton(this);
	proceedButton->setText("Proceed");

	connect(proceedButton, &QPushButton::released, this, [this]() {
		obs_data_t *vSettings =
			obs_source_get_settings(_videoCaptureSource);
		std::string vJson = obs_data_get_json(vSettings);
		obs_data_release(vSettings);
		obs_data_t *aSettings =
			obs_source_get_settings(_audioCaptureSource);
		std::string aJson = obs_data_get_json(aSettings);
		emit proceedPressed(vJson, aJson);
	});

	rightLayout->addWidget(audioDeviceLabel);
	rightLayout->addWidget(_audioSources);
	rightLayout->addWidget(_levelsWidget);
	rightLayout->addWidget(spacerRight);
	rightLayout->addWidget(proceedButton);

	inputs->addLayout(leftLayout, 50);
	inputs->addLayout(rightLayout, 50);
	layout->addLayout(inputs);
	_setupTempSources();
	SetupVolMeter();

	connect(_videoSources, &QComboBox::currentIndexChanged, this,
		[this](int index) {
			auto vSettings = obs_data_create();
			std::string id = _videoSourceIds[index];
			obs_data_set_string(vSettings, "video_device_id",
					    id.c_str());
			obs_source_reset_settings(_videoCaptureSource,
						  vSettings);
			obs_data_release(vSettings);
		});

	connect(_audioSources, &QComboBox::currentIndexChanged, this,
		[this](int index) {
			obs_data_t *aSettings =
				obs_source_get_settings(_audioCaptureSource);
			std::string id = _audioSourceIds[index];
			obs_data_set_string(aSettings, "device_id", id.c_str());
			obs_source_update(_audioCaptureSource, aSettings);
			obs_data_release(aSettings);
		});
}

void AudioVideoSetup::_setupTempSources()
{
	// Get settings
	config_t *const global_config = obs_frontend_get_global_config();
	config_set_default_string(global_config, "ElgatoCloud",
				  "DefaultVideoCaptureSettings", "");
	config_set_default_string(global_config, "ElgatoCloud",
				  "DefaultAudioCaptureSettings", "");

	std::string videoSettingsJson = config_get_string(
		global_config, "ElgatoCloud", "DefaultVideoCaptureSettings");
	std::string audioSettingsJson = config_get_string(
		global_config, "ElgatoCloud", "DefaultAudioCaptureSettings");

	obs_data_t *videoSettings =
		videoSettingsJson != ""
			? obs_data_create_from_json(videoSettingsJson.c_str())
			: nullptr;
	obs_data_t *audioSettings =
		audioSettingsJson != ""
			? obs_data_create_from_json(audioSettingsJson.c_str())
			: nullptr;

	const char *videoSourceId = "dshow_input";
	const char *vId = obs_get_latest_input_type_id(videoSourceId);
	_videoCaptureSource = obs_source_create_private(
		vId, "elgato-cloud-video-config", videoSettings);

	obs_properties_t *vProps = obs_source_properties(_videoCaptureSource);
	obs_property_t *vDevices =
		obs_properties_get(vProps, "video_device_id");
	for (size_t i = 0; i < obs_property_list_item_count(vDevices); i++) {
		std::string name = obs_property_list_item_name(vDevices, i);
		std::string id = obs_property_list_item_string(vDevices, i);
		_videoSourceIds.push_back(id);
		_videoSources->addItem(name.c_str());
	}
	obs_properties_destroy(vProps);

	obs_data_t *vSettings = obs_source_get_settings(_videoCaptureSource);
	std::string vDevice = obs_data_get_string(vSettings, "video_device_id");
	if (vDevice != "") {
		auto it = std::find(_videoSourceIds.begin(),
				    _videoSourceIds.end(), vDevice);
		if (it != _videoSourceIds.end()) {
			_videoSources->setCurrentIndex(
				static_cast<int>(it - _videoSourceIds.begin()));
		}
	}
	obs_data_release(vSettings);

	auto addDrawCallback = [this]() {
		obs_display_add_draw_callback(_videoPreview->GetDisplay(),
					      AudioVideoSetup::DrawVideoPreview,
					      this);
	};
	connect(_videoPreview, &OBSQTDisplay::DisplayCreated, addDrawCallback);
	_videoPreview->show();
	signal_handler_t *videoSigHandler =
		obs_source_get_signal_handler(_videoCaptureSource);
	signal_handler_connect_ref(videoSigHandler, "update",
				   AudioVideoSetup::DefaultVideoUpdated, this);
	obs_data_release(videoSettings);

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
	signal_handler_t *audioSigHandler =
		obs_source_get_signal_handler(_audioCaptureSource);
	signal_handler_connect_ref(audioSigHandler, "update",
				   AudioVideoSetup::DefaultAudioUpdated, this);
	obs_data_release(audioSettings);
}

void AudioVideoSetup::SetupVolMeter()
{
	obs_log(LOG_INFO, "SetupVolMeter");
	if (_volmeter) {
		obs_volmeter_remove_callback(
			_volmeter, AudioVideoSetup::OBSVolumeLevel, this);
		obs_volmeter_destroy(_volmeter);
		_volmeter = nullptr;
	}
	_volmeter = obs_volmeter_create(OBS_FADER_LOG);
	obs_volmeter_attach_source(_volmeter, _audioCaptureSource);
	obs_volmeter_add_callback(_volmeter, AudioVideoSetup::OBSVolumeLevel,
				  this);
}

void AudioVideoSetup::DefaultVideoUpdated(void *data, calldata_t *params)
{
	UNUSED_PARAMETER(params);
	UNUSED_PARAMETER(data);
	// auto config = static_cast<AudioVideoSetup*>(data);
	// config_t *const global_config = obs_frontend_get_global_config();
	// auto settings = obs_source_get_settings(config->_video_capture_source);
	// auto dataStr = obs_data_get_json(settings);
	// config_set_string(global_config, "ElgatoCloud", "DefaultVideoCaptureSettings", dataStr);

	// obs_data_release(settings);
}

void AudioVideoSetup::DefaultAudioUpdated(void *data, calldata_t *params)
{
	UNUSED_PARAMETER(params);
	auto config = static_cast<AudioVideoSetup *>(data);
	config->SetupVolMeter();
	// auto config = static_cast<AudioVideoSetup*>(data);
	// config_t *const global_config = obs_frontend_get_global_config();
	// auto settings = obs_source_get_settings(config->_audio_capture_source);
	// auto dataStr = obs_data_get_json(settings);
	// config_set_string(global_config, "ElgatoCloud", "DefaultAudioCaptureSettings", dataStr);

	// obs_data_release(settings);
}

void AudioVideoSetup::OpenConfigVideoSource()
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

void AudioVideoSetup::OpenConfigAudioSource()
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

void AudioVideoSetup::OBSVolumeLevel(void *data,
				     const float magnitude[MAX_AUDIO_CHANNELS],
				     const float peak[MAX_AUDIO_CHANNELS],
				     const float inputPeak[MAX_AUDIO_CHANNELS])
{
	UNUSED_PARAMETER(peak);
	UNUSED_PARAMETER(inputPeak);
	obs_log(LOG_INFO, "OBS Volume Level");
	auto config = static_cast<AudioVideoSetup *>(data);
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

void AudioVideoSetup::DrawVideoPreview(void *data, uint32_t cx, uint32_t cy)
{
	auto config = static_cast<AudioVideoSetup *>(data);

	if (!config->_videoCaptureSource)
		return;

	uint32_t sourceCX =
		std::max(obs_source_get_width(config->_videoCaptureSource), 1u);
	uint32_t sourceCY = std::max(
		obs_source_get_height(config->_videoCaptureSource), 1u);

	int x, y;
	int newCX, newCY;
	float scale;

	GetScaleAndCenterPos(sourceCX, sourceCY, cx, cy, x, y, scale);

	newCX = int(scale * float(sourceCX));
	newCY = int(scale * float(sourceCY));

	gs_viewport_push();
	gs_projection_push();
	const bool previous = gs_set_linear_srgb(true);

	gs_ortho(0.0f, float(sourceCX), 0.0f, float(sourceCY), -100.0f, 100.0f);
	gs_set_viewport(x, y, newCX, newCY);
	obs_source_video_render(config->_videoCaptureSource);

	gs_set_linear_srgb(previous);
	gs_projection_pop();
	gs_viewport_pop();
}

AudioVideoSetup::~AudioVideoSetup()
{
	obs_display_remove_draw_callback(_videoPreview->GetDisplay(),
					 AudioVideoSetup::DrawVideoPreview,
					 this);
	if (_videoCaptureSource) {
		obs_source_release(_videoCaptureSource);
		obs_source_remove(_videoCaptureSource);
		signal_handler_t *videoSigHandler =
			obs_source_get_signal_handler(_videoCaptureSource);
		signal_handler_disconnect(videoSigHandler, "update",
					  AudioVideoSetup::DefaultVideoUpdated,
					  this);
	}
	if (_volmeter) {
		obs_volmeter_detach_source(_volmeter);
		obs_volmeter_remove_callback(
			_volmeter, AudioVideoSetup::OBSVolumeLevel, this);
		obs_volmeter_destroy(_volmeter);
	}
	if (_audioCaptureSource) {
		obs_source_release(_audioCaptureSource);
		obs_source_remove(_audioCaptureSource);
		signal_handler_t *audioSigHandler =
			obs_source_get_signal_handler(_audioCaptureSource);
		signal_handler_disconnect(audioSigHandler, "update",
					  AudioVideoSetup::DefaultAudioUpdated,
					  this);
	}
}

StreamPackageSetupWizard::StreamPackageSetupWizard(QWidget *parent,
						   ElgatoProduct *product,
						   std::string filename)
	: QDialog(parent),
	  _thumbnailPath(product->thumbnailPath),
	  _productName(product->name),
	  _filename(filename)
{
	setupWizard = this;

	QVBoxLayout *layout = new QVBoxLayout(this);
	_steps = new QStackedWidget(this);

	// Step 1- select a new or existing install (step index: 0)
	InstallType *installerType =
		new InstallType(this, _productName, _thumbnailPath);
	connect(installerType, &InstallType::newCollectionPressed, this,
		[this]() {
			_setup.installType = InstallTypes::NewCollection;
			_steps->setCurrentIndex(1);
		});
	connect(installerType, &InstallType::existingCollectionPressed, this,
		[this]() {
			_setup.installType = InstallTypes::AddToCollection;
			_steps->setCurrentIndex(2);
		});
	_steps->addWidget(installerType);

	// Step 2a- Provide a name for the new collection (step index: 1)
	auto *newName =
		new NewCollectionName(this, _productName, _thumbnailPath);
	connect(newName, &NewCollectionName::proceedPressed, this,
		[this](std::string name) {
			_setup.collectionName = name;
			_steps->setCurrentIndex(3);
		});
	_steps->addWidget(newName);

	// Step 2b- Select an existing collection (step index: 2)
	_steps->addWidget(new QWidget(this));

	// Step 3- Set up A/V source (step index: 3)
	auto *avSetup = new AudioVideoSetup(this, _productName, _thumbnailPath);
	_steps->addWidget(avSetup);
	connect(avSetup, &AudioVideoSetup::proceedPressed, this,
		[this](std::string vSettings, std::string aSettings) {
			_setup.videoSettings = vSettings;
			_setup.audioSettings = aSettings;
			install();
		});

	layout->addWidget(_steps);
}

StreamPackageSetupWizard::~StreamPackageSetupWizard()
{
	setupWizard = nullptr;
}

void StreamPackageSetupWizard::install()
{
	obs_log(LOG_INFO, "Elgato Req File: %s", _filename.c_str());

	// TODO: Clean up this mess of setting up the pack install path.
	auto path = obs_module_config_path("packs");
	os_mkdirs(path);
	auto abs_path = os_get_abs_path_ptr(path);
	std::string abs_path_str = std::string(abs_path);
	replace_all(abs_path_str, std::string(":\\\\"), std::string(":/"));
	std::replace(abs_path_str.begin(), abs_path_str.end(), '\\', '/');
	std::string packDirName(_setup.collectionName);
	std::replace(packDirName.begin(), packDirName.end(), ' ', '_');
	std::string pack_path = abs_path_str + "/" + packDirName;
	bfree(path);
	bfree(abs_path);
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
