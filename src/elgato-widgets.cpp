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
#include <QPainter>
#include <QPixmap>
#include <QPainterPath>
#include <QResizeEvent>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QFileInfo>

#include "elgato-widgets.hpp"
#include "elgato-styles.hpp"
#include "obs-utils.hpp"
#include "util.h"

namespace elgatocloud {

QHBoxLayout* centeredWidgetLayout(QWidget* widget)
{
	auto layout = new QHBoxLayout();
	layout->addStretch();
	layout->addWidget(widget);
	layout->addStretch();
	return layout;
}

VideoCaptureSourceSelector::VideoCaptureSourceSelector(QWidget *parent,
						       std::string sourceLabel,
						       std::string sourceName,
						       obs_data_t *videoData)
	: QWidget(parent),
	  _sourceName(sourceName),
	  _noneSelected(true),
	  _deactivated(false)
{
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	std::string imageBaseDir = getImagesPath();

	//std::string cameraIconSmPath = imageBaseDir + "camera-icon-sm.png";
	//QPixmap cameraIcon(cameraIconSmPath.c_str());
	std::string cameraIconPath = imageBaseDir + "icon-camera.svg";
	_loading = true;

	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 12, 0, 0);
	layout->setSpacing(4);

	auto label = new IconLabel(cameraIconPath, sourceLabel, this);

	_videoSources = new QComboBox(this);
	_videoSources->setStyleSheet(EWizardComboBoxStyle);

	_videoPreview = new OBSQTDisplay();

	_stack = new QStackedWidget(this);
	//_stack->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

	_videoPreviewWidget = new VideoPreviewWidget(_videoPreview, 8, this);
	_videoPreviewWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	auto placeholder = new CameraPlaceholder(8, this);
	std::string cameraPlaceholderIconPath = imageBaseDir + "camera-placeholder-icon.svg";
	placeholder->setIcon(cameraPlaceholderIconPath.c_str());
	_stack->addWidget(placeholder);
	_stack->addWidget(_videoPreviewWidget);

	layout->addWidget(label);
	layout->addWidget(_stack);
	layout->addWidget(_videoSources);
	
	
	layout->addStretch();
	_setupTempSource(videoData);

	if (_videoSources->currentIndex() > 0) {
		this->_noneSelected = false;
		_stack->setCurrentIndex(1);
	} else {
		this->_noneSelected = true;
		_stack->setCurrentIndex(0);
	}

	connect(_videoSources, &QComboBox::currentIndexChanged, this,
		[this](int index) {
			_loading = true;
			if (index > 0) {
				auto vSettings = obs_data_create();
				std::string id = _videoSourceIds[index];
				obs_data_set_string(vSettings,
						    "video_device_id",
						    id.c_str());
				_changeSource(vSettings);
				obs_data_release(vSettings);
			} else {
				_changeSource(nullptr);
			}
		});
}

VideoCaptureSourceSelector::~VideoCaptureSourceSelector()
{
	obs_display_remove_draw_callback(
		_videoPreview->GetDisplay(),
		VideoCaptureSourceSelector::DrawVideoPreview, this);
	if (_videoCaptureSource) {
		//obs_source_remove(_videoCaptureSource);
		obs_source_release(_videoCaptureSource);
	}
}

void VideoCaptureSourceSelector::_setupTempSource(obs_data_t *videoData)
{
#ifdef WIN32
	const char *videoSourceId = "dshow_input";
	const char* vd_id = "video_device_id";
#elif __APPLE__
	const char *videoSourceId = "av_capture_input";
	const char* vd_id = "device";
#endif	
	const char *vId = obs_get_latest_input_type_id(videoSourceId);
	_videoCaptureSource = obs_source_create_private(
		vId, "elgato-cloud-video-config", videoData);

	obs_properties_t *vProps = obs_source_properties(_videoCaptureSource);
	obs_property_t *vDevices =
		obs_properties_get(vProps, vd_id);
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
	std::string vDevice = obs_data_get_string(vSettings, vd_id);
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
	//_videoPreview->show();
	//obs_data_release(videoSettings);
}

