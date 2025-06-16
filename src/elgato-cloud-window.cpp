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
	setFixedWidth(32);
	setFixedHeight(32);
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
	paint.drawEllipse(5, 5, 22, 22);

	QFont font = paint.font();
	font.setPixelSize(14);
	paint.setPen("#FFFFFF");
	paint.setFont(font);
	paint.drawText(QRect(5, 5, 22, 22), Qt::AlignCenter,
		       _character.c_str());
	paint.end();

}

void Avatar::mousePressEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton) {
		emit clicked();
	}

	QWidget::mousePressEvent(event); // Pass event along if needed
}

AvatarImage::AvatarImage(QWidget* parent) : QWidget(parent)
{
	auto api = MarketplaceApi::getInstance();
	setFixedWidth(32);
	setFixedHeight(32);

	std::string imageBaseDir = GetDataPath();
	imageBaseDir += "/images/";

	std::string imagePath = api->avatarReady()
		? api->avatarPath()
		: imageBaseDir + "image-loading.svg";

	QPixmap avatarPixmap = _setupImage(imagePath);

	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(4, 4, 4, 4);
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

void AvatarImage::mousePressEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton) {
		emit clicked();
	}

	QWidget::mousePressEvent(event); // Pass event along if needed
}

QPixmap AvatarImage::_setupImage(std::string imagePath)
{
	int targetHeight = 24;
	int cornerRadius = 12;
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

UserMenu::UserMenu(QWidget* parent) : QWidget(parent)
{
	QSize widgetSize(32, 32);
	auto api = MarketplaceApi::getInstance();
	_menu = new QMenu(this);

	// Disabled "Account" label
	std::string name = api->firstName() + " " + api->lastName();
	_accountTitle = new QAction(name.c_str(), _menu);
	_accountTitle->setEnabled(false);
	_menu->addAction(_accountTitle);

	// View Account (active)
	QAction* viewAccount = new QAction("View Account", _menu);
	_menu->addAction(viewAccount);

	// Separator
	_menu->addSeparator();

	QAction* signOut = new QAction("Sign out", _menu);
	_menu->addAction(signOut);

	auto layout = new QVBoxLayout(this);
	setFixedSize(widgetSize);
	layout->setContentsMargins(0, 0, 0, 0);
	_plainAvatar = new Avatar(this);
	_imgAvatar = new AvatarImage(this);
	_stack = new QStackedWidget(this);
	_stack->setFixedSize(widgetSize);
	_stack->addWidget(_plainAvatar);
	_stack->addWidget(_imgAvatar);
	_stack->setCurrentIndex(elgatoCloud->loggedIn && api->avatarReady() ? 1 : 0);
	connect(api, &MarketplaceApi::AvatarDownloaded, this, [this, api]() {
		_stack->setCurrentIndex(elgatoCloud->loggedIn && api->avatarReady() ? 1 : 0);
		if (elgatoCloud->loggedIn) {
			std::string name = api->firstName() + " " + api->lastName();
			_accountTitle->setText(name.c_str());
			_plainAvatar->update();
			_imgAvatar->update();
		}
	});

	connect(_plainAvatar, &Avatar::clicked, [this]() {
		_showModalMenu();
	});

	connect(_imgAvatar, &AvatarImage::clicked, [this]() {
		_showModalMenu();
	});

	connect(viewAccount, &QAction::triggered, []() {
	});

	connect(signOut, &QAction::triggered, []() {
		elgatoCloud->LogOut();
	});

	_menu->setStyleSheet(R"(
	QMenu::item:disabled {
		background-color: transparent;
	})");
}

void UserMenu::_showModalMenu()
{
	// Transparent modal overlay to enforce modality
	// Create a transparent non-modal QWidget to act as an overlay
	QWidget* overlay = new QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint);
	overlay->setAttribute(Qt::WA_TranslucentBackground);
	overlay->setAttribute(Qt::WA_DeleteOnClose);
	overlay->setWindowModality(Qt::ApplicationModal); // Soft application modality
	overlay->resize(qApp->primaryScreen()->size());
	overlay->move(0, 0);
	overlay->show();

	// Position and show the menu
	QPoint menuPos = this->mapToGlobal(QPoint(0, this->height()));
	_menu->popup(menuPos);

	// Close the overlay when the menu is dismissed
	connect(_menu, &QMenu::aboutToHide, overlay, &QWidget::close);
}

