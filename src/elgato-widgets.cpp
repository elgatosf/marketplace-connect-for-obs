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

#include <obs-frontend-api.h>
#include <util/config-file.h>

#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>

#include "elgato-widgets.hpp"
#include "elgato-styles.hpp"
#include "obs-utils.hpp"

namespace elgatocloud {

VideoCaptureSourceSelector::VideoCaptureSourceSelector(QWidget* parent, std::string sourceLabel, std::string sourceName, obs_data_t* videoData)
	: QWidget(parent), _sourceName(sourceName), _noneSelected(true)
{
	std::string imageBaseDir =
		obs_get_module_data_path(obs_current_module());
	imageBaseDir += "/images/";

	setFixedWidth(258);

	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 16);
	auto videoDeviceLabel = new QLabel(this);

	videoDeviceLabel->setText(sourceLabel.c_str());
	videoDeviceLabel->setStyleSheet("QLabel { font-size: 12pt; }");
	_videoSources = new QComboBox(this);
	_videoSources->setStyleSheet(EComboBoxStyle);

	_videoPreview = new OBSQTDisplay(this);
	_videoPreview->setFixedHeight(144);
	_videoPreview->hide();

	_blank = new QLabel(this);
	_blank->setText("None Selected");
	_blank->setAlignment(Qt::AlignCenter);
	_blank->setFixedHeight(144);

	auto videoSettings = new QHBoxLayout(this);
	videoSettings->addWidget(_videoSources);

	//auto configButton = new QPushButton(this);
	//std::string settingsIconPath = imageBaseDir + "icon-settings.svg";
	//QIcon settingsIcon = QIcon();
	//settingsIcon.addFile(settingsIconPath.c_str(), QSize(), QIcon::Normal,
	//	QIcon::Off);
	//configButton->setIcon(settingsIcon);
	//configButton->setIconSize(QSize(22, 22));
	//configButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	//configButton->setStyleSheet(EIconButtonStyle);
	//connect(configButton, &QPushButton::released, this, [this]() {
	//	obs_frontend_open_source_properties(_videoCaptureSource);
	//	});
	//videoSettings->addWidget(configButton);
	layout->addWidget(videoDeviceLabel);
	layout->addLayout(videoSettings);
	layout->addWidget(_videoPreview);
	layout->addWidget(_blank);
	auto spacer = new QWidget(this);
	spacer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	layout->addWidget(spacer);
	_setupTempSource(videoData);

	if (_videoSources->currentIndex() > 0) {
		this->_noneSelected = false;
		this->_videoPreview->show();
		this->_blank->hide();
	} else {
		this->_noneSelected = true;
		this->_videoPreview->hide();
		this->_blank->show();
	}

	connect(_videoSources, &QComboBox::currentIndexChanged, this,
		[this](int index) {
			if (index > 0) {
				auto vSettings = obs_data_create();
				std::string id = _videoSourceIds[index];
				obs_data_set_string(vSettings, "video_device_id",
					id.c_str());
				obs_source_reset_settings(_videoCaptureSource,
					vSettings);
				obs_data_release(vSettings);
				this->_noneSelected = false;
				this->_videoPreview->show();
				this->_blank->hide();
			}
			else {
				this->_noneSelected = true;
				this->_videoPreview->hide();
				this->_blank->show();
			}

		});
}

VideoCaptureSourceSelector::~VideoCaptureSourceSelector()
{
	obs_display_remove_draw_callback(_videoPreview->GetDisplay(),
		VideoCaptureSourceSelector::DrawVideoPreview,
		this);
	if (_videoCaptureSource) {
		obs_source_release(_videoCaptureSource);
		obs_source_remove(_videoCaptureSource);
	}
}

void VideoCaptureSourceSelector::_setupTempSource(obs_data_t* videoData)
{
	const char* videoSourceId = "dshow_input";
	const char* vId = obs_get_latest_input_type_id(videoSourceId);
	_videoCaptureSource = obs_source_create_private(
		vId, "elgato-cloud-video-config", videoData);

	obs_properties_t* vProps = obs_source_properties(_videoCaptureSource);
	obs_property_t* vDevices =
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

	obs_data_t* vSettings = obs_source_get_settings(_videoCaptureSource);
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
			VideoCaptureSourceSelector::DrawVideoPreview,
			this);
		};
	connect(_videoPreview, &OBSQTDisplay::DisplayCreated, addDrawCallback);
	//_videoPreview->show();
	//obs_data_release(videoSettings);
}

void VideoCaptureSourceSelector::DrawVideoPreview(void* data, uint32_t cx, uint32_t cy)
{
	auto config = static_cast<VideoCaptureSourceSelector*>(data);

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

std::string VideoCaptureSourceSelector::GetSettings() const
{
	obs_data_t *vSettings =
		obs_source_get_settings(_videoCaptureSource);
	std::string vJson = obs_data_get_json(vSettings);
	obs_data_release(vSettings);
	return vJson;
}

std::string VideoCaptureSourceSelector::GetSourceName() const
{
	return _sourceName;
}
}