void VideoCaptureSourceSelector::resizeEvent(QResizeEvent* event)
{
	UNUSED_PARAMETER(event);
	int newWidth = width();
	int newHeight = static_cast<int>(newWidth * 9.0 / 16.0);
	_stack->setFixedSize(QSize(newWidth, newHeight));
	update();
}

void VideoCaptureSourceSelector::_changeSource(obs_data_t *vSettings)
{
	if (vSettings != nullptr) {
		if (_videoCaptureSource) {
			obs_source_t *tmp = _videoCaptureSource;
			_videoCaptureSource = nullptr;
			obs_source_release(tmp);
		}

		const char *videoSourceId = "dshow_input";
		const char *vId = obs_get_latest_input_type_id(videoSourceId);
		_videoCaptureSource = obs_source_create_private(
			vId, "elgato-cloud-video-config", vSettings);

		this->_noneSelected = false;
		_stack->setCurrentIndex(1);
	} else {
		this->_noneSelected = true;
		_stack->setCurrentIndex(0);
		if (_videoCaptureSource) {
			obs_source_t *tmp = _videoCaptureSource;
			_videoCaptureSource = nullptr;
			obs_source_release(tmp);
		}
	}
}

void VideoCaptureSourceSelector::DrawVideoPreview(void *data, uint32_t cx,
						  uint32_t cy)
{
	auto config = static_cast<VideoCaptureSourceSelector *>(data);
	if (config->_loading) {
		config->_loading = false;
	}
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
	if (!_videoCaptureSource) {
		return "{}";
	}
	obs_data_t *vSettings = obs_source_get_settings(_videoCaptureSource);
	std::string vJson = obs_data_get_json(vSettings);
	obs_data_release(vSettings);
	return vJson;
}

std::string VideoCaptureSourceSelector::GetSourceName() const
{
	return _sourceName;
}

void VideoCaptureSourceSelector::DisableTempSource()
{
	calldata_t cd = {};
	calldata_set_bool(&cd, "active", false);
	proc_handler_t *ph = obs_source_get_proc_handler(_videoCaptureSource);
	proc_handler_call(ph, "activate", &cd);
	calldata_free(&cd);
}

void VideoCaptureSourceSelector::EnableTempSource()
{
	calldata_t cd = {};
	calldata_set_bool(&cd, "active", true);
	proc_handler_t *ph = obs_source_get_proc_handler(_videoCaptureSource);
	proc_handler_call(ph, "activate", &cd);
	calldata_free(&cd);
}

VideoPreviewWidget::VideoPreviewWidget(OBSQTDisplay* videoPreview, int radius, QWidget* parent)
	: QWidget(parent), _videoPreview(videoPreview), _radius(radius)
{
	_videoPreview->setParent(this);
	_videoPreview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	_videoPreview->show();
	setAttribute(Qt::WA_TransparentForMouseEvents); // Optional: pass events through if wrapper is decorative
}

void VideoPreviewWidget::resizeEvent(QResizeEvent* event)
{
	//QWidget::resizeEvent(event);
	QSize newSize = event->size();
	int newWidth = newSize.width();
	int newHeight = static_cast<int>(newWidth * 9.0 / 16.0);
	resize(newWidth, newHeight);
	setFixedHeight(newHeight);

	if (_videoPreview) {
		_videoPreview->setGeometry(QRect(0, 0, newWidth, newHeight));
	}
	update(); // Trigger repaint for the border
}

void VideoPreviewWidget::applyRoundedMask()
{
	QPainterPath path;
	path.addRoundedRect(_videoPreview->rect(), _radius, _radius);
	_videoPreview->setMask(QRegion(path.toFillPolygon().toPolygon()));
}

void VideoPreviewWidget::paintEvent(QPaintEvent* event)
{
	QWidget::paintEvent(event);
}

QSize VideoPreviewWidget::sizeHint() const
{
	// Return the preferred size of the widget
	//QRectF rect = _videoPreview->rect();
	int w = width();
	int h = static_cast<int>(w * 9.0 / 16.0);
	return QSize(w, h);
}


ProgressSpinner::ProgressSpinner(
	QWidget *parent, int width, int height,
	int progressWidth, QColor fgColor, QColor bgColor, bool cycle, bool showPct)
