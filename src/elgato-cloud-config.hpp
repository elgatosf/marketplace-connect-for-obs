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

#include <QDialog>
#include <QWidget>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QStackedWidget>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.hpp>
#include <thread>
#include <mutex>

#include "qt-display.hpp"

namespace elgatocloud {

class SimpleVolumeMeter;

class DefaultAVWidget : public QWidget {
	Q_OBJECT
public:
	explicit DefaultAVWidget(QWidget *parent = nullptr);
	~DefaultAVWidget();

	void SetupVolMeter();
	void OpenConfigAudioSource();

	static void OBSVolumeLevel(void *data,
				   const float magnitude[MAX_AUDIO_CHANNELS],
				   const float peak[MAX_AUDIO_CHANNELS],
				   const float inputPeak[MAX_AUDIO_CHANNELS]);

	static void DefaultAudioUpdated(void *data, calldata_t *params);
	void save();

private:
	obs_source_t *_videoCaptureSource = nullptr;
	OBSQTDisplay *_videoPreview = nullptr;
	QPushButton *_settingsButton = nullptr;
	QLabel *_blank = nullptr;
	QComboBox *_videoSources = nullptr;
	std::vector<std::string> _videoSourceIds;
	std::string _sourceName;
	QStackedWidget *_stack = nullptr;
	void _setupTempVideoSource(obs_data_t *videoSettings);
	bool _noneSelected;

	void _setupTempAudioSource(obs_data_t *audioSettings);
	SimpleVolumeMeter *_levelsWidget = nullptr;
	obs_source_t *_audioCaptureSource = nullptr;
	obs_volmeter_t *_volmeter = nullptr;
	QComboBox *_audioSources = nullptr;
	std::vector<std::string> _audioSourceIds;
};

class SimpleVolumeMeter : public QWidget {
	Q_OBJECT
public:
	explicit SimpleVolumeMeter(QWidget *parent = nullptr,
				   obs_volmeter_t *volmeter = nullptr);

	~SimpleVolumeMeter();

	inline void setVolmeter(obs_volmeter_t *volmeter = nullptr)
	{
		_volmeter = volmeter;
	}
	void setLevel(float magnitude, float peak, float inputPeak);
	void calculateDisplayPeak(uint64_t ts);

private:
	QLabel *_dbValue;

	std::mutex _lock;
	obs_volmeter_t *_volmeter;
	uint64_t _lastRedraw = 0;
	float _minMag = -60.0f;
	float _maxMag = 0.0f;
	float _currentMag = -60.0f;
	float _currentPeak = -60.0f;
	float _currentInputPeak = -60.0f;
	float _displayMag = -60.0f;
	float _displayPeak = -60.0f;
	float _displayInputPeak = -60.0f;
	float _decayRate = 40.0f;

protected:
	void paintEvent(QPaintEvent *event) override;
};

class ElgatoCloudConfig : public QDialog {
	Q_OBJECT

public:
	explicit ElgatoCloudConfig(QWidget *parent = nullptr);
	~ElgatoCloudConfig();
	void OpenConfigVideoSource();
	void OpenConfigAudioSource();
	static void DrawVideoPreview(void *data, uint32_t cx, uint32_t cy);

	static void OBSVolumeLevel(void *data,
				   const float magnitude[MAX_AUDIO_CHANNELS],
				   const float peak[MAX_AUDIO_CHANNELS],
				   const float inputPeak[MAX_AUDIO_CHANNELS]);

private:
	void _save();
	DefaultAVWidget *_avWidget;
	QLabel *_dbValue = nullptr;
	SimpleVolumeMeter *_levelsWidget = nullptr;
	obs_source_t *_videoCaptureSource = nullptr;
	obs_source_t *_audioCaptureSource = nullptr;
	obs_volmeter_t *_volmeter = nullptr;
	OBSQTDisplay *_videoPreview = nullptr;
	QCheckBox *_makerCheckbox = nullptr;
	std::string _installDirectory;

signals:
	void closeClicked();
};

ElgatoCloudConfig *openConfigWindow(QWidget *parent);

} // namespace elgatocloud