WindowToolBar::WindowToolBar(QWidget *parent) : QWidget(parent)
{
	std::string imageBaseDir = GetDataPath();
	imageBaseDir += "/images/";

	setFixedHeight(48);

	auto api = MarketplaceApi::getInstance();

	QPalette pal = QPalette();
	pal.setColor(QPalette::Window, "#000000");
	setAutoFillBackground(true);
	setPalette(pal);

	_layout = new QHBoxLayout(this);
	_layout->setContentsMargins(8, 8, 8, 8);
	_layout->setSpacing(0);
	_logo = new QLabel(this);
	std::string logoPath = imageBaseDir + "marketplace-full-logo.svg";
	QPixmap logoPixmap = QPixmap(logoPath.c_str());
	_logo->setPixmap(logoPixmap);
	_layout->addWidget(_logo);

	_layout->addStretch();

	_storeButton = new QPushButton(this);
	_storeButton->setToolTip(
		obs_module_text("MarketplaceWindow.StoreButton.Tooltip"));
	std::string storeIconPath = imageBaseDir + "button-marketplace-icon.svg";
	QString buttonStyle = EIconOnlyButtonStyle;
	buttonStyle.replace("${img}", storeIconPath.c_str());
	_storeButton->setFixedSize(32, 32);
	_storeButton->setMaximumHeight(32);
	_storeButton->setStyleSheet(buttonStyle);
	_storeButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

	connect(_storeButton, &QPushButton::pressed, this, [this, api]() {
		api->OpenStoreInBrowser();
		});
	_layout->addWidget(_storeButton);

	_settingsButton = new QPushButton(this);
	_settingsButton->setToolTip(obs_module_text(
		"MarketplaceWindow.OpenSettingsButton.Tooltip"));
	std::string settingsIconPath = imageBaseDir + "button-settings-icon.svg";
	QString settingsButtonStyle = EIconOnlyButtonStyle;
	settingsButtonStyle.replace("${img}", settingsIconPath.c_str());
	_settingsButton->setFixedSize(32, 32);
	_settingsButton->setMaximumHeight(32);
	_settingsButton->setStyleSheet(settingsButtonStyle);
	_settingsButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

	connect(_settingsButton, &QPushButton::pressed, this,
		[this]() { emit settingsClicked(); });
	_layout->addWidget(_settingsButton);

	//_logInButton = new QPushButton(this);
	//_logInButton->setText(
	//	obs_module_text("MarketplaceWindow.LoginButton.LogIn"));
	//_logInButton->setHidden(elgatoCloud->loggedIn);
	//_logInButton->setStyleSheet(
	//	"QPushButton {font-size: 12pt; border-radius: 8px; padding: 8px; background-color: #232323; border: none; } "
	//	"QPushButton:hover {background-color: #444444; }");
	//_layout->addWidget(_logInButton);
	//connect(_logInButton, &QPushButton::clicked, this,
	//	[this]() { elgatoCloud->StartLogin(); });

	//_logOutButton = new QPushButton(this);
	//_logOutButton->setText(
	//	obs_module_text("MarketplaceWindow.LoginButton.LogOut"));
	//_logOutButton->setHidden(!elgatoCloud->loggedIn);
	//_logOutButton->setStyleSheet(
	//	"QPushButton {font-size: 12pt; border-radius: 8px; padding: 8px; background-color: #232323; border: none; } "
	//	"QPushButton:hover {background-color: #444444; }");
	//connect(_logOutButton, &QPushButton::clicked, this,
	//	[this]() { elgatoCloud->LogOut(); });

	//_layout->addWidget(_logOutButton);
	_userMenu = new UserMenu(this);
	_userMenu->setHidden(!elgatoCloud->loggedIn);
	
	_layout->addWidget(_userMenu);

	connect(api, &MarketplaceApi::AvatarDownloaded, this, [this]() {
		updateState();
	});
}

WindowToolBar::~WindowToolBar() {}

void WindowToolBar::disableLogout(bool disabled)
{
//	_logOutButton->setDisabled(disabled);
}