: QWidget(parent), _width(width), _height(height), _progressWidth(progressWidth),
	  _fgColor(fgColor),
	  _bgColor(bgColor),
	  _blue(true),
	  _showPct(showPct)
{
	_minimumValue = 0;
	_maximumValue = 100;
	_value = 0;

	if (cycle) {
		QPropertyAnimation *animBlue =
			new QPropertyAnimation(this, "valueBlue", this);
		animBlue->setDuration(1000);
		animBlue->setStartValue(0.0);
		animBlue->setEndValue(100.0);
		animBlue->setEasingCurve(QEasingCurve::InOutExpo);
		QPropertyAnimation *animGrey =
			new QPropertyAnimation(this, "valueGrey", this);
		animGrey->setDuration(1000);
		animGrey->setStartValue(0.0);
		animGrey->setEndValue(100.0);
		animGrey->setEasingCurve(QEasingCurve::InOutExpo);

		QSequentialAnimationGroup *group =
			new QSequentialAnimationGroup(this);
		group->addAnimation(animBlue);
		group->addAnimation(animGrey);
		group->setLoopCount(100);
		group->start();
	}
}

ProgressSpinner::~ProgressSpinner() {}

void ProgressSpinner::setValueBlue(double value)
{
	_blue = true;
	_value = value;
	update();
}

void ProgressSpinner::setValueGrey(double value)
{
	_blue = false;
	_value = value;
	update();
}

void ProgressSpinner::paintEvent(QPaintEvent *e)
{
	UNUSED_PARAMETER(e);
	int margin = _progressWidth / 2;

	int width = _width - _progressWidth;
	int height = _height - _progressWidth;

	double value = 360.0 * (_value - _minimumValue) /
		       (_maximumValue - _minimumValue);

	QPainter paint;
	paint.begin(this);
	paint.setRenderHint(QPainter::Antialiasing);

	QRect rect(0, 0, _width, _height);
	paint.setPen(Qt::NoPen);
	paint.drawRect(rect);

	QPen pen;
	QColor fg = _blue ? _fgColor : _bgColor;
	QColor bg = _blue ? _bgColor : _fgColor;
	pen.setColor(fg);
	pen.setWidth(_progressWidth);

	paint.setPen(pen);
	paint.drawArc(margin, margin, width, height, 90.0 * 16.0,
		      -value * 16.0);

	QPen bgPen;
	bgPen.setColor(bg);
	bgPen.setWidth(_progressWidth);
	paint.setPen(bgPen);
	float remaining = 360.0 - value;
	paint.drawArc(margin, margin, width, height, 90.0 * 16.0,
		      remaining * 16.0);

	if (_showPct) {
		double fontSize = _width / 4.0;
		paint.setPen(fg); // or choose a custom color

		QFont font = paint.font();
		font.setPixelSize(fontSize); // <-- set your desired size in pixels
		font.setBold(true);

		paint.setFont(font);

		QString text = QString::number(int(_value)) + "%";

		// Center inside the whole widget
		QRect textRect(0, 0, _width, _height);

		paint.drawText(textRect, Qt::AlignCenter, text);
	}

	paint.end();
}

SpinnerPanel::SpinnerPanel(QWidget *parent, std::string title,
			   std::string subTitle, bool background)
	: QWidget(parent)
{
	UNUSED_PARAMETER(title);
	UNUSED_PARAMETER(subTitle);
	UNUSED_PARAMETER(background);
	QVBoxLayout *vLayout = new QVBoxLayout();
	QHBoxLayout *hLayout = new QHBoxLayout();
	auto spinner = new ProgressSpinner(this, 124, 124, 8, QColor(32, 76, 254), QColor(200, 200, 200));
	spinner->setFixedHeight(124);
	spinner->setFixedWidth(124);

	hLayout->addStretch();
	hLayout->addWidget(spinner);
	hLayout->addStretch();
	vLayout->addStretch();
	vLayout->addLayout(hLayout);
	vLayout->addStretch();
	setLayout(vLayout);
}

