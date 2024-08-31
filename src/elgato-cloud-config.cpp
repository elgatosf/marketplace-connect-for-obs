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

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>

#include "elgato-cloud-config.hpp"
#include "elgato-cloud-data.hpp"
#include "plugin-support.h"
#include "obs-utils.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <algorithm>
#include <QApplication>
#include <QThread>
#include <QMetaObject>
#include <QRect>
#include <QPainter>
#include <QLinearGradient>
#include <util/platform.h>

namespace elgatocloud {

SimpleVolumeMeter::SimpleVolumeMeter(QWidget *parent, obs_volmeter_t *volmeter)
	: QWidget(parent),
	  _volmeter(volmeter)
{
	setFixedHeight(16);
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

	painter.fillRect(bgRect, QColor(50, 50, 50));

	QLinearGradient gradient(0, 0, width, 0);
	gradient.setColorAt(0.0, QColor(86, 69, 255));
	gradient.setColorAt(1.0, QColor(129, 60, 255));
	painter.fillRect(widgetRect, gradient);

	_lastRedraw = ts;
}

ElgatoCloudConfig::ElgatoCloudConfig(QWidget *parent) : QWidget(parent)
{
	std::string imageBaseDir =
		obs_get_module_data_path(obs_current_module());
	imageBaseDir += "/images/";

	auto layout = new QVBoxLayout();
	auto toolbar = new QHBoxLayout();

	auto title = new QLabel(this);
	title->setText("Elgato Cloud - Settings");
	title->setStyleSheet("QLabel { font-size: 18pt; }");
	title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	std::string closeIconPath = imageBaseDir + "circle-xmark-regular.svg";
	QIcon closeIcon = QIcon();
	closeIcon.addFile(closeIconPath.c_str(), QSize(), QIcon::Normal,
			  QIcon::Off);
	auto closeButton = new QPushButton(this);
	closeButton->setIcon(closeIcon);
	closeButton->setIconSize(QSize(32, 32));
	closeButton->setStyleSheet("QPushButton {background: transparent; }");
	closeButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	connect(closeButton, &QPushButton::pressed, this,
		[this]() { emit closeClicked(); });
	toolbar->addWidget(title);
	toolbar->addWidget(closeButton);

	auto label = new QLabel(this);
	auto configVideoButton = new QPushButton(this);

	_videoPreview = new OBSQTDisplay(this);
	_videoPreview->setMinimumHeight(200);

	label->setText("Default Video Device");
	label->setStyleSheet("font-size: 18pt;");
	configVideoButton->setText("Configure Video Device");

	connect(configVideoButton, &QPushButton::clicked,
		[this]() { OpenConfigVideoSource(); });

	layout->addLayout(toolbar);
	layout->addWidget(label);
	layout->addWidget(_videoPreview);
	_videoPreview->hide();
	layout->addWidget(configVideoButton);

	auto micLabel = new QLabel(this);
	micLabel->setText("Default Audio Input");
	micLabel->setStyleSheet("font-size:18pt;");
	_levelsWidget = new SimpleVolumeMeter(this, _volmeter);
	_levelsWidget->hide();
	layout->addWidget(micLabel);
	layout->addWidget(_levelsWidget);
	auto configMicButton = new QPushButton(this);
	configMicButton->setText("Configure Audio Input");

	connect(configMicButton, &QPushButton::clicked,
		[this]() { OpenConfigAudioSource(); });

	layout->addWidget(configMicButton);

	auto spacer = new QWidget(this);
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	layout->addWidget(spacer);
	setLayout(layout);
	_SetupTmpSources();
}

void ElgatoCloudConfig::_SetupTmpSources()
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
	auto addDrawCallback = [this]() {
		obs_display_add_draw_callback(
			_videoPreview->GetDisplay(),
			ElgatoCloudConfig::DrawVideoPreview, this);
	};
	connect(_videoPreview, &OBSQTDisplay::DisplayCreated, addDrawCallback);
	_videoPreview->show();
	signal_handler_t *videoSigHandler =
		obs_source_get_signal_handler(_videoCaptureSource);
	signal_handler_connect_ref(videoSigHandler, "update",
				   ElgatoCloudConfig::DefaultVideoUpdated,
				   this);
	obs_data_release(videoSettings);