void WindowToolBar::updateState()
{
	auto api = MarketplaceApi::getInstance();
	_userMenu->setHidden(!elgatoCloud->loggedIn);
	//_avatar->setHidden(!elgatoCloud->loggedIn || api->avatarReady());
	//_avatarImage->setHidden(!elgatoCloud->loggedIn || !api->avatarReady());
	//if (elgatoCloud->loggedIn) {
		//_avatar->update();
		//_avatarImage->update();
	//}
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

	std::string imageBaseDir = GetDataPath();
	imageBaseDir += "/images/";
	std::string iconPath = imageBaseDir + "your-library-icon.svg";

	_layout = new QHBoxLayout(this);
	_sideMenu = new QListWidget(this);
	QIcon icon(iconPath.c_str());
	auto yourLibrary = new QListWidgetItem(icon, obs_module_text("MarketplaceWindow.PurchasedTab"));
	_sideMenu->addItem(yourLibrary);
	//_sideMenu->addItem(obs_module_text("MarketplaceWindow.PurchasedTab"));
	//_sideMenu->addItem("Installed (#)");
	_sideMenu->setSizePolicy(QSizePolicy::Preferred,
				 QSizePolicy::Expanding);
	_sideMenu->setStyleSheet(ELeftNavListStyle);
	_sideMenu->setCurrentRow(0);
	_sideMenu->setFixedWidth(240);
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
	//scroll->setStyleSheet("border: none;");
	scroll->setStyleSheet(ESlateContainerStyle);


	_purchased = new ProductGrid(this);
	_purchased->setSizePolicy(QSizePolicy::Expanding,
				  QSizePolicy::Expanding);
	refreshProducts();

	auto noProducts = new QWidget(this);
	noProducts->setStyleSheet(ESlateContainerStyle);
	auto npLayout = new QVBoxLayout(noProducts);
	npLayout->addStretch();
	auto npTitle = new QLabel(
		obs_module_text("MarketplaceWindow.Purchased.NoPurchasesTitle"),
		noProducts);
	npTitle->setStyleSheet(EBlankSlateTitleStyle);
	npTitle->setAlignment(Qt::AlignCenter);
	npLayout->addWidget(npTitle);
	auto npSubTitle = new QLabel(
		obs_module_text(
			"MarketplaceWindow.Purchased.NoPurchasesSubtitle"),
		noProducts);
	npSubTitle->setStyleSheet(EBlankSlateSubTitleStyle);
	npSubTitle->setAlignment(Qt::AlignCenter);
	npLayout->addWidget(npSubTitle);
	npLayout->setSpacing(8);

	auto hLayout = new QHBoxLayout();
	auto mpButton = new QPushButton(this);
	mpButton->setText(
		obs_module_text("MarketplaceWindow.Purchased.OpenMarketplaceButton"));
	mpButton->setStyleSheet(EBlankSlateButtonStyle);
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
	//setFixedSize(1140, 600);

	QPalette pal = QPalette();
	pal.setColor(QPalette::Window, "#000000");
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
	_stackedContent->setStyleSheet("background-color: #000000");
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
	setFixedSize(970, 520);
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

ProgressThumbnail::ProgressThumbnail(float hoverOpacity, bool hoverDisabled, QWidget* parent)
	: QLabel(parent), _hoverOpacity(hoverOpacity), _hoverDisabled(hoverDisabled), _opacity(1.0f)
{
	setAttribute(Qt::WA_TranslucentBackground);
	setStyleSheet("background: transparent;");
	setMinimumSize(1, 1);
}

void ProgressThumbnail::setCustomPixmap(const QPixmap& pixmap)
{
	setCustomPixmap(pixmap, size());
}

void ProgressThumbnail::setCustomPixmap(const QPixmap& pixmap, const QSize& size)
{
	_pixmap = pixmap;
	_pixmapScaled = _pixmap.scaledToWidth(size.width(), Qt::SmoothTransformation);
	QImage grayImage = _pixmapScaled.toImage().convertToFormat(QImage::Format_Grayscale8);
	_pixmapScaledDisabled = QPixmap::fromImage(grayImage);
	updateGeometry();
	update();
}

QSize ProgressThumbnail::sizeHint() const
{
	//auto size = _pixmapScaled.size();
	//return _pixmapScaled.size();
	if (_pixmap.isNull())
		return QLabel::sizeHint();

	int labelWidth = width() > 0 ? width() : _pixmap.width();
	int scaledHeight = (labelWidth * _pixmap.height()) / _pixmap.width();
	return QSize(labelWidth, scaledHeight);
}

void ProgressThumbnail::setProgress(float progress)
{
	// Clamp value between 0.0 and 1.0
	_progress = std::clamp(progress, 0.0f, 1.0f);
	update();
}

void ProgressThumbnail::resizeEvent(QResizeEvent* event)
{
	QLabel::resizeEvent(event);
	setCustomPixmap(_pixmap, event->size());
}

void ProgressThumbnail::setDisabled(bool disabled)
{
	_disabled = disabled;
	update();
}

void ProgressThumbnail::setDownloading(bool downloading)
{
	_downloading = downloading;
	_opacity = 1.0f;
	update();
}

void ProgressThumbnail::paintEvent(QPaintEvent* event)
{
	if (_pixmap.isNull()) return;

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);

	// Rounded corners
	const int radius = 8;
	QPainterPath path;
	path.addRoundedRect(rect(), radius, radius);
	painter.setClipPath(path);

	QSize labelSize = size();

	int splitX = static_cast<int>(labelSize.width() * _progress);
	int height = _pixmapScaled.height();
	int width = labelSize.width();
	float opacity = _downloading ? 1.0 : _opacity;

	painter.setOpacity(opacity);
	if (!_disabled || _downloading) {
		painter.drawPixmap(0, 0, splitX, height, _pixmapScaled, 0, 0, splitX, height);
	} else {
		painter.drawPixmap(0, 0, splitX, height, _pixmapScaledDisabled, 0, 0, splitX, height);
	}

	QRect rightSide(splitX, 0, width - splitX, height);
	QColor bgColor(42,42,42);
	painter.fillRect(rightSide, bgColor);

	painter.setOpacity(1.0);
	QColor borderColor(255, 255, 255, 64);
	QPen pen(borderColor);
	pen.setWidthF(2.0);
	painter.setPen(pen);
	painter.setBrush(Qt::NoBrush);

	painter.drawRoundedRect(rect(), radius, radius);
	if (_progress > 0.0f && _progress < 1.0f) {
		painter.drawLine(splitX, 0, splitX, height);
	}
}

