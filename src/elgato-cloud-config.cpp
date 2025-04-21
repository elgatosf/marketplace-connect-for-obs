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

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>

#include "elgato-styles.hpp"
#include "elgato-widgets.hpp"
#include "elgato-cloud-config.hpp"
#include "elgato-cloud-data.hpp"
#include "util.h"
#include <plugin-support.h>
#include "obs-utils.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QPainterPath>
#include <QApplication>
#include <QThread>
#include <QMetaObject>
#include <QRect>
#include <QPainter>
#include <QFileDialog>
#include <QLinearGradient>
#include <util/platform.h>

namespace elgatocloud {

DefaultAVWidget *AVWIDGET = nullptr;
ElgatoCloudConfig *configWindow = nullptr;

DefaultAVWidget::DefaultAVWidget(QWidget *parent) : QWidget(parent)
{
	AVWIDGET = this;
	std::string imageBaseDir = GetDataPath();
	imageBaseDir += "/images/";
	auto layout = new QHBoxLayout();
	layout->setSpacing(16);
	layout->setContentsMargins(0, 0, 0, 0);

	auto dropDowns = new QVBoxLayout();
	dropDowns->setSpacing(16);
	dropDowns->setContentsMargins(0, 0, 0, 0);

	auto avWidgetTitle = new QLabel(obs_module_text("MarketplaceWindow.Settings.DefaultAV.Label"), this);
	avWidgetTitle->setStyleSheet(EWizardStepTitle);

	auto videoSourceLabel = new QLabel(
		obs_module_text("MarketplaceWindow.Settings.DefaultVideoDevice.Label"), this);
	videoSourceLabel->setStyleSheet(EWizardFieldLabel);

	auto audioSourceLabel = new QLabel(
		obs_module_text("MarketplaceWindow.Settings.DefaultAudioDevice.Label"), this);
	audioSourceLabel->setStyleSheet(EWizardFieldLabel);

	_videoSources = new QComboBox(this);
	_videoSources->setStyleSheet(EWizardComboBoxStyle);

	_videoPreview = new OBSQTDisplay(this);
	auto videoPreviewWidget = new VideoPreviewWidget(_videoPreview, 8, this);
	videoPreviewWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	_blank = new CameraPlaceholder(8, this);
	std::string cameraIconPath = imageBaseDir + "camera-placeholder-icon.svg";
	_blank->setIcon(cameraIconPath.c_str());
	_blank->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	auto videoSettings = new QWidget(this);
	auto vsLayout = new QHBoxLayout(this);
	vsLayout->setContentsMargins(0, 0, 0, 0);

	vsLayout->addWidget(_videoSources);

	_settingsButton = new QPushButton(this);
	std::string settingsIconPath = imageBaseDir + "button-settings-icon.svg";
	std::string settingsIconDisabledPath = imageBaseDir + "button-settings-icon-disabled.svg";
	QString settingsButtonStyle = EWizardIconOnlyButtonStyle;
	settingsButtonStyle.replace("${img}", settingsIconPath.c_str());
	settingsButtonStyle.replace("${img-disabled}", settingsIconDisabledPath.c_str());
	_settingsButton->setFixedSize(32, 32);
	_settingsButton->setMaximumHeight(32);
	_settingsButton->setStyleSheet(settingsButtonStyle);
	_settingsButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	_settingsButton->setToolTip(
		obs_module_text("MarketplaceWindow.Settings.DefaultVideoDevice.SettingsButton.Tooltip"));
	connect(_settingsButton, &QPushButton::released, this, [this]() {
		obs_frontend_open_source_properties(_videoCaptureSource);
	});
	vsLayout->addWidget(_settingsButton);
	videoSettings->setLayout(vsLayout);
	videoSettings->setSizePolicy(QSizePolicy::Preferred,
				     QSizePolicy::Preferred);

	_audioSources = new QComboBox(this);
	_audioSources->setStyleSheet(EWizardComboBoxStyle);

	_levelsWidget = new SimpleVolumeMeter(this, _volmeter);
	// Add Dropdown and meter

	dropDowns->addWidget(avWidgetTitle);
	auto videoSourceSettingLayout = new QVBoxLayout();
	videoSourceSettingLayout->setContentsMargins(0, 0, 0, 0);
	videoSourceSettingLayout->setSpacing(0);
	videoSourceSettingLayout->addWidget(videoSourceLabel);
	videoSourceSettingLayout->addWidget(videoSettings);
	dropDowns->addLayout(videoSourceSettingLayout);

	auto audioSourceSettingLayout = new QVBoxLayout();
	audioSourceSettingLayout->setContentsMargins(0, 0, 0, 0);
	audioSourceSettingLayout->setSpacing(0);
	audioSourceSettingLayout->addWidget(audioSourceLabel);
	audioSourceSettingLayout->addWidget(_audioSources);

	dropDowns->addLayout(audioSourceSettingLayout);
	dropDowns->addStretch();
	//dropDowns->addWidget(_levelsWidget);

	// Get settings
	auto config = elgatoCloud->GetConfig();

	std::string audioSettingsJson =
		obs_data_get_string(config, "DefaultAudioCaptureSettings");

	obs_data_t *audioSettings =
		audioSettingsJson != ""
			? obs_data_create_from_json(audioSettingsJson.c_str())
			: nullptr;

	_audioCaptureSource = nullptr;
	_setupTempAudioSource(audioSettings);
	SetupVolMeter();

	obs_data_release(audioSettings);

	connect(_audioSources, &QComboBox::currentIndexChanged, this, [this](int index) {
		obs_data_t *aSettings =
			obs_source_get_settings(_audioCaptureSource);
		std::string id = _audioSourceIds[index];
		obs_data_set_string(aSettings, "device_id", id.c_str());

		if (_volmeter) {
			obs_volmeter_remove_callback(
				_volmeter, DefaultAVWidget::OBSVolumeLevel,
				this);
			obs_volmeter_destroy(_volmeter);
			_volmeter = nullptr;
		}

		// For some reason, we need to completely deconstruct the temporary
		// audio capture source in order to connect the new device vol meter
		// to the volume meter in the UI. It should be possible to inject
		// the newly selected device, and re-connect the _volmeter callbacks
		_setupTempAudioSource(aSettings);
		obs_data_release(aSettings);
		SetupVolMeter();
	});

	dropDowns->addStretch();

	//avSettings->addWidget(_videoPreview);
	_stack = new QStackedWidget(this);
	_stack->setFixedWidth(300);
	_stack->addWidget(_blank);
	_stack->addWidget(videoPreviewWidget);
	_stack->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
	auto previewsLayout = new QVBoxLayout();
	previewsLayout->setContentsMargins(0, 0, 0, 0);
	previewsLayout->setSpacing(8);
	previewsLayout->addWidget(_stack);
	previewsLayout->addWidget(_levelsWidget);
	previewsLayout->addStretch();
	layout->addLayout(dropDowns);
	layout->addLayout(previewsLayout);

	setLayout(layout);

	std::string videoSettingsJson =
		obs_data_get_string(config, "DefaultVideoCaptureSettings");

	obs_data_t *videoData =
		videoSettingsJson != ""
			? obs_data_create_from_json(videoSettingsJson.c_str())
			: nullptr;

	_setupTempVideoSource(videoData);

	if (_videoSources->currentIndex() > 0) {
		_settingsButton->setDisabled(false);
		_noneSelected = false;
		_stack->setCurrentIndex(1);
	} else {
		_settingsButton->setDisabled(true);
		_noneSelected = true;
		_stack->setCurrentIndex(0);
	}

	obs_data_release(videoData);
	obs_data_release(config);

	connect(_videoSources, &QComboBox::currentIndexChanged, this,
		[this](int index) {
			if (index > 0) {
				_settingsButton->setDisabled(false);
				auto vSettings = obs_data_create();
				std::string id = _videoSourceIds[index];
				obs_data_set_string(vSettings,
						    "video_device_id",
						    id.c_str());
				obs_source_reset_settings(_videoCaptureSource,
							  vSettings);
				obs_data_release(vSettings);
				_noneSelected = false;
				_stack->setCurrentIndex(1);
			} else {
				_settingsButton->setDisabled(true);
				_noneSelected = true;
				_stack->setCurrentIndex(0);
			}
		});
}

DefaultAVWidget::~DefaultAVWidget()
{
	AVWIDGET = nullptr;
	if (_volmeter) {
		obs_volmeter_detach_source(_volmeter);
		obs_volmeter_remove_callback(
			_volmeter, DefaultAVWidget::OBSVolumeLevel, this);
		obs_volmeter_destroy(_volmeter);
	}
	if (_audioCaptureSource) {
		obs_source_release(_audioCaptureSource);
		obs_source_remove(_audioCaptureSource);
		signal_handler_t *audioSigHandler =
			obs_source_get_signal_handler(_audioCaptureSource);
		signal_handler_disconnect(audioSigHandler, "update",
					  DefaultAVWidget::DefaultAudioUpdated,
					  this);
	}
	if (_videoCaptureSource) {
		obs_source_release(_videoCaptureSource);
	}
	_levelsWidget = nullptr;
}

void DefaultAVWidget::_setupTempAudioSource(obs_data_t *audioSettings)
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
	if (_audioSourceIds.size() == 0) {
		for (size_t i = 0; i < obs_property_list_item_count(aDevices);
		     i++) {
			std::string name =
				obs_property_list_item_name(aDevices, i);
			std::string id =
				obs_property_list_item_string(aDevices, i);
			_audioSourceIds.push_back(id);
			_audioSources->addItem(name.c_str());
		}
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
}

void DefaultAVWidget::save()
{
	auto config = elgatoCloud->GetConfig();
	auto settings = obs_source_get_settings(_audioCaptureSource);
	auto dataStr = obs_data_get_json(settings);
	obs_data_set_string(config, "DefaultAudioCaptureSettings", dataStr);

	if (!_noneSelected) {
		auto vSettings = obs_source_get_settings(_videoCaptureSource);
		auto vDataStr = obs_data_get_json(vSettings);
		obs_data_set_string(config, "DefaultVideoCaptureSettings",
				    vDataStr);
		obs_data_release(vSettings);
	} else {
		obs_data_set_string(config, "DefaultVideoCaptureSettings", "");
	}
	obs_data_release(settings);
	obs_data_release(config);
}

void DefaultAVWidget::SetupVolMeter()
{
	_volmeter = obs_volmeter_create(OBS_FADER_LOG);
	obs_volmeter_attach_source(_volmeter, _audioCaptureSource);
	obs_volmeter_add_callback(_volmeter, DefaultAVWidget::OBSVolumeLevel,
				  this);
}

void DefaultAVWidget::DefaultAudioUpdated(void *data, calldata_t *params)
{
	UNUSED_PARAMETER(params);
	auto config = static_cast<DefaultAVWidget *>(data);
	config->SetupVolMeter();
}

void DefaultAVWidget::OpenConfigAudioSource()
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
	}
	obs_properties_destroy(props);
}