SmallSpinner::SmallSpinner(QWidget* parent)
	: QWidget(parent)
{
	QVBoxLayout* vLayout = new QVBoxLayout();
	QHBoxLayout* hLayout = new QHBoxLayout();
	auto spinner = new ProgressSpinner(this, 24, 24, 2, QColor(255, 255, 255), QColor(255, 255, 255, 25));
	spinner->setFixedHeight(24);
	spinner->setFixedWidth(24);

	hLayout->addStretch();
	hLayout->addWidget(spinner);
	hLayout->addStretch();
	vLayout->addLayout(hLayout);
	setLayout(vLayout);
}

StepperStep::StepperStep(std::string text, bool firstStep, QWidget* parent)
	: QWidget(parent), _firstStep(firstStep), _status(FUTURE_STEP)
{
	std::string imageBaseDir =
		obs_get_module_data_path(obs_current_module());
	imageBaseDir += "/images/";
	std::string currentMarkerPath = imageBaseDir + "stepper-marker-current.svg";
	std::string priorMarkerPath = imageBaseDir + "stepper-marker-prior.svg";
	std::string futureMarkerPath = imageBaseDir + "stepper-marker-future.svg";
	_currentMarker = QPixmap(currentMarkerPath.c_str());
	_priorMarker = QPixmap(priorMarkerPath.c_str());
	_futureMarker = QPixmap(futureMarkerPath.c_str());

	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(8);
	if (!_firstStep) {
		auto separatorLayout = new QHBoxLayout();
		separatorLayout->setContentsMargins(0, 0, 0, 0);
		separatorLayout->setSpacing(0);
		std::string activeIconPath = imageBaseDir + "stepper-separator-active.svg";
		std::string inactiveIconPath = imageBaseDir + "stepper-separator-inactive.svg";
		_activeSeparator = QPixmap(activeIconPath.c_str());
		_inactiveSeparator = QPixmap(inactiveIconPath.c_str());
		_separator = new QLabel(this);
		_separator->setFixedSize(16, 16);
		_separator->setPixmap(_inactiveSeparator);
		separatorLayout->addWidget(_separator);
		separatorLayout->addStretch();
		layout->addLayout(separatorLayout);
	} else {
		_separator = nullptr;
	}

	auto labelLayout = new QHBoxLayout();
	labelLayout->setContentsMargins(0, 0, 0, 0);
	labelLayout->setSpacing(8);
	_marker = new QLabel(this);
	_marker->setPixmap(_priorMarker);
	//_marker->setFixedSize(16, 16);
	_label = new QLabel(text.c_str(), this);
	_label->setStyleSheet("QLabel { font-size: 14px; }");
	labelLayout->addWidget(_marker);
	labelLayout->addWidget(_label);
	labelLayout->addStretch();
	layout->addLayout(labelLayout);
}

void StepperStep::setStatus(StepperStepStatus status)
{
	_status = status;
	_update();
}

void StepperStep::_update()
{
	switch (_status) {
	case PRIOR_STEP:
		_marker->setPixmap(_priorMarker);
		if (!_firstStep) {
			_separator->setPixmap(_activeSeparator);
		}
		_label->setStyleSheet(EStepperLabelPrior);
		break;
	case FUTURE_STEP:
		_marker->setPixmap(_futureMarker);
		if (!_firstStep) {
			_separator->setPixmap(_inactiveSeparator);
		}
		_label->setStyleSheet(EStepperLabelFuture);
		break;
	case CURRENT_STEP:
		_marker->setPixmap(_currentMarker);
		if (!_firstStep) {
			_separator->setPixmap(_activeSeparator);
		}
		_label->setStyleSheet(EStepperLabelCurrent);
		break;
	}
	update();
}

Stepper::Stepper(std::vector<std::string> stepLabels, QWidget* parent)
	: QWidget(parent)
{
	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(8);

	int i = 0;
	for (auto label : stepLabels) {
		auto* step = new StepperStep(label, i == 0, this);
		layout->addWidget(step);
		_steps.append(step);
		i++;
	}

	_currentStep = 2;
	_update();
}

void Stepper::setStep(int step)
{
	_currentStep = step;
	_update();
}

void Stepper::incrementStep()
{
	_currentStep++;
	_update();
}

void Stepper::decrementStep()
{
	if(_currentStep > 0)
		_currentStep--;
	_update();
}