void ProgressThumbnail::onHoverEnter(QHoverEvent* event)
{
	if (_hoverDisabled) {
		return;
	}
	_opacity = _hoverOpacity;
	update();
}

void ProgressThumbnail::onHoverLeave(QHoverEvent* event)
{
	if (_hoverDisabled) {
		return;
	}
	_opacity = 1.0f;
	update();
}

void ProgressThumbnail::disableHover()
{
	_hoverDisabled = true;
	_opacity = 1.0f;
	update();
}

void ProgressThumbnail::enableHover()
{
	_hoverDisabled = false;
	_opacity = 0.5f;
	update();
}

ProductThumbnail::ProductThumbnail(QWidget* parent, const QPixmap& pixmap)
	: QWidget(parent)
{
	setAttribute(Qt::WA_Hover);
	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	_thumbnail = new ProgressThumbnail(0.1f, false, this);
	_thumbnail->setCustomPixmap(pixmap);
	_thumbnail->setProgress(1.0);
	std::string imageBaseDir = GetDataPath();
	imageBaseDir += "/images/";
	std::string iconPath = imageBaseDir + "button-download.svg";
	QPixmap iconPixmap(iconPath.c_str());
	QIcon downloadIcon(iconPixmap);
	_downloadButton = new QPushButton("Install", this);
	_downloadButton->setFixedHeight(32);
	_downloadButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	_downloadButton->setIcon(downloadIcon);
	_downloadButton->setIconSize(iconPixmap.rect().size());
	_downloadButton->setStyleSheet(EBlankSlateButtonStyle);
	_downloadButton->hide();
	connect(_downloadButton, &QPushButton::clicked, [this]() {
		setDownloading(true);
		emit downloadClicked();
	});

	std::string stopIconPath = imageBaseDir + "button-stop-download.svg";
	QPixmap stopIconPixmap(stopIconPath.c_str());
	QIcon stopDownloadIcon(stopIconPixmap);

	_stopDownloadButton = new QPushButton(this);
	_stopDownloadButton->setFixedHeight(32);
	_stopDownloadButton->setFixedWidth(32);
	_stopDownloadButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	_stopDownloadButton->setIcon(stopDownloadIcon);
	_stopDownloadButton->setIconSize(stopIconPixmap.rect().size());
	_stopDownloadButton->setStyleSheet(EStopDownloadButtonStyle);
	_stopDownloadButton->hide();
	connect(_stopDownloadButton, &QPushButton::clicked, [this]() {
		setDownloading(false);
		emit cancelDownloadClicked();
	});

	layout->addWidget(_thumbnail);
}

void ProductThumbnail::setPixmap(const QPixmap& pixmap)
{
	_thumbnail->setPixmap(pixmap);
}