void DefaultAVWidget::OBSVolumeLevel(void *data,
				     const float magnitude[MAX_AUDIO_CHANNELS],
				     const float peak[MAX_AUDIO_CHANNELS],
				     const float inputPeak[MAX_AUDIO_CHANNELS])
{
	UNUSED_PARAMETER(peak);
	UNUSED_PARAMETER(inputPeak);
	auto config = static_cast<DefaultAVWidget *>(data);
	float mag = magnitude[0];
	float pk = peak[0];
	float ip = inputPeak[0];
	config->_levelsWidget->setLevel(mag, pk, ip);
	QMetaObject::invokeMethod(QCoreApplication::instance()->thread(),
				  [config]() {
					  // TODO: Use proper signals to stop this update from occuring.
					  if (!AVWIDGET) {
						  return;
					  }
					  config->_levelsWidget->update();
				  });
}

void DefaultAVWidget::_setupTempVideoSource(obs_data_t *videoSettings)
{
	const char *videoSourceId = "dshow_input";
	const char *vId = obs_get_latest_input_type_id(videoSourceId);
	_videoCaptureSource = obs_source_create_private(
		vId, "elgato-cloud-video-config", videoSettings);

	obs_properties_t *vProps = obs_source_properties(_videoCaptureSource);
	obs_property_t *vDevices =
		obs_properties_get(vProps, "video_device_id");
	_videoSources->addItem("None");
	_videoSourceIds.push_back("NONE");
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
		obs_display_add_draw_callback(
			_videoPreview->GetDisplay(),
			VideoCaptureSourceSelector::DrawVideoPreview, this);
	};
	connect(_videoPreview, &OBSQTDisplay::DisplayCreated, addDrawCallback);
}