	const char *audioSourceId = "wasapi_input_capture";
	const char *aId = obs_get_latest_input_type_id(audioSourceId);
	_audioCaptureSource = obs_source_create_private(
		aId, "elgato-cloud-audio-config", audioSettings);
	_volmeter = obs_volmeter_create(OBS_FADER_LOG);
	obs_volmeter_attach_source(_volmeter, _audioCaptureSource);
	obs_volmeter_add_callback(_volmeter, ElgatoCloudConfig::OBSVolumeLevel,
				  this);
	_levelsWidget->show();
	signal_handler_t *audioSigHandler =
		obs_source_get_signal_handler(_audioCaptureSource);
	signal_handler_connect_ref(audioSigHandler, "update",
				   ElgatoCloudConfig::DefaultAudioUpdated,
				   this);
	obs_data_release(audioSettings);
}

void ElgatoCloudConfig::DefaultVideoUpdated(void *data, calldata_t *params)
{
	UNUSED_PARAMETER(params);
	auto ecc = static_cast<ElgatoCloudConfig *>(data);
	config_t *const global_config = obs_frontend_get_global_config();
	auto settings = obs_source_get_settings(ecc->_videoCaptureSource);
	auto dataStr = obs_data_get_json(settings);
	config_set_string(global_config, "ElgatoCloud",
			  "DefaultVideoCaptureSettings", dataStr);

	obs_data_release(settings);
}

void ElgatoCloudConfig::DefaultAudioUpdated(void *data, calldata_t *params)
{
	UNUSED_PARAMETER(params);
	auto ecc = static_cast<ElgatoCloudConfig *>(data);
	config_t *const global_config = obs_frontend_get_global_config();
	auto settings = obs_source_get_settings(ecc->_audioCaptureSource);
	auto dataStr = obs_data_get_json(settings);
	config_set_string(global_config, "ElgatoCloud",
			  "DefaultAudioCaptureSettings", dataStr);

	obs_data_release(settings);
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
		obs_log(LOG_INFO, "--- VIDEO: %s [%s]", name.c_str(),
			id.c_str());
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
		obs_log(LOG_INFO, "--- MIC: %s [%s]", name.c_str(), id.c_str());
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

	uint32_t sourceCX = std::max(
		obs_source_get_width(window->_videoCaptureSource), 1u);
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

ElgatoCloudConfig::~ElgatoCloudConfig()
{
	obs_display_remove_draw_callback(_videoPreview->GetDisplay(),
					 ElgatoCloudConfig::DrawVideoPreview,
					 this);
	_levelsWidget = nullptr;
	if (_videoCaptureSource) {
		obs_source_release(_videoCaptureSource);
		obs_source_remove(_videoCaptureSource);
		signal_handler_t *videoSigHandler =
			obs_source_get_signal_handler(_videoCaptureSource);
		signal_handler_disconnect(
			videoSigHandler, "update",
			ElgatoCloudConfig::DefaultVideoUpdated, this);
	}
	if (_volmeter) {
		obs_volmeter_detach_source(_volmeter);
		obs_volmeter_remove_callback(
			_volmeter, ElgatoCloudConfig::OBSVolumeLevel, this);
		obs_volmeter_destroy(_volmeter);
	}
	if (_audioCaptureSource) {
		obs_source_release(_audioCaptureSource);
		obs_source_remove(_audioCaptureSource);
		signal_handler_t *audioSigHandler =
			obs_source_get_signal_handler(_audioCaptureSource);
		signal_handler_disconnect(
			audioSigHandler, "update",
			ElgatoCloudConfig::DefaultAudioUpdated, this);
	}
}

} // namespace elgatocloud
