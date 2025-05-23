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

#include "elgato-cloud-window.hpp"
#include "elgato-styles.hpp"

#include <curl/curl.h>
#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QAction>
#include <QLabel>
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QPixmap>
#include <QPainter>
#include <QString>
#include <QPalette>
#include <QPainterPath>
#include <QProgressBar>
#include <QtConcurrent/QtConcurrent>
#include <QApplication>
#include <QThread>
#include <QMetaObject>
#include "elgato-cloud-data.hpp"
#include "elgato-cloud-config.hpp"
#include "elgato-widgets.hpp"
#include "setup-wizard.hpp"
#include "flowlayout.h"
#include "api.hpp"
#include "obs-utils.hpp"

#include <plugin-support.h>

namespace elgatocloud {

ElgatoCloudWindow *ElgatoCloudWindow::window = nullptr;

Avatar::Avatar(QWidget *parent) : QWidget(parent)
{
	update();
	setFixedWidth(40);
	setFixedHeight(40);
}

void Avatar::update()
{
	auto api = MarketplaceApi::getInstance();
	_bgColor = api->avatarColor();
	auto first = api->firstName();
	_character = first.substr(0, 1).c_str();
}

void Avatar::paintEvent(QPaintEvent *e)
{
	QPainter paint;
	paint.begin(this);
	paint.setRenderHint(QPainter::Antialiasing);
	paint.setPen(_bgColor.c_str());
	paint.setBrush(QBrush(_bgColor.c_str(), Qt::SolidPattern));
	paint.drawEllipse(1, 1, 38, 38);

	QFont font = paint.font();
	font.setPixelSize(24);
	paint.setPen("#FFFFFF");
	paint.setFont(font);
	paint.drawText(QRect(1, 1, 38, 38), Qt::AlignCenter,
		       _character.c_str());
	paint.end();
}

AvatarImage::AvatarImage(QWidget* parent) : QWidget(parent)
{
	auto api = MarketplaceApi::getInstance();
	setFixedWidth(40);
	setFixedHeight(40);

	std::string imageBaseDir = GetDataPath();
	imageBaseDir += "/images/";

	std::string imagePath = api->avatarReady()
		? api->avatarPath()
		: imageBaseDir + "image-loading.svg";

	QPixmap avatarPixmap = _setupImage(imagePath);

	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	_avatarImg = new QLabel(this);
	_avatarImg->setPixmap(avatarPixmap);
	_avatarImg->setSizePolicy(QSizePolicy::Preferred,
		QSizePolicy::Preferred);
	layout->addWidget(_avatarImg);
	connect(api, &MarketplaceApi::AvatarDownloaded, this, [this]() {
		update();
	});
}

void AvatarImage::update()
{
	auto api = MarketplaceApi::getInstance();
	std::string imageBaseDir = GetDataPath();
	imageBaseDir += "/images/";

	std::string imagePath = api->avatarReady()
		? api->avatarPath()
		: imageBaseDir + "image-loading.svg";
	QPixmap avatarPixmap = _setupImage(imagePath);

	_avatarImg->setPixmap(avatarPixmap);
}

QPixmap AvatarImage::_setupImage(std::string imagePath)
{
	int targetHeight = 40;
	int cornerRadius = 20;
	QPixmap img;

	if (imagePath != "")
		img.load(imagePath.c_str());

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

	return target;
}

DownloadButton::DownloadButton(QWidget *parent) : QWidget(parent)
{
	std::string imageBaseDir = GetDataPath();
	imageBaseDir += "/images/";

	//setFixedWidth(48);

	QHBoxLayout *layout = new QHBoxLayout(this);
	_stackedWidget = new QStackedWidget(this);
	_stackedWidget->setStyleSheet("background-color: #151515");
	_stackedWidget->setFixedSize(24, 24);
	_stackedWidget->setSizePolicy(QSizePolicy::Preferred,
				      QSizePolicy::Preferred);

	_downloadButton = new QPushButton(this);
	_downloadButton->setToolTip(
		obs_module_text("MarketplaceWindow.DownloadButton.Tooltip"));
	std::string downloadIconPath = imageBaseDir + "download.svg";
	std::string downloadIconHoverPath = imageBaseDir + "download_hover.svg";
	std::string downloadIconDisabledPath =
		imageBaseDir + "download_disabled.svg";
	QString buttonStyle = EIconHoverDisabledButtonStyle;
	buttonStyle.replace("${img}", downloadIconPath.c_str());
	buttonStyle.replace("${hover-img}", downloadIconHoverPath.c_str());
	buttonStyle.replace("${disabled-img}",
			    downloadIconDisabledPath.c_str());
	_downloadButton->setFixedSize(24, 24);
	_downloadButton->setStyleSheet(buttonStyle);
	_downloadButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	//_downloadButton->setDisabled(true);
	connect(_downloadButton, &QPushButton::clicked, [this]() {
		_stackedWidget->setCurrentIndex(1);
		emit downloadClicked();
	});
	_stackedWidget->addWidget(_downloadButton);

	_downloadProgress = new DownloadProgress(this);
	_downloadProgress->setValue(0.0);
	_downloadProgress->setMinimum(0.0);
	_downloadProgress->setMaximum(100.0);
	_downloadProgress->setFixedHeight(24);
	_downloadProgress->setFixedWidth(24);

	_stackedWidget->addWidget(_downloadProgress);

	QWidget *spacer = new QWidget(this);
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	layout->addWidget(spacer);
	layout->addWidget(_stackedWidget);
}

void DownloadButton::setDisabled(bool disabled)
{
	_downloadButton->setDisabled(disabled);
}

void DownloadButton::setValue(double value)
{
	_downloadProgress->setValue(value);
}

void DownloadButton::resetDownload()
{
	_stackedWidget->setCurrentIndex(0);
	_downloadProgress->setValue(0.0);
}

DownloadProgress::DownloadProgress(QWidget *parent) : QWidget(parent)
{
	_width = 20;
	_height = 20;
	_minimumValue = 0;
	_maximumValue = 100;
	_value = 0;
	_progressWidth = 2;
}

DownloadProgress::~DownloadProgress() {}

void DownloadProgress::setValue(double value)
{
	_value = value;
	update();
}

void DownloadProgress::setMinimum(double minimum)
{
	_minimumValue = minimum;
	update();
}

void DownloadProgress::setMaximum(double maximum)
{
	_maximumValue = maximum;
	update();
}

void DownloadProgress::paintEvent(QPaintEvent *e)
{
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
	pen.setColor(QColor(32, 76, 254));
	pen.setWidth(_progressWidth);

	paint.setPen(pen);
	paint.drawArc(margin, margin, width, height, 90.0 * 16.0,
		      -value * 16.0);

	QPen bgPen;
	bgPen.setColor(QColor(59, 59, 59));
	bgPen.setWidth(_progressWidth);
	paint.setPen(bgPen);
	float remaining = 360.0 - value;
	paint.drawArc(margin, margin, width, height, 90.0 * 16.0,
		      remaining * 16.0);

	paint.end();
}

WindowToolBar::WindowToolBar(QWidget *parent) : QWidget(parent)
{
	std::string imageBaseDir = GetDataPath();
	imageBaseDir += "/images/";

	auto api = MarketplaceApi::getInstance();

	QPalette pal = QPalette();
	pal.setColor(QPalette::Window, "#151515");
	setAutoFillBackground(true);
	setPalette(pal);

	_layout = new QHBoxLayout(this);
	_layout->setContentsMargins(16, 12, 16, 12);
	_layout->setSpacing(16);
	_logo = new QLabel(this);
	std::string logoPath = imageBaseDir + "mp-logo-full.png";
	QPixmap logoPixmap = QPixmap(logoPath.c_str());
	_logo->setPixmap(logoPixmap);
	_layout->addWidget(_logo);

	_layout->addStretch();

	_settingsButton = new QPushButton(this);
	_settingsButton->setToolTip(obs_module_text(
		"MarketplaceWindow.OpenSettingsButton.Tooltip"));
	std::string settingsIconPath = imageBaseDir + "settings.svg";
	std::string settingsIconHoverPath = imageBaseDir + "settings_hover.svg";
	QString settingsButtonStyle = EIconHoverButtonStyle;
	settingsButtonStyle.replace("${img}", settingsIconPath.c_str());
	settingsButtonStyle.replace("${hover-img}",
				    settingsIconHoverPath.c_str());
	_settingsButton->setFixedSize(24, 24);
	_settingsButton->setMaximumHeight(24);
	_settingsButton->setStyleSheet(settingsButtonStyle);
	_settingsButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

	connect(_settingsButton, &QPushButton::pressed, this,
		[this]() { emit settingsClicked(); });
	_layout->addWidget(_settingsButton);

	_storeButton = new QPushButton(this);
	_storeButton->setToolTip(
		obs_module_text("MarketplaceWindow.StoreButton.Tooltip"));
	std::string storeIconPath = imageBaseDir + "marketplace-logo.svg";
	std::string storeIconHoverPath =
		imageBaseDir + "marketplace-logo_hover.svg";
	QString buttonStyle = EIconHoverButtonStyle;
	buttonStyle.replace("${img}", storeIconPath.c_str());
	buttonStyle.replace("${hover-img}", storeIconHoverPath.c_str());
	_storeButton->setFixedSize(24, 24);
	_storeButton->setMaximumHeight(24);
	_storeButton->setStyleSheet(buttonStyle);
	_storeButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

	connect(_storeButton, &QPushButton::pressed, this, [this, api]() {
		api->OpenStoreInBrowser();
	});
	_layout->addWidget(_storeButton);

	_logInButton = new QPushButton(this);
	_logInButton->setText(
		obs_module_text("MarketplaceWindow.LoginButton.LogIn"));
	_logInButton->setHidden(elgatoCloud->loggedIn);
	_logInButton->setStyleSheet(
		"QPushButton {font-size: 12pt; border-radius: 8px; padding: 8px; background-color: #232323; border: none; } "
		"QPushButton:hover {background-color: #444444; }");
	_layout->addWidget(_logInButton);
	connect(_logInButton, &QPushButton::clicked, this,
		[this]() { elgatoCloud->StartLogin(); });

	_logOutButton = new QPushButton(this);
	_logOutButton->setText(
		obs_module_text("MarketplaceWindow.LoginButton.LogOut"));
	_logOutButton->setHidden(!elgatoCloud->loggedIn);
	_logOutButton->setStyleSheet(
		"QPushButton {font-size: 12pt; border-radius: 8px; padding: 8px; background-color: #232323; border: none; } "
		"QPushButton:hover {background-color: #444444; }");
	connect(_logOutButton, &QPushButton::clicked, this,
		[this]() { elgatoCloud->LogOut(); });

	_layout->addWidget(_logOutButton);
	_avatar = new Avatar(this);
	_avatarImage = new AvatarImage(this);
	_layout->addWidget(_avatar);
	_layout->addWidget(_avatarImage);

	connect(api, &MarketplaceApi::AvatarDownloaded, this, [this]() {
		updateState();
	});
}

WindowToolBar::~WindowToolBar() {}

void WindowToolBar::disableLogout(bool disabled)
{
	_logOutButton->setDisabled(disabled);
}

void WindowToolBar::updateState()
{
	auto api = MarketplaceApi::getInstance();
	_logInButton->setHidden(elgatoCloud->loggedIn);
	_logOutButton->setHidden(!elgatoCloud->loggedIn);
	_avatar->setHidden(!elgatoCloud->loggedIn || api->avatarReady());
	_avatarImage->setHidden(!elgatoCloud->loggedIn || !api->avatarReady());
	if (elgatoCloud->loggedIn) {
		_avatar->update();
		_avatarImage->update();
	}
}

Placeholder::Placeholder(QWidget *parent, std::string message) : QWidget(parent)
{
	_layout = new QVBoxLayout(this);
	auto sp1 = new QWidget(this);
	sp1->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	auto sp2 = new QWidget(this);
	sp2->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	auto label = new QLabel(this);
	label->setText(message.c_str());
	label->setStyleSheet("QLabel { font-size: 18pt; }");
	label->setAlignment(Qt::AlignCenter);
	_layout->addWidget(sp1);
	_layout->addWidget(label);
	_layout->addWidget(sp2);
	setLayout(_layout);
}

Placeholder::~Placeholder() {}

ProductGrid::ProductGrid(QWidget *parent) : QWidget(parent)
{
	_layout = new FlowLayout(this);
	setLayout(_layout);
}

ProductGrid::~ProductGrid() {}

size_t ProductGrid::loadProducts()
{
	QLayoutItem *item;
	while ((item = layout()->takeAt(0)) != NULL) {
		delete item->widget();
		delete item;
	}

	for (auto &product : elgatoCloud->products) {
		auto widget = new ElgatoProductItem(this, product.get());
		layout()->addWidget(widget);
	}
	repaint();
	return elgatoCloud->products.size();
}

void ProductGrid::disableDownload()
{
	for (int i = 0; i < layout()->count(); ++i) {
		auto item = dynamic_cast<ElgatoProductItem *>(
			layout()->itemAt(i)->widget());
		item->disableDownload();
	}
}

void ProductGrid::enableDownload()
{
	for (int i = 0; i < layout()->count(); ++i) {
		auto item = dynamic_cast<ElgatoProductItem *>(
			layout()->itemAt(i)->widget());
		item->enableDownload();
	}
}

OwnedProducts::OwnedProducts(QWidget *parent) : QWidget(parent)
{
	auto api = MarketplaceApi::getInstance();
	_layout = new QHBoxLayout(this);
	_sideMenu = new QListWidget(this);
	_sideMenu->addItem(obs_module_text("MarketplaceWindow.PurchasedTab"));
	//_sideMenu->addItem("Installed (#)");
	_sideMenu->setSizePolicy(QSizePolicy::Preferred,
				 QSizePolicy::Expanding);
	_sideMenu->setStyleSheet(
		"QListWidget { border: none; font-size: 12pt; outline: none; } QListWidget::item { padding: 12px; border-radius: 8px; } QListWidget::item:selected { background-color: #444444} QListWidget::item:hover { background-color: #656565; }");
	_sideMenu->setCurrentRow(0);
	connect(_sideMenu, &QListWidget::itemPressed, this,
		[this](QListWidgetItem *item) {
			QString val = item->text();
			if (val ==
			    obs_module_text("MarketplaceWindow.PurchasedTab")) {
				if (_numProducts > 0)
					_content->setCurrentIndex(0);
				else
					_content->setCurrentIndex(2);
			} else {
				_content->setCurrentIndex(1);
			}
		});

	_content = new QStackedWidget(this);
	_installed = new Placeholder(this, "Installed, not yet implemented...");

	auto scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setStyleSheet("border: none;");

	_purchased = new ProductGrid(this);
	_purchased->setSizePolicy(QSizePolicy::Expanding,
				  QSizePolicy::Expanding);
	refreshProducts();

	auto noProducts = new QWidget(this);
	auto npLayout = new QVBoxLayout(noProducts);
	npLayout->addStretch();
	auto npTitle = new QLabel(
		obs_module_text("MarketplaceWindow.Purchased.NoPurchasesTitle"),
		noProducts);
	npTitle->setStyleSheet("QLabel {font-size: 18pt;}");
	npTitle->setAlignment(Qt::AlignCenter);
	npLayout->addWidget(npTitle);
	auto npSubTitle = new QLabel(
		obs_module_text(
			"MarketplaceWindow.Purchased.NoPurchasesSubtitle"),
		noProducts);
	npSubTitle->setStyleSheet("QLabel {font-size: 13pt;}");
	npSubTitle->setAlignment(Qt::AlignCenter);
	npLayout->addWidget(npSubTitle);
	npLayout->setSpacing(16);

	auto hLayout = new QHBoxLayout();
	auto mpButton = new QPushButton(this);
	mpButton->setText(
		obs_module_text("MarketplaceWindow.Purchased.OpenMarketplaceButton"));
	mpButton->setStyleSheet(
		"QPushButton {font-size: 12pt; border-radius: 8px; padding: 16px; background-color: #232323; border: none; } "
		"QPushButton:hover {background-color: #444444; }");
	connect(mpButton, &QPushButton::clicked, this, [this, api]() {
		api->OpenStoreInBrowser();
	});
	hLayout->addStretch();
	hLayout->addWidget(mpButton);
	hLayout->addStretch();

	npLayout->addLayout(hLayout);
	npLayout->addStretch();
	scroll->setWidget(_purchased);
	_content->addWidget(scroll);
	_content->addWidget(_installed);
	_content->addWidget(noProducts);
	_layout->addWidget(_sideMenu);
	_layout->addWidget(_content);
	setLayout(_layout);
}

OwnedProducts::~OwnedProducts() {}

void OwnedProducts::refreshProducts()
{
	if (elgatoCloud->loggedIn) {
		_numProducts = _purchased->loadProducts();
		if (_numProducts == 0) {
			_content->setCurrentIndex(2);
		}
	}
}

ElgatoCloudWindow::ElgatoCloudWindow(QWidget *parent) : QDialog(parent)
//ui(new Ui_ElgatoCloudWindow)
{
	elgatoCloud->mainWindowOpen = true;
	elgatoCloud->window = this;
	initialize();
	setLoggedIn();
	loading = false;
}

ElgatoCloudWindow::~ElgatoCloudWindow()
{
	if (elgatoCloud) {
		elgatoCloud->loggingIn = false;
		elgatoCloud->mainWindowOpen = false;
		elgatoCloud->window = nullptr;
	}
}

void ElgatoCloudWindow::initialize()
{
	setWindowTitle(QString("Elgato Marketplace Connect"));
	setFixedSize(1140, 600);

	QPalette pal = QPalette();
	pal.setColor(QPalette::Window, "#151515");
	setAutoFillBackground(true);
	setPalette(pal);

	_layout = new QVBoxLayout();
	_layout->setSpacing(0);
	_layout->setContentsMargins(0, 0, 0, 0);

	_mainWidget = new QWidget(this);
	auto mainLayout = new QVBoxLayout(_mainWidget);
	mainLayout->setSpacing(0);
	mainLayout->setContentsMargins(0, 0, 0, 0);

	_toolbar = new WindowToolBar(_mainWidget);
	connect(_toolbar, &WindowToolBar::settingsClicked, this,
		[this]() { _config = openConfigWindow(this); });

	mainLayout->addWidget(_toolbar);
	_stackedContent = new QStackedWidget(_mainWidget);
	_stackedContent->setStyleSheet("background-color: #151515");
	_stackedContent->setSizePolicy(QSizePolicy::Expanding,
				       QSizePolicy::Expanding);

	_ownedProducts =
		new OwnedProducts(_stackedContent); // Main content, id: 0
	_stackedContent->addWidget(_ownedProducts);

	auto loginNeeded = new LoginNeeded(this); // Login needed notice, id: 1
	_stackedContent->addWidget(loginNeeded);

	auto connectionError =
		new ConnectionError(this); // Connection Error Notice, id: 2
	_stackedContent->addWidget(connectionError);

	auto loadingWidget = new LoadingWidget(this); // Loading widget, id: 3
	_stackedContent->addWidget(loadingWidget);

	auto loginErrorWidget =
		new LoginError(this); // Login error widget, id: 4
	_stackedContent->addWidget(loginErrorWidget);

	auto loggingInWidget = new LoggingIn(this); // logging in widget, id: 5
	_stackedContent->addWidget(loggingInWidget);

	mainLayout->addWidget(_stackedContent);
	_mainWidget->setLayout(mainLayout);

	_layout->addWidget(_mainWidget);

	setLayout(_layout);
}

void ElgatoCloudWindow::setLoading()
{
	_toolbar->disableLogout(true);
	_stackedContent->setCurrentIndex(3);
}

void ElgatoCloudWindow::on_logInButton_clicked()
{
	elgatoCloud->StartLogin();
}

void ElgatoCloudWindow::on_logOutButton_clicked()
{
	elgatoCloud->LogOut();
}

void ElgatoCloudWindow::setLoggedIn()
{
	_toolbar->disableLogout(false);
	if (elgatoCloud->loginError) {
		_stackedContent->setCurrentIndex(4);
	} else if (elgatoCloud->connectionError) {
		// Connection error shows sign in issue
		// for now.
		_stackedContent->setCurrentIndex(4);
	} else if (elgatoCloud->loggingIn) {
		_stackedContent->setCurrentIndex(5);
	} else if (!elgatoCloud->loggedIn) {
		_stackedContent->setCurrentIndex(1);
	} else if (elgatoCloud->loading) {
		_toolbar->disableLogout(true);
		_stackedContent->setCurrentIndex(3);
	} else {
		_stackedContent->setCurrentIndex(0);
	}
	_toolbar->updateState();
}

void ElgatoCloudWindow::setupOwnedProducts()
{
	_ownedProducts->refreshProducts();
	_stackedContent->setCurrentIndex(0);
}

ElgatoProductItem::ElgatoProductItem(QWidget *parent, ElgatoProduct *product)
	: QWidget(parent),
	  _product(product)
{
	setFixedWidth(270);
	//setStyleSheet("QWidget { border: 1px solid #FFF; }");
	std::string imageBaseDir = GetDataPath();
	imageBaseDir += "/images/";

	QVBoxLayout *layout = new QVBoxLayout();
	layout->setSpacing(4);
	std::string imagePath = product->ready()
					? product->thumbnailPath
					: imageBaseDir + "image-loading.svg";

	product->SetProductItem(this);

	QPixmap previewImage = _setupImage(imagePath);

	auto titleLayout = new QVBoxLayout();
	titleLayout->setSpacing(0);
	std::string name = _product->name.size() > 24
				   ? _product->name.substr(0, 24) + "..."
				   : _product->name;
	QLabel *label = new QLabel(name.c_str(), this);
	label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	label->setMinimumWidth(150);
	label->setStyleSheet("QLabel {font-size: 11pt;}");

	titleLayout->addWidget(label);
	auto subTitle = new QLabel("Scene Collection", this);
	subTitle->setStyleSheet("QLabel {font-size: 10pt; color: #7E7E7E; }");
	titleLayout->addWidget(subTitle);

	_downloadButton = new DownloadButton(this);

	connect(_downloadButton, &DownloadButton::downloadClicked, [this]() {
		auto p = dynamic_cast<ProductGrid *>(parentWidget());
		p->disableDownload();
		bool success = _product->DownloadProduct();
		if (!success) {
			p->enableDownload();
			resetDownload();
		}
	});

	auto labelLayout = new QHBoxLayout();
	labelLayout->addLayout(titleLayout);
	QWidget *spacer = new QWidget(this);
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	//spacer->setStyleSheet("QWidget { border: 1px solid #FFFFFF; }");
	labelLayout->addWidget(spacer);
	labelLayout->addWidget(_downloadButton);

	_labelImg = new QLabel(this);
	_labelImg->setPixmap(previewImage);
	_labelImg->setSizePolicy(QSizePolicy::Preferred,
				 QSizePolicy::Preferred);
	layout->addWidget(_labelImg);
	layout->addLayout(labelLayout);

	setLayout(layout);
}

ElgatoProductItem::~ElgatoProductItem() {}

void ElgatoProductItem::resetDownload()
{
	//_labelDownload->setCurrentIndex(0);
	_downloadButton->resetDownload();
	auto pw = dynamic_cast<ProductGrid *>(parentWidget());
	pw->enableDownload();
}

void ElgatoProductItem::disableDownload()
{
	//_downloadButton->setVisible(false);
	_downloadButton->setDisabled(true);
}

void ElgatoProductItem::enableDownload()
{
	//_downloadButton->setVisible(true);
	_downloadButton->setDisabled(false);
}

void ElgatoProductItem::updateImage()
{
	std::string imageBaseDir = GetDataPath();
	imageBaseDir += "/images/";
	std::string imagePath = _product->ready()
					? _product->thumbnailPath
					: imageBaseDir + "image-loading.svg";
	QPixmap image = _setupImage(imagePath);
	_labelImg->setPixmap(image);
}

QPixmap ElgatoProductItem::_setupImage(std::string imagePath)
{
	int targetHeight = 120;
	int cornerRadius = 8;
	QPixmap img;

	if (imagePath != "")
		img.load(imagePath.c_str());

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

	return target;
}

void ElgatoProductItem::UpdateDownload(bool downloading, int progress)
{
	if (downloading) {
		_downloadButton->setValue(progress);
	}
}

LoginNeeded::LoginNeeded(QWidget *parent) : QWidget(parent)
{
	auto layout = new QVBoxLayout(this);

	auto login = new QLabel(this);
	login->setText(obs_module_text("MarketplaceWindow.LoginNeeded.Title"));
	login->setStyleSheet("QLabel {font-size: 18pt; }");
	login->setAlignment(Qt::AlignCenter);

	auto subHLayout = new QHBoxLayout();
	auto loginSub = new QLabel(this);
	loginSub->setText(
		obs_module_text("MarketplaceWindow.LoginNeeded.Subtitle"));
	loginSub->setStyleSheet("QLabel {font-size: 13pt; }");
	loginSub->setFixedWidth(480);
	loginSub->setWordWrap(true);
	loginSub->setAlignment(Qt::AlignCenter);
	subHLayout->addStretch();
	subHLayout->addWidget(loginSub);
	subHLayout->addStretch();

	auto hLayout = new QHBoxLayout();
	auto loginButton = new QPushButton(this);
	loginButton->setText(
		obs_module_text("MarketplaceWindow.LoginButton.LogIn"));
	loginButton->setStyleSheet(
		"QPushButton {font-size: 12pt; border-radius: 8px; padding: 16px; background-color: #232323; border: none; } "
		"QPushButton:hover {background-color: #444444; }");
	connect(loginButton, &QPushButton::clicked, this,
		[this]() { elgatoCloud->StartLogin(); });
	hLayout->addStretch();
	hLayout->addWidget(loginButton);
	hLayout->addStretch();

	layout->addStretch();
	layout->addWidget(login);
	layout->addLayout(subHLayout);
	layout->addLayout(hLayout);
	layout->addStretch();
	layout->setSpacing(16);
}

LoggingIn::LoggingIn(QWidget* parent) : QWidget(parent)
{
	auto layout = new QVBoxLayout(this);

	auto login = new QLabel(this);
	login->setText(obs_module_text("MarketplaceWindow.LoggingIn.Title"));
	login->setStyleSheet("QLabel {font-size: 18pt; }");
	login->setAlignment(Qt::AlignCenter);

	auto subHLayout = new QHBoxLayout();
	auto loginSub = new QLabel(this);
	loginSub->setText(
		obs_module_text("MarketplaceWindow.LoggingIn.Subtitle"));
	loginSub->setStyleSheet("QLabel {font-size: 13pt; }");
	loginSub->setFixedWidth(480);
	loginSub->setWordWrap(true);
	loginSub->setAlignment(Qt::AlignCenter);
	subHLayout->addStretch();
	subHLayout->addWidget(loginSub);
	subHLayout->addStretch();

	auto hLayout = new QHBoxLayout();
	auto loginButton = new QPushButton(this);
	loginButton->setText(
		obs_module_text("MarketplaceWindow.LoggingIn.TryAgain"));
	loginButton->setStyleSheet(
		"QPushButton {font-size: 12pt; border-radius: 8px; padding: 16px; background-color: #232323; border: none; } "
		"QPushButton:hover {background-color: #444444; }");
	connect(loginButton, &QPushButton::clicked, this,
		[this]() { elgatoCloud->StartLogin(); });
	hLayout->addStretch();
	hLayout->addWidget(loginButton);
	hLayout->addStretch();
	layout->addStretch();
	layout->addWidget(login);
	layout->addLayout(subHLayout);
	layout->addLayout(hLayout);
	layout->addStretch();
	layout->setSpacing(16);
}

LoginError::LoginError(QWidget *parent) : QWidget(parent)
{
	auto layout = new QVBoxLayout(this);

	auto login = new QLabel(this);
	login->setText(obs_module_text("MarketplaceWindow.LoginError.Title"));
	login->setStyleSheet("QLabel {font-size: 18pt;}");
	login->setAlignment(Qt::AlignCenter);

	auto subHLayout = new QHBoxLayout();
	auto loginSub = new QLabel(this);
	loginSub->setText(
		obs_module_text("MarketplaceWindow.LoginError.Subtitle"));
	loginSub->setWordWrap(true);
	loginSub->setAlignment(Qt::AlignCenter);
	loginSub->setStyleSheet("QLabel {font-size: 13pt; }");
	loginSub->setFixedWidth(480);
	subHLayout->addStretch();
	subHLayout->addWidget(loginSub);
	subHLayout->addStretch();

	auto hLayout = new QHBoxLayout();
	auto loginButton = new QPushButton(this);
	loginButton->setText(
		obs_module_text("MarketplaceWindow.LoginButton.LogIn"));
	loginButton->setStyleSheet(
		"QPushButton {font-size: 12pt; border-radius: 8px; padding: 16px; background-color: #232323; border: none; } "
		"QPushButton:hover {background-color: #444444; }");
	connect(loginButton, &QPushButton::clicked, this,
		[this]() { elgatoCloud->StartLogin(); });
	hLayout->addStretch();
	hLayout->addWidget(loginButton);
	hLayout->addStretch();

	layout->addStretch();
	layout->addWidget(login);
	layout->addLayout(subHLayout);
	layout->addLayout(hLayout);
	layout->addStretch();
	layout->setSpacing(16);
}

ConnectionError::ConnectionError(QWidget *parent) : QWidget(parent)
{
	auto layout = new QVBoxLayout(this);

	auto connectionError = new QLabel(this);
	connectionError->setText(
		obs_module_text("MarketplaceWindow.ConnectionError.Title"));
	connectionError->setStyleSheet("QLabel {font-size: 18pt;}");
	connectionError->setAlignment(Qt::AlignCenter);

	auto subHLayout = new QHBoxLayout();
	auto connectionErrorSub = new QLabel(this);
	connectionErrorSub->setText(
		obs_module_text("MarketplaceWindow.ConnectionError.Subtitle"));
	connectionErrorSub->setWordWrap(true);
	connectionErrorSub->setAlignment(Qt::AlignCenter);
	connectionErrorSub->setStyleSheet("QLabel {font-size: 13pt; }");
	connectionErrorSub->setFixedWidth(480);
	subHLayout->addStretch();
	subHLayout->addWidget(connectionErrorSub);
	subHLayout->addStretch();


	layout->addStretch();
	layout->addWidget(connectionError);
	layout->addLayout(subHLayout);
	layout->addStretch();
	layout->setSpacing(16);
}

LoadingWidget::LoadingWidget(QWidget *parent) : QWidget(parent)
{
	auto layout = new QVBoxLayout(this);
	auto loading = new QLabel(this);
	loading->setText(obs_module_text("MarketplaceWindow.Loading.Title"));
	loading->setStyleSheet("QLabel {font-size: 18pt;}");
	loading->setAlignment(Qt::AlignCenter);

	//auto spinner = new ProgressSpinner(this);
	auto spinner = new SpinnerPanel(this, "", "", true);
	spinner->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	layout->addStretch();
	layout->addWidget(spinner);
	layout->addWidget(loading);
	layout->addStretch();
	layout->setSpacing(16);
}

void OpenElgatoCloudWindow()
{
	if (elgatoCloud->mainWindowOpen) {
		ElgatoCloudWindow::window->show();
		ElgatoCloudWindow::window->raise();
		ElgatoCloudWindow::window->activateWindow();
	} else {
		const auto mainWindow = static_cast<QMainWindow *>(
			obs_frontend_get_main_window());
		const QRect &hostRect = mainWindow->geometry();

		ElgatoCloudWindow::window = new ElgatoCloudWindow(mainWindow);
		ElgatoCloudWindow::window->setAttribute(Qt::WA_DeleteOnClose);
		if (elgatoCloud->loggedIn) {
			ElgatoCloudWindow::window->setLoading();
		}
		ElgatoCloudWindow::window->show();
		ElgatoCloudWindow::window->move(
			hostRect.center() -
			ElgatoCloudWindow::window->rect().center());

		if (elgatoCloud->loggedIn) {
			QFuture<void> future = QtConcurrent::run(
				[]() { elgatoCloud->LoadPurchasedProducts(); });
		}
	}
}

ElgatoCloudWindow *GetElgatoCloudWindow()
{
	if (elgatoCloud->mainWindowOpen) {
		return ElgatoCloudWindow::window;
	} else {
		return nullptr;
	}
}

void ElgatoCloudWindowSetEnabled(bool enable)
{
	if (elgatoCloud->mainWindowOpen) {
		ElgatoCloudWindow::window->setEnabled(enable);
		auto flags = enable ? ElgatoCloudWindow::window->windowFlags() &
					      ~Qt::FramelessWindowHint
				    : Qt::FramelessWindowHint;
		ElgatoCloudWindow::window->setWindowFlags(flags);
		ElgatoCloudWindow::window->show();
	}
}

void CloseElgatoCloudWindow()
{
	if (elgatoCloud->mainWindowOpen) {
		ElgatoCloudWindow::window->close();
	}
}

extern void InitElgatoCloud(obs_module_t *module)
{
	elgatoCloud = new ElgatoCloud(module);
	QAction *action = (QAction *)obs_frontend_add_tools_menu_qaction(
		"Elgato Marketplace Connect");
	action->connect(action, &QAction::triggered, OpenElgatoCloudWindow);
	obs_frontend_add_event_callback(CheckForUpdatesOnLaunch, nullptr);
}

extern void ShutDown()
{
	delete elgatoCloud;
}

extern obs_data_t *GetElgatoCloudConfig()
{
	if (elgatoCloud) {
		return elgatoCloud->GetConfig();
	}
	return nullptr;
}

extern void CheckForUpdates(bool forceCheck)
{
	if (!elgatoCloud) {
		return;
	}
	elgatoCloud->CheckUpdates(forceCheck);
}

extern void CheckForUpdatesOnLaunch(enum obs_frontend_event event,
				    void *private_data)
{
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		obs_frontend_remove_event_callback(CheckForUpdatesOnLaunch,
						   nullptr);
		CheckForUpdates(false);
	}
}

} // namespace elgatocloud