SimpleVolumeMeter::SimpleVolumeMeter(QWidget *parent, obs_volmeter_t *volmeter)
	: QWidget(parent),
	  _volmeter(volmeter)
{
	setFixedHeight(8);
}

SimpleVolumeMeter::~SimpleVolumeMeter() {}

void SimpleVolumeMeter::setLevel(float magnitude, float peak, float inputPeak)
{
	uint64_t ts = os_gettime_ns();
	std::unique_lock l(_lock);
	_currentMag = magnitude;
	_currentPeak = peak;
	_currentInputPeak = inputPeak;
	l.unlock();
	calculateDisplayPeak(ts);
}

void SimpleVolumeMeter::calculateDisplayPeak(uint64_t ts)
{
	float deltaT = float(ts - _lastRedraw) * 0.000000001;
	if (_currentPeak > _displayPeak || isnan(_displayPeak)) {
		_displayPeak = _currentPeak;
	} else {
		float decay = deltaT * _decayRate;
		_displayPeak = std::max(_displayPeak - decay, _minMag);
	}
}

void SimpleVolumeMeter::paintEvent(QPaintEvent *event)
{
	UNUSED_PARAMETER(event);
	uint64_t ts = os_gettime_ns();
	calculateDisplayPeak(ts);

	QRect widgetRect = rect();
	QRect bgRect = rect();
	int width = widgetRect.width();
	float fwidth = static_cast<float>(width) * (_displayPeak - _minMag) /
		       (-_minMag);
	int newWidth = static_cast<int>(fwidth);
	widgetRect.setWidth(std::min(width, newWidth));

	QPainter painter(this);

	QPainterPath path;
	path.addRoundedRect(bgRect, 4, 4);
	painter.setClipPath(path);

	// Draw background
	painter.fillRect(bgRect, QColor(49, 49, 49));
	QLinearGradient gradient(0, 0, width, 0);
	gradient.setColorAt(0.0, QColor(59, 180, 85));
	gradient.setColorAt(0.6, QColor(59, 180, 85));
	gradient.setColorAt(0.8, QColor(251, 219, 0));
	gradient.setColorAt(1.0, QColor(255, 60, 78));
	path.clear();
	path.addRoundedRect(widgetRect, 4, 4);
	painter.setClipPath(path);

	painter.fillRect(bgRect, gradient);

	_lastRedraw = ts;
}