void Stepper::_update()
{
	int i = 0;
	for (auto step : _steps) {
		StepperStepStatus status = 
			i == _currentStep ? CURRENT_STEP :
			i < _currentStep ? PRIOR_STEP :
			FUTURE_STEP;
		step->setStatus(status);
		i++;
	}
}

RoundedImageLabel::RoundedImageLabel(int cornerRadius, QWidget* parent)
	: QLabel(parent), _cornerRadius(cornerRadius)
{
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	setMinimumHeight(40);
}

void RoundedImageLabel::setImage(const QPixmap& pixmap)
{
	originalPixmap = pixmap;
	updateScaledPixmap();
	update();
}

void RoundedImageLabel::resizeEvent(QResizeEvent* event)
{
	QLabel::resizeEvent(event);
	updateScaledPixmap();
}

void RoundedImageLabel::updateScaledPixmap()
{
	if (originalPixmap.isNull() || parentWidget() == nullptr)
		return;

	int targetWidth = width();
	int newHeight = (targetWidth * originalPixmap.height()) / originalPixmap.width();
	QPixmap scaled = originalPixmap.scaled(targetWidth, newHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);

	// Create rounded mask
	QPixmap rounded(scaled.size());
	rounded.fill(Qt::transparent);

	QPainter painter(&rounded);
	painter.setRenderHint(QPainter::Antialiasing);

	QPainterPath path;
	path.addRoundedRect(scaled.rect(), _cornerRadius, _cornerRadius);
	painter.setClipPath(path);
	painter.drawPixmap(0, 0, scaled);

	// Draw 1px semi-transparent white border around the rounded rect
	QColor strokeColor(255, 255, 255, 64); // 50% white
	QPen pen(strokeColor, 1);
	painter.setClipping(false); // Stop clipping so we can draw the stroke
	painter.setPen(pen);
	painter.drawPath(path);

	scaledPixmapWithRoundedCorners = rounded;
	setFixedHeight(rounded.height());
}

void RoundedImageLabel::paintEvent(QPaintEvent* event)
{
	QLabel::paintEvent(event);

	if (!scaledPixmapWithRoundedCorners.isNull())
	{
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);
		int x = (width() - scaledPixmapWithRoundedCorners.width()) / 2;
		painter.drawPixmap(x, 0, scaledPixmapWithRoundedCorners);
	}
}

CameraPlaceholder::CameraPlaceholder(int cornerRadius, QWidget* parent)
	: QWidget(parent), _cornerRadius(cornerRadius), _svgRenderer(nullptr) {

	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void CameraPlaceholder::setIcon(const QString& svgFilePath) {
	if (_svgRenderer) {
		delete _svgRenderer;
		_svgRenderer = nullptr;
	}

	QFileInfo check(svgFilePath);
	if (check.exists() && check.suffix().toLower() == "svg") {
		_svgRenderer = new QSvgRenderer(svgFilePath, this);

		// Optional: get default size from SVG
		_iconSize = _svgRenderer->defaultSize();
		if (_iconSize.isEmpty()) {
			_iconSize = QSize(48, 48); // fallback size
		}

		update(); // trigger repaint
	}
}

void CameraPlaceholder::resizeEvent(QResizeEvent* event) {
	int w = event->size().width();
	int h = static_cast<int>(w * 9.0 / 16.0);
	resize(w, h);
	QWidget::resizeEvent(event);
	updateGeometry();
}

void CameraPlaceholder::paintEvent(QPaintEvent* event) {
	Q_UNUSED(event);

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	QRectF rect = this->rect();

	QPainterPath path;
	path.addRoundedRect(rect, _cornerRadius, _cornerRadius);
	painter.setClipPath(path);

	// Draw background
	painter.fillRect(rect, QColor(35, 35, 35));

	// Draw SVG in the center
	if (_svgRenderer) {
		QSizeF size = _iconSize;
		QPointF topLeft((rect.width() - size.width()) / 2.0, (rect.height() - size.height()) / 2.0);
		QRectF iconRect(topLeft, size);
		_svgRenderer->render(&painter, iconRect);
	}
}

QSize CameraPlaceholder::sizeHint() const
{
	// Return the preferred size of the widget
	QRectF rect = this->rect();
	int w = rect.width();
	int h = static_cast<int>(w * 9.0 / 16.0);
	return QSize(w, h);
}

InfoLabel::InfoLabel(const QString& text, const QString& svgPath, QWidget* parent)
	: QWidget(parent) {

	_iconLabel = new QLabel;
	_textLabel = new QLabel;

	_iconLabel->setFixedSize(20, 20);
	_iconLabel->setScaledContents(true);
	_iconLabel->setAlignment(Qt::AlignCenter);
	_iconLabel->setStyleSheet("QLabel {background: transparent;}");

	_textLabel->setText(text);
	_textLabel->setStyleSheet("QLabel {background: transparent; color: white; font-size: 14px;}");

	QHBoxLayout* layout = new QHBoxLayout(this);
	layout->setContentsMargins(16, 8, 16, 8);
	layout->setSpacing(8);
	layout->addWidget(_iconLabel);
	layout->addWidget(_textLabel);
	layout->addStretch();

	setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
	setMinimumHeight(30);
	setAttribute(Qt::WA_StyledBackground);

	if (!svgPath.isEmpty()) {
		setIconFromSvg(svgPath);
	}
}

void InfoLabel::setText(const QString& text) {
	_textLabel->setText(text);
}

void InfoLabel::setIconFromSvg(const QString& svgPath) {
	QPixmap pixmap = _renderSvgToPixmap(svgPath, QSize(24, 24));
	_iconLabel->setPixmap(pixmap);
}

QPixmap InfoLabel::_renderSvgToPixmap(const QString& svgPath, const QSize& size) {
	QSvgRenderer renderer(svgPath);

	QImage image(size * devicePixelRatioF(), QImage::Format_ARGB32_Premultiplied);
	image.setDevicePixelRatio(devicePixelRatioF());
	image.fill(Qt::transparent);

	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing);
	renderer.render(&painter, QRectF(QPointF(0, 0), QSizeF(size)));

	return QPixmap::fromImage(image);
}