bool ProductThumbnail::event(QEvent* e)
{
	//auto buttonSize = _downloadButton->size();
	if (!_downloading && _disable) {
		return QWidget::event(e);
	}
	QSize thumbSize;
	QSize buttonSize;
	int offsetX, offsetY;
	switch (e->type()) {
	case QEvent::HoverEnter:
		thumbSize = _thumbnail->size();
		buttonSize = !_downloading ? _downloadButton->size() : _stopDownloadButton->size();
		offsetX = (thumbSize.width() - buttonSize.width()) / 2;
		offsetY = (thumbSize.height() - buttonSize.height()) / 2;
		if (!_downloading) {
			_thumbnail->onHoverEnter(static_cast<QHoverEvent*>(e));
			_downloadButton->setGeometry(offsetX, offsetY, buttonSize.width(), buttonSize.height());
			_downloadButton->show();
		} else {
			_stopDownloadButton->setGeometry(offsetX, offsetY, buttonSize.width(), buttonSize.height());
			_stopDownloadButton->show();
		}
		return true;
		break;
	case QEvent::HoverLeave:
		if (!_downloading) {
			_thumbnail->onHoverLeave(static_cast<QHoverEvent*>(e));
			_downloadButton->hide();
		} else {
			_stopDownloadButton->hide();
		}
		return true;
		break;
	default:
		break;
	}
	return QWidget::event(e);
}

void ProductThumbnail::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);
}

QSize ProductThumbnail::sizeHint() const
{
	return _thumbnail ? _thumbnail->sizeHint() : QWidget::sizeHint();
}

void ProductThumbnail::disable(bool disable)
{
	_disable = disable;
	_thumbnail->setDisabled(disable);
	update();
}

void ProductThumbnail::enable(bool enable)
{
	_disable = !enable;
	_thumbnail->setDisabled(_disable);
	update();
}

void ProductThumbnail::setDownloading(bool downloading)
{
	_thumbnail->setDownloading(downloading);
	_downloadButton->hide();
	if (!downloading) {
		_stopDownloadButton->hide();
	}
	update();
	_downloading = downloading;
}

ElgatoProductItem::ElgatoProductItem(QWidget *parent, ElgatoProduct *product)
	: QWidget(parent),
	  _product(product)
{
	setFixedWidth(220);
	std::string imageBaseDir = GetDataPath();
	imageBaseDir += "/images/";

	QVBoxLayout *layout = new QVBoxLayout();
	layout->setSpacing(4);
	std::string imagePath = product->ready()
					? product->thumbnailPath
					: imageBaseDir + "image-loading.svg";

	product->SetProductItem(this);

	QPixmap previewImage(imagePath.c_str());

	auto titleLayout = new QVBoxLayout();
	titleLayout->setSpacing(0);
	std::string name = _product->name.size() > 25
				   ? _product->name.substr(0, 23) + "..."
				   : _product->name;
	QLabel *label = new QLabel(name.c_str(), this);
	label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	label->setMinimumWidth(150);
	label->setStyleSheet("QLabel {font-size: 13px; font-weight: 600; }");

	titleLayout->addWidget(label);
	auto subTitle = new QLabel("Scene Collection", this);
	subTitle->setStyleSheet("QLabel {font-size: 12px; color: rgba(255, 255, 255, 0.67); }");
	titleLayout->addWidget(subTitle);

	_labelImg = new ProductThumbnail(this, previewImage);
	_labelImg->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	connect(_labelImg, &ProductThumbnail::downloadClicked, [this]() {
		auto p = dynamic_cast<ProductGrid*>(parentWidget());
		p->disableDownload();
		bool success = _product->DownloadProduct();
		if (!success) {
			p->enableDownload();
			resetDownload();
		}
	});

	connect(_labelImg, &ProductThumbnail::cancelDownloadClicked, [this]() {
		_product->StopProductDownload();
		auto p = dynamic_cast<ProductGrid*>(parentWidget());
		p->enableDownload();
		resetDownload();
	});

	layout->addWidget(_labelImg);
	layout->addLayout(titleLayout);

	setLayout(layout);
}

ElgatoProductItem::~ElgatoProductItem() {}

void ElgatoProductItem::resetDownload()
{
	//_labelDownload->setCurrentIndex(0);
	setDisabled(false);
	_labelImg->disable(false);
	_labelImg->setDownloading(false);
	auto pw = dynamic_cast<ProductGrid *>(parentWidget());
	pw->enableDownload();
}

void ElgatoProductItem::disableDownload()
{
	//_downloadButton->setVisible(false);
	setDisabled(true);
	//_labelImg->setDisabled(true);
	_labelImg->disable(true);
}