ElgatoCloudConfig::ElgatoCloudConfig(QWidget *parent) : QDialog(parent)
{

	// Disable all video capture sources so that single-thread
	// capture sources, such as the Elgato Facecam, can be properly
	// selected in the wizard.  Will re-enable any disabled sources
	// in the wizard destructor.
	obs_enum_sources(
		&ElgatoCloudConfig::
		DisableVideoCaptureSources,
		this);

	configWindow = this;
	std::string imageBaseDir = GetDataPath();
	imageBaseDir += "/images/";

	setFixedSize(QSize(680, 528));
	setAttribute(Qt::WA_DeleteOnClose);
	setWindowTitle(obs_module_text("MarketplaceWindow.Settings.Title"));

	auto layout = new QVBoxLayout();
	layout->setSpacing(16);
	layout->setContentsMargins(16, 16, 16, 16);

	// Default video and audio capture device settings
	_avWidget = new DefaultAVWidget(this);
	layout->addWidget(_avWidget);

	// Create the horizontal line
	QFrame* line = new QFrame();
	line->setFrameShape(QFrame::HLine);
	line->setFrameShadow(QFrame::Plain);  // No 3D effect
	line->setLineWidth(1);
	line->setFixedHeight(1);  // Force it to stay 1px high
	line->setStyleSheet("color: rgba(255, 255, 255, 0.12); rgba(255, 255, 255, 0.12);");

	layout->addWidget(line);

	auto advancedTitle = new QLabel(obs_module_text("MarketplaceWindow.Settings.Advanced"), this);
	advancedTitle->setStyleSheet(EWizardStepTitle);

	layout->addWidget(advancedTitle);

	// Theme installation location setting
	auto filePickerLabel = new QLabel(
		obs_module_text("MarketplaceWindow.Settings.InstallLocation"), this);
	filePickerLabel->setStyleSheet(EWizardFieldLabel);

	auto config = elgatoCloud->GetConfig();
	_installDirectory = obs_data_get_string(config, "InstallLocation");

	auto filePicker = new QWidget(this);
	filePicker->setStyleSheet(
		"QWidget {background-color: #232323; border-radius: 8px; margin: 0px 0px 0px 0px; padding: 0px 16px 0px 0px;}");
	auto fpLayout = new QHBoxLayout();
	fpLayout->setSpacing(0);
	fpLayout->setContentsMargins(0, 0, 0, 0);
	auto directory = new QLabel(_installDirectory.c_str(), filePicker);
	directory->setFixedHeight(32);
	directory->setStyleSheet(
		"QLabel {color: #FFFFFF; padding: 0px 16px 0px 16px; font-size: 14px;}");
	directory->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	auto directoryPick = new QPushButton(filePicker);
	directoryPick->setFixedHeight(24);
	directoryPick->setFixedWidth(40);

	connect(directoryPick, &QPushButton::released, this,
		[this, directory]() {
			QFileDialog *dialog =
				new QFileDialog(this, 
						obs_module_text("MarketplaceWindow.Settings.InstallLocation"),
						_installDirectory.c_str());
			dialog->setFileMode(QFileDialog::Directory);
			//dialog->setAttribute(Qt::WA_DeleteOnClose);
			if (dialog->exec()) {
				auto files = dialog->selectedFiles();
				_installDirectory = files[0].toStdString();
				directory->setText(_installDirectory.c_str());
			}
		});

	std::string openIconPath = imageBaseDir + "open-file-picker.svg";
	std::string openIconHoverPath =
		imageBaseDir + "open-file-picker_hover.svg";

	QString buttonStyle = EInlineIconHoverButtonStyle;
	buttonStyle.replace("${img}", openIconPath.c_str());
	buttonStyle.replace("${hover-img}", openIconHoverPath.c_str());
	directoryPick->setStyleSheet(buttonStyle);
	directoryPick->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	fpLayout->addWidget(directory);
	fpLayout->addWidget(directoryPick);
	filePicker->setLayout(fpLayout);

	auto filePickerLayout = new QVBoxLayout();
	filePickerLayout->setContentsMargins(0, 0, 0, 0);
	filePickerLayout->setSpacing(4);

	filePickerLayout->addWidget(filePickerLabel);
	filePickerLayout->addWidget(filePicker);
	layout->addLayout(filePickerLayout);

	// Maker Tools toggle.
	bool makerTools = obs_data_get_bool(config, "MakerTools");
	_makerCheckbox = new QCheckBox(
		obs_module_text("MarketplaceWindow.Settings.EnableMakerTools"), this);
	_makerCheckbox->setChecked(makerTools);
	std::string checkedImage = imageBaseDir + "checkbox_checked.png";
	std::string uncheckedImage = imageBaseDir + "checkbox_unchecked.png";
	QString checkBoxStyle = EWizardCheckBoxStyle;
	checkBoxStyle.replace("${checked-img}", checkedImage.c_str());
	checkBoxStyle.replace("${unchecked-img}", uncheckedImage.c_str());
	_makerCheckbox->setStyleSheet(checkBoxStyle);
	_makerCheckbox->setToolTip(obs_module_text("MarketplaceWindow.Settings.EnableMakerTools.Tooltip"));
	auto makerToolsTip = new QLabel(obs_module_text("MarketplaceWindow.Settings.EnableMakerTools.Tip"), this);
	makerToolsTip->setStyleSheet(EWizardCheckBoxTipStyle);

	auto makerLayout = new QVBoxLayout();
	makerLayout->setContentsMargins(0, 0, 0, 0);
	makerLayout->setSpacing(4);
	makerLayout->addWidget(_makerCheckbox);
	makerLayout->addWidget(makerToolsTip);

	layout->addLayout(makerLayout);

	connect(_makerCheckbox, &QCheckBox::stateChanged, [this](int state) {
		bool makerTools = state == Qt::Checked;
		_makerRestartMsg->setVisible(makerTools != elgatoCloud->MakerToolsOnStart());
	});

	std::string restartTxt = elgatoCloud->MakerToolsOnStart() ? obs_module_text("MarketplaceWindow.Settings.MakerToolsRestartWarning.Disabled") : obs_module_text("MarketplaceWindow.Settings.MakerToolsRestartWarning.Enabled");
	//_makerRestartMsg = new QLabel(restartTxt.c_str(), this);
	//_makerRestartMsg->setVisible(false);
	//_makerRestartMsg->setStyleSheet(
	//	"QLabel {color: #FFFFFF; padding: 8px 16px 0px 16px; font-size: 12pt; font-style: italic; }");
	//_makerRestartMsg->setAlignment(Qt::AlignCenter);

	std::string infoIconPath = imageBaseDir + "info-icon.svg";
	_makerRestartMsg = new InfoLabel(restartTxt.c_str(), infoIconPath.c_str(), this);
	_makerRestartMsg->setVisible(makerTools != elgatoCloud->MakerToolsOnStart());
	layout->addWidget(_makerRestartMsg);
	layout->addStretch();

	std::string version = "";
	version += versionNoBuild() + releaseType() + " (" + buildNumber() +")";
	auto versionLabel = new QLabel(version.c_str(), this);
	versionLabel->setStyleSheet(EWizardSmallLabel);
	//layout->addWidget(versionLabel);

	auto buttons = new QHBoxLayout();

	QPushButton *cancelButton = new QPushButton(this);
	cancelButton->setText(
		obs_module_text("General.CancelButton"));
	cancelButton->setStyleSheet(EWizardQuietButtonStyle);

	QPushButton *saveButton = new QPushButton(this);
	saveButton->setText(
		obs_module_text("General.SaveButton"));
	saveButton->setStyleSheet(EWizardButtonStyle);

	buttons->addWidget(versionLabel);
	buttons->addStretch();
	buttons->addWidget(cancelButton);
	buttons->addWidget(saveButton);
	connect(saveButton, &QPushButton::released, this, [this]() {
		auto config = elgatoCloud->GetConfig();
		obs_data_set_string(config, "InstallLocation",
				    _installDirectory.c_str());
		obs_data_set_bool(config, "MakerTools",
				  _makerCheckbox->isChecked());
		obs_data_release(config);
		_save();
		close();
	});
	connect(cancelButton, &QPushButton::released, this,
		[this]() { close(); });

	layout->addLayout(buttons);
	setStyleSheet("background-color: #151515");
	setLayout(layout);
	obs_data_release(config);
}

