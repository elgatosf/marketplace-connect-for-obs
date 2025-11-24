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
#include <QListWidget>
#include <QStackedWidget>
#include <QSvgRenderer>
#include <QHBoxLayout>

#include "qt-display.hpp"

namespace elgatocloud {

QHBoxLayout* centeredWidgetLayout(QWidget* widget);

class VideoPreviewWidget;

class VideoCaptureSourceSelector : public QWidget {
	Q_OBJECT

public:
	VideoCaptureSourceSelector(QWidget *parent, std::string sourceLabel,
				   std::string sourceName,
				   obs_data_t *videoData);
	~VideoCaptureSourceSelector();

	static void DrawVideoPreview(void *data, uint32_t cx, uint32_t cy);
	std::string GetSettings() const;
	std::string GetSourceName() const;
	void DisableTempSource();
	void EnableTempSource();

protected:
	void resizeEvent(QResizeEvent* event) override;

private:
	obs_source_t *_videoCaptureSource = nullptr;
	VideoPreviewWidget* _videoPreviewWidget = nullptr;
	OBSQTDisplay *_videoPreview = nullptr;
	QStackedWidget* _stack = nullptr;
	QLabel *_blank = nullptr;
	QComboBox *_videoSources = nullptr;
	std::vector<std::string> _videoSourceIds;
	std::string _sourceName;
	void _setupTempSource(obs_data_t *videoData);
	void _changeSource(obs_data_t* vSettings);
	bool _noneSelected;
	bool _loading;
	bool _deactivated;
};

class VideoPreviewWidget : public QWidget {
	Q_OBJECT

public:
	VideoPreviewWidget(OBSQTDisplay* videoPreview, int radius, QWidget* parent);

protected:
	void resizeEvent(QResizeEvent* event) override;
	void paintEvent(QPaintEvent* event) override;
	QSize sizeHint() const override;

private:
	OBSQTDisplay* _videoPreview;
	int _radius;

	void applyRoundedMask();
};

class ProgressSpinner : public QWidget {
	Q_OBJECT
	Q_PROPERTY(double valueBlue WRITE setValueBlue READ getValue)
	Q_PROPERTY(double valueGrey WRITE setValueGrey READ getValue)
public:
	ProgressSpinner(QWidget* parent, int width, int height,
		int progressWidth, QColor fgColor, QColor bgColor, bool cycle = true);
	~ProgressSpinner();
	inline double getValue() const { return _value; }
	void setValueGrey(double value);
	void setValueBlue(double value);

protected:
	void paintEvent(QPaintEvent *e) override;

private:
	int _width;
	int _height;
	int _progressWidth;
	QColor _fgColor;
	QColor _bgColor;
	double _minimumValue;
	double _maximumValue;
	double _value;
	bool _blue;
};

class SpinnerPanel : public QWidget {
	Q_OBJECT
public:
	SpinnerPanel(QWidget *parent, std::string title, std::string subTitle,
		     bool background);
};

class SmallSpinner : public QWidget {
	Q_OBJECT
public:
	SmallSpinner(QWidget* parent);
};

enum StepperStepStatus {
	PRIOR_STEP,
	CURRENT_STEP,
	FUTURE_STEP
};

class StepperStep : public QWidget {
	Q_OBJECT
public:
	StepperStep(std::string text, bool firstStep, QWidget* parent);
	void setStatus(StepperStepStatus status);
private:
	StepperStepStatus _status;
	bool _firstStep;
	QPixmap _priorMarker, _currentMarker, _futureMarker;
	QPixmap _activeSeparator, _inactiveSeparator;
	QLabel* _marker;
	QLabel* _label;
	QLabel* _separator;

	void _update();
};

class Stepper : public QWidget {
	Q_OBJECT
public:
	Stepper(std::vector<std::string> stepLabels, QWidget* parent);

	void setStep(int newStep);
	void incrementStep();
	void decrementStep();
private:
	int _currentStep = 0;
	QListWidget* _list;
	QVector<StepperStep*> _steps;
	void _update();
};

class RoundedImageLabel : public QLabel
{
	Q_OBJECT

public:
	explicit RoundedImageLabel(int cornerRadius, QWidget* parent = nullptr);
	void setImage(const QPixmap& pixmap);

protected:
	void resizeEvent(QResizeEvent* event) override;
	void paintEvent(QPaintEvent* event) override;

private:
	QPixmap originalPixmap;
	QPixmap scaledPixmapWithRoundedCorners;
	void updateScaledPixmap();
	int _cornerRadius;
};

class CameraPlaceholder : public QWidget {
	Q_OBJECT

public:
	explicit CameraPlaceholder(int cornerRadius, QWidget* parent = nullptr);
	void setIcon(const QString& svgFilePath);

protected:
	void resizeEvent(QResizeEvent* event) override;
	void paintEvent(QPaintEvent* event) override;
	QSize sizeHint() const override;

private:
	QSvgRenderer* _svgRenderer;
	QSize _iconSize;
	int _cornerRadius;
};

class InfoLabel : public QWidget {
	Q_OBJECT

public:
	explicit InfoLabel(const QString& text = "", const QString& svgPath = "", QWidget* parent = nullptr);
	void setText(const QString& text);
	void setIconFromSvg(const QString& svgPath);  // Updated

protected:
	void paintEvent(QPaintEvent* event) override;

private:
	QLabel* _iconLabel;
	QLabel* _textLabel;

	QPixmap _renderSvgToPixmap(const QString& svgPath, const QSize& size);
};

class StreamPackageHeader : public QWidget {
	Q_OBJECT
public:
	StreamPackageHeader(QWidget* parent, std::string name,
		std::string thumbnailPath);
};

class IconLabel : public QWidget {
	Q_OBJECT
public:
	IconLabel(const std::string& svgPath, const std::string& name, QWidget* parent);
};

} // namespace elgatocloud
