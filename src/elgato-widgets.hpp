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

#include <string>
#include <obs-module.h>

#include <QWidget>
#include <QLabel>
#include <QComboBox>

#include "qt-display.hpp"

namespace elgatocloud {

class VideoCaptureSourceSelector : public QWidget {
	Q_OBJECT

public:
	VideoCaptureSourceSelector(QWidget* parent, std::string sourceLabel, std::string sourceName, obs_data_t* videoData);
	~VideoCaptureSourceSelector();

	static void DrawVideoPreview(void* data, uint32_t cx, uint32_t cy);
	std::string GetSettings() const;
	std::string GetSourceName() const;
	void DisableTempSource();
	void EnableTempSource();

private:
	obs_source_t* _videoCaptureSource = nullptr;
	OBSQTDisplay* _videoPreview = nullptr;
	QLabel* _blank = nullptr;
	QComboBox* _videoSources = nullptr;
	std::vector<std::string> _videoSourceIds;
	std::string _sourceName;
	void _setupTempSource(obs_data_t* videoData);
	bool _noneSelected;
};

class ProgressSpinner : public QWidget {
	Q_OBJECT
	Q_PROPERTY(double valueBlue WRITE setValueBlue READ getValue )
	Q_PROPERTY(double valueGrey WRITE setValueGrey READ getValue )
public:
	ProgressSpinner(QWidget* parent);
	~ProgressSpinner();
	inline double getValue() const { return _value; }
	void setValueGrey(double value);
	void setValueBlue(double value);
	

protected:
	void paintEvent(QPaintEvent* e) override;

private:
	int _width;
	int _height;
	int _progressWidth;
	double _minimumValue;
	double _maximumValue;
	double _value;
	bool _blue;
};

class SpinnerPanel : public QWidget {
	Q_OBJECT
public:
	SpinnerPanel(QWidget* parent, std::string title, std::string subTitle, bool background);
};

}