void ElgatoProductItem::enableDownload()
{
	//_downloadButton->setVisible(true);
	setDisabled(false);
	_labelImg->disable(false);
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

	QColor borderColor(255, 255, 255, 32);
	QPen pen(borderColor);
	pen.setWidth(1);
	painter.setPen(pen);
	painter.setBrush(Qt::NoBrush);

	QRectF rect(0, 0, targetWidth, targetHeight);
	painter.drawRoundedRect(rect, 8, 8);
	return target;
}

void ElgatoProductItem::UpdateDownload(bool downloading, int progress)
{
	if (downloading) {
		_labelImg->updateDownloadProgress((float)progress/100.0f);
	}
}

LoginNeeded::LoginNeeded(QWidget *parent) : QWidget(parent)
{
	auto layout = new QVBoxLayout(this);
	auto container = new QWidget(this);
	container->setStyleSheet(ESlateContainerStyle);
	setContentsMargins(0, 0, 0, 0);
	auto containerLayout = new QVBoxLayout(container);

	auto login = new QLabel(this);
	login->setText(obs_module_text("MarketplaceWindow.LoginNeeded.Title"));
	login->setStyleSheet(EBlankSlateTitleStyle);
	login->setAlignment(Qt::AlignCenter);

	auto subHLayout = new QHBoxLayout();
	auto loginSub = new QLabel(this);
	loginSub->setText(
		obs_module_text("MarketplaceWindow.LoginNeeded.Subtitle"));
	loginSub->setStyleSheet(EBlankSlateSubTitleStyle);
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
	QString loginButtonStyle = EBlankSlateButtonStyle;
	loginButton->setStyleSheet(loginButtonStyle);
	connect(loginButton, &QPushButton::clicked, this,
		[this]() { elgatoCloud->StartLogin(); });
	hLayout->addStretch();
	hLayout->addWidget(loginButton);
	hLayout->addStretch();

	containerLayout->addStretch();
	containerLayout->addWidget(login);
	containerLayout->addLayout(subHLayout);
	containerLayout->addLayout(hLayout);
	containerLayout->addStretch();
	containerLayout->setSpacing(8);

	layout->addWidget(container);
}

LoggingIn::LoggingIn(QWidget* parent) : QWidget(parent)
{
	auto layout = new QVBoxLayout(this);
	auto container = new QWidget(this);
	container->setStyleSheet(ESlateContainerStyle);
	setContentsMargins(0, 0, 0, 0);
	auto containerLayout = new QVBoxLayout(container);

	auto spinner = new SmallSpinner(this);

	auto login = new QLabel(this);
	login->setText(obs_module_text("MarketplaceWindow.LoggingIn.Title"));
	login->setStyleSheet(EBlankSlateTitleStyle);
	login->setAlignment(Qt::AlignCenter);

	auto subHLayout = new QHBoxLayout();
	auto loginSub = new QLabel(this);
	loginSub->setText(
		obs_module_text("MarketplaceWindow.LoggingIn.Subtitle"));
	loginSub->setStyleSheet(EBlankSlateSubTitleStyle);
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
	loginButton->setStyleSheet(EBlankSlateQuietButtonStyle);
	connect(loginButton, &QPushButton::clicked, this,
		[this]() { elgatoCloud->StartLogin(); });
	hLayout->addStretch();
	hLayout->addWidget(loginButton);
	hLayout->addStretch();
	containerLayout->addStretch();
	containerLayout->addWidget(spinner);
	containerLayout->addWidget(login);
	containerLayout->addLayout(subHLayout);
	containerLayout->addLayout(hLayout);
	containerLayout->addStretch();
	containerLayout->setSpacing(8);

	layout->addWidget(container);
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
	loading->setStyleSheet(EBlankSlateTitleStyle);
	loading->setFixedWidth(360);
	loading->setWordWrap(true);
	loading->setAlignment(Qt::AlignCenter);
	auto loadingLayout = new QHBoxLayout();
	loadingLayout->addStretch();
	loadingLayout->addWidget(loading);
	loadingLayout->addStretch();

	//auto spinner = new ProgressSpinner(this);
	//auto spinner = new SpinnerPanel(this, "", "", true);
	auto spinner = new SmallSpinner(this);
	spinner->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	auto spinnerLayout = new QHBoxLayout();

	layout->addStretch();
	spinnerLayout->addStretch();
	spinnerLayout->addWidget(spinner);
	spinnerLayout->addStretch();
	layout->addLayout(spinnerLayout);
	layout->addLayout(loadingLayout);
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