void InfoLabel::paintEvent(QPaintEvent* event) {
	Q_UNUSED(event);
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing, true);

	QPainterPath path;
	path.addRoundedRect(rect(), 8, 8);

	painter.fillPath(path, QColor("#061965"));  // Blue background
}

StreamPackageHeader::StreamPackageHeader(QWidget* parent, std::string name,
	std::string thumbnailPath = "")
	: QWidget(parent)
{
	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setSpacing(16);
	layout->setContentsMargins(0, 0, 0, 0);
	QLabel* nameLabel = new QLabel(this);
	nameLabel->setText(name.c_str());
	nameLabel->setSizePolicy(QSizePolicy::Expanding,
		QSizePolicy::Preferred);
	nameLabel->setStyleSheet(EWizardStepTitle);
	nameLabel->setWordWrap(true);
	layout->addWidget(nameLabel);
	if (thumbnailPath != "") {
		auto thumbnail = new RoundedImageLabel(8, this);
		QPixmap image(thumbnailPath.c_str());
		thumbnail->setImage(image);
		layout->addWidget(thumbnail);
	} else {
		auto placeholder = new CameraPlaceholder(8, this);
		std::string imageBaseDir = GetDataPath();
		imageBaseDir += "/images/";
		std::string imgPath = imageBaseDir + "icon-scene-collection.svg";
		placeholder->setIcon(imgPath.c_str());
		layout->addWidget(placeholder);
	}
}

IconLabel::IconLabel(const std::string& svgPath, const std::string& text, QWidget* parent)
	: QWidget(parent)
{
	QHBoxLayout* layout = new QHBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(6);

	QLabel* iconLabel = new QLabel(this);
	QSize iconSize(20, 20);
	QPixmap pixmap(iconSize);
	pixmap.fill(Qt::transparent);

	QSvgRenderer renderer(QString(svgPath.c_str()));
	QPainter painter(&pixmap);
	renderer.render(&painter);

	iconLabel->setPixmap(pixmap);
	iconLabel->setFixedSize(iconSize);

	QLabel* textLabel = new QLabel(text.c_str(), this);
	textLabel->setStyleSheet(EWizardFieldLabelQuiet);

	layout->addWidget(iconLabel);
	layout->addWidget(textLabel);
}


} // namespace elgatocloud