bool ElgatoCloudConfig::DisableVideoCaptureSources(void* data,
	obs_source_t* source)
{
	auto config = static_cast<ElgatoCloudConfig*>(data);
	std::string sourceType = obs_source_get_id(source);
	if (sourceType == "dshow_input") {
		auto settings = obs_source_get_settings(source);
		bool active = obs_data_get_bool(settings, "active");
		obs_data_release(settings);
		if (active) {
			calldata_t cd = {};
			calldata_set_bool(&cd, "active", false);
			proc_handler_t* ph =
				obs_source_get_proc_handler(source);
			proc_handler_call(ph, "activate", &cd);
			calldata_free(&cd);
			std::string id = obs_source_get_uuid(source);
			config->_toEnable.push_back(id);
		}
	}
	return true;
}

bool ElgatoCloudConfig::EnableVideoCaptureSources(void* data,
	obs_source_t* source)
{
	auto config = static_cast<ElgatoCloudConfig*>(data);
	std::string sourceType = obs_source_get_id(source);
	if (sourceType == "dshow_input") {
		auto settings = obs_source_get_settings(source);
		std::string id = obs_source_get_uuid(source);
		obs_data_release(settings);
		if (std::find(config->_toEnable.begin(), config->_toEnable.end(), id) != config->_toEnable.end()) {
			calldata_t cd = {};
			calldata_set_bool(&cd, "active", true);
			proc_handler_t* ph =
				obs_source_get_proc_handler(source);
			proc_handler_call(ph, "activate", &cd);
			calldata_free(&cd);
		}
	}
	return true;
}

void ElgatoCloudConfig::OpenConfigVideoSource()
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
	}
	obs_properties_destroy(props);
}

void ElgatoCloudConfig::OpenConfigAudioSource()
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
	}
	obs_properties_destroy(props);
}

void ElgatoCloudConfig::OBSVolumeLevel(
	void *data, const float magnitude[MAX_AUDIO_CHANNELS],
	const float peak[MAX_AUDIO_CHANNELS],
	const float inputPeak[MAX_AUDIO_CHANNELS])
{
	UNUSED_PARAMETER(peak);
	UNUSED_PARAMETER(inputPeak);
	auto config = static_cast<ElgatoCloudConfig *>(data);
	if (!config || !config->_levelsWidget) {
		return;
	}
	float mag = magnitude[0];
	float pk = peak[0];
	float ip = inputPeak[0];
	config->_levelsWidget->setLevel(mag, pk, ip);
	QMetaObject::invokeMethod(
		QCoreApplication::instance()->thread(), [config]() {
			ElgatoCloud *ec = GetElgatoCloud();
			if (ec->window && config && config->_levelsWidget) {
				config->_levelsWidget->update();
			}
		});
}

void ElgatoCloudConfig::DrawVideoPreview(void *data, uint32_t cx, uint32_t cy)
{
	ElgatoCloudConfig *window = static_cast<ElgatoCloudConfig *>(data);

	if (!window->_videoCaptureSource)
		return;

	uint32_t sourceCX =
		std::max(obs_source_get_width(window->_videoCaptureSource), 1u);
	uint32_t sourceCY = std::max(
		obs_source_get_height(window->_videoCaptureSource), 1u);

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
	obs_source_video_render(window->_videoCaptureSource);

	gs_set_linear_srgb(previous);
	gs_projection_pop();
	gs_viewport_pop();
}

void ElgatoCloudConfig::_save()
{
	_avWidget->save();
	elgatoCloud->SaveConfig();
}

ElgatoCloudConfig::~ElgatoCloudConfig()
{
	if (!_videoCaptureSource) {
		calldata_t cd = {};
		calldata_set_bool(&cd, "active", false);
		proc_handler_t* ph =
			obs_source_get_proc_handler(_videoCaptureSource);
		proc_handler_call(ph, "activate", &cd);
		calldata_free(&cd);
	}

	obs_enum_sources(
		&ElgatoCloudConfig::
		EnableVideoCaptureSources,
		this);
	configWindow = nullptr;
}

ElgatoCloudConfig *openConfigWindow(QWidget *parent)
{
	ElgatoCloudConfig *window = nullptr;
	if (configWindow == nullptr) {
		window = new ElgatoCloudConfig(parent);
		window->show();
	} else {
		window = configWindow;
	}
	window->raise();
	window->activateWindow();
	return window;
}

} // namespace elgatocloud
