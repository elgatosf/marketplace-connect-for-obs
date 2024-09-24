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
#include "setup-wizard.hpp"
#include "flowlayout.h"

#include "plugin-support.h"

namespace elgatocloud {

ElgatoCloudWindow *ElgatoCloudWindow::window = nullptr;

WindowToolBar::WindowToolBar(QWidget *parent) : QWidget(parent)
{
	std::string imageBaseDir =
		obs_get_module_data_path(obs_current_module());
	imageBaseDir += "/images/";

	QPalette pal = QPalette();
	pal.setColor(QPalette::Window, "#151515");
	setAutoFillBackground(true);
	setPalette(pal);

	_layout = new QHBoxLayout(this);
	_layout->setContentsMargins(12, 12, 12, 12);
	_logo = new QLabel(this);
	std::string logoPath = imageBaseDir + "mp-logo.png";
	QPixmap logoPixmap = QPixmap(logoPath.c_str());
	_logo->setPixmap(logoPixmap);
	_layout->addWidget(_logo);

	_spacer = new QWidget(this);
	_spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	_layout->addWidget(_spacer);

	std::string settingsIconPath = imageBaseDir + "icon-settings.svg";
	QIcon settingsIcon = QIcon();
	settingsIcon.addFile(settingsIconPath.c_str(), QSize(), QIcon::Normal,
			     QIcon::Off);
	_settingsButton = new QPushButton(this);
	_settingsButton->setIcon(settingsIcon);
	_settingsButton->setIconSize(QSize(22, 22));
	_settingsButton->setStyleSheet(
		"QPushButton {background: transparent; border: none; }");
	connect(_settingsButton, &QPushButton::pressed, this,
		[this]() { emit settingsClicked(); });
	_layout->addWidget(_settingsButton);

	std::string storeIconPath = imageBaseDir + "icon-store.svg";
	QIcon storeIcon = QIcon();
	storeIcon.addFile(storeIconPath.c_str(), QSize(), QIcon::Normal,
			  QIcon::Off);
	_storeButton = new QPushButton(this);
	_storeButton->setIcon(storeIcon);
	_storeButton->setIconSize(QSize(22, 22));
	_storeButton->setStyleSheet(
		"QPushButton {background: transparent; border: none; }");
	_layout->addWidget(_storeButton);

	_logInButton = new QPushButton(this);
	_logInButton->setText("Log In");
	_logInButton->setHidden(elgatoCloud->loggedIn);
	_logInButton->setStyleSheet(
		"QPushButton {font-size: 12pt; border-radius: 8px; padding: 8px;}");
	_layout->addWidget(_logInButton);
	connect(_logInButton, &QPushButton::clicked, this,
		[this]() { elgatoCloud->StartLogin(); });

	_logOutButton = new QPushButton(this);
	_logOutButton->setText("Log Out");
	_logOutButton->setHidden(!elgatoCloud->loggedIn);
	_logOutButton->setStyleSheet(
		"QPushButton {font-size: 12pt; border-radius: 8px; padding: 8px;}");
	_layout->addWidget(_logOutButton);
	connect(_logOutButton, &QPushButton::clicked, this,
		[this]() { elgatoCloud->LogOut(); });
}

WindowToolBar::~WindowToolBar() {}

void WindowToolBar::updateState()
{
	_logInButton->setHidden(elgatoCloud->loggedIn);
	_logOutButton->setHidden(!elgatoCloud->loggedIn);
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

void ProductGrid::loadProducts()
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
	_layout = new QHBoxLayout(this);
	_sideMenu = new QListWidget(this);
	_sideMenu->addItem("Purchased");
	_sideMenu->addItem("Installed (#)");
	_sideMenu->setSizePolicy(QSizePolicy::Preferred,
				 QSizePolicy::Expanding);
	_sideMenu->setStyleSheet(
		"QListWidget { border: none; font-size: 12pt; outline: none; } QListWidget::item { padding: 12px; border-radius: 8px; } QListWidget::item:selected { background-color: #444444} QListWidget::item:hover { background-color: #656565; }");
	_sideMenu->setCurrentRow(0);
	connect(_sideMenu, &QListWidget::itemPressed, this,
		[this](QListWidgetItem *item) {
			QString val = item->text();
			if (val == "Purchased") {
				_content->setCurrentIndex(0);
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
	if (elgatoCloud->loggedIn) {
		_purchased->loadProducts();
	}
	//auto test = new QWidget(this);
	//test->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	//test->setStyleSheet("QWidget {background-color: #AAAAAA;}");
	//scroll->setWidget(test);
	scroll->setWidget(_purchased);
	_content->addWidget(scroll);
	_content->addWidget(_installed);
	_layout->addWidget(_sideMenu);
	_layout->addWidget(_content);
	setLayout(_layout);
}

OwnedProducts::~OwnedProducts() {}

void OwnedProducts::refreshProducts()
{
	if (elgatoCloud->loggedIn) {
		_purchased->loadProducts();
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
		elgatoCloud->mainWindowOpen = false;
		elgatoCloud->window = nullptr;
	}
}

void ElgatoCloudWindow::initialize()
{
	setWindowTitle(QString("Elgato Cloud"));
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
	connect(_toolbar, &WindowToolBar::settingsClicked, this, [this]() {
		_mainWidget->setVisible(false);
		_config->setVisible(true);
	});

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

	_config = new ElgatoCloudConfig(this);
	_config->setVisible(false);
	connect(_config, &ElgatoCloudConfig::closeClicked, this, [this]() {
		_mainWidget->setVisible(true);
		_config->setVisible(false);
	});

	mainLayout->addWidget(_stackedContent);
	_mainWidget->setLayout(mainLayout);

	_layout->addWidget(_mainWidget);
	_layout->addWidget(_config);

	setLayout(_layout);
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
	if (elgatoCloud->connectionError) {
		_stackedContent->setCurrentIndex(2);
	} else if (!elgatoCloud->loggedIn) {
		_stackedContent->setCurrentIndex(1);
	} else if (elgatoCloud->loading) {
		_stackedContent->setCurrentIndex(3);
	} else {
		_stackedContent->setCurrentIndex(0);
	}
	_toolbar->updateState();
}

void ElgatoCloudWindow::setupOwnedProducts()
{
	_ownedProducts->refreshProducts();
}

ElgatoProductItem::ElgatoProductItem(QWidget *parent, ElgatoProduct *product)
	: QWidget(parent),
	  _product(product)
{
	setFixedWidth(270);

	std::string imageBaseDir =
		obs_get_module_data_path(obs_current_module());
	imageBaseDir += "/images/";

	QVBoxLayout *layout = new QVBoxLayout();

	std::string imagePath = product->ready()
					? product->thumbnailPath
					: imageBaseDir + "image-loading.png";
	product->SetProductItem(this);

	QPixmap previewImage = _setupImage(imagePath);

	std::string name = _product->name.size() > 22
				   ? _product->name.substr(0, 22) + "..."
				   : _product->name;
	QLabel *label = new QLabel(name.c_str(), this);
	label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	label->setMinimumWidth(150);
	label->setStyleSheet("QLabel {font-size: 11pt;}");

	_labelDownload = new QStackedWidget(this);

	std::string downloadIconPath = imageBaseDir + "download.png";
	_downloadButton = new QPushButton(this);

	QIcon downloadIcon = QIcon();
	downloadIcon.addFile(downloadIconPath.c_str(), QSize(), QIcon::Normal,
			     QIcon::Off);
	_downloadButton->setIcon(downloadIcon);
	_downloadButton->setIconSize(QSize(22, 22));
	_downloadButton->setStyleSheet(
		"QPushButton { background: transparent; }");
	_downloadButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	//downloadButton->setDisabled(true);
	connect(_downloadButton, &QPushButton::clicked, [this]() {
		auto p = dynamic_cast<ProductGrid *>(parentWidget());
		p->disableDownload();
		_labelDownload->setCurrentIndex(1);
		_product->DownloadProduct();
	});

	_downloadProgress = new QProgressBar(this);
	_downloadProgress->setMaximum(100);
	_downloadProgress->setMinimum(0);
	_downloadProgress->setValue(0);
	_downloadProgress->setFixedHeight(22);

	auto labelLayout = new QHBoxLayout();
	labelLayout->addWidget(label);
	QWidget *spacer = new QWidget(this);
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	//spacer->setStyleSheet("QWidget { border: 1px solid #FFFFFF; }");
	labelLayout->addWidget(spacer);
	labelLayout->addWidget(_downloadButton);
	auto labelWidget = new QWidget(this);
	labelWidget->setLayout(labelLayout);

	_labelDownload->addWidget(labelWidget);
	_labelDownload->addWidget(_downloadProgress);

	_labelImg = new QLabel(this);
	_labelImg->setPixmap(previewImage);
	_labelImg->setSizePolicy(QSizePolicy::Preferred,
				 QSizePolicy::Preferred);
	layout->addWidget(_labelImg);
	layout->addWidget(_labelDownload);

	setLayout(layout);
}

ElgatoProductItem::~ElgatoProductItem() {}

void ElgatoProductItem::resetDownload()
{
	_labelDownload->setCurrentIndex(0);
	auto pw = dynamic_cast<ProductGrid *>(parentWidget());
	pw->enableDownload();
}

void ElgatoProductItem::disableDownload()
{
	_downloadButton->setVisible(false);
}

void ElgatoProductItem::enableDownload()
{
	_downloadButton->setVisible(true);
}

void ElgatoProductItem::updateImage()
{
	std::string imageBaseDir =
		obs_get_module_data_path(obs_current_module());
	imageBaseDir += "/images/";
	std::string imagePath = _product->ready()
					? _product->thumbnailPath
					: imageBaseDir + "image-loading.png";

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
		obs_log(LOG_INFO, "Download Progress: %i", progress);
		_downloadProgress->setValue(progress);
	}
}

LoginNeeded::LoginNeeded(QWidget *parent) : QWidget(parent)
{
	auto layout = new QVBoxLayout(this);

	auto topSpacer = new QWidget(this);
	topSpacer->setSizePolicy(QSizePolicy::Preferred,
				 QSizePolicy::Expanding);
	auto botSpacer = new QWidget(this);
	botSpacer->setSizePolicy(QSizePolicy::Preferred,
				 QSizePolicy::Expanding);

	auto login = new QLabel(this);
	login->setText("Please Log In");
	login->setStyleSheet("QLabel {font-size: 18pt;}");
	login->setAlignment(Qt::AlignCenter);

	auto loginSub = new QLabel(this);
	loginSub->setText(
		"Click the Log In button above to log into your account and begin using the Elgato Marketplace plugin.");
	loginSub->setWordWrap(true);
	loginSub->setAlignment(Qt::AlignCenter);

	layout->addWidget(topSpacer);
	layout->addWidget(login);
	layout->addWidget(loginSub);
	layout->addWidget(botSpacer);
}

ConnectionError::ConnectionError(QWidget *parent) : QWidget(parent)
{
	auto layout = new QVBoxLayout(this);

	auto topSpacer = new QWidget(this);
	topSpacer->setSizePolicy(QSizePolicy::Preferred,
				 QSizePolicy::Expanding);
	auto botSpacer = new QWidget(this);
	botSpacer->setSizePolicy(QSizePolicy::Preferred,
				 QSizePolicy::Expanding);

	auto connectionError = new QLabel(this);
	connectionError->setText("Connection Error");
	connectionError->setStyleSheet("QLabel {font-size: 18pt;}");
	connectionError->setAlignment(Qt::AlignCenter);

	auto connectionErrorSub = new QLabel(this);
	// TODO- CHANGE THIS TEXT IF NOT INTERNAL
	connectionErrorSub->setText(
		"Could not connect to the server. (make sure you are using the VPN to connect to staging)");
	connectionErrorSub->setWordWrap(true);
	connectionErrorSub->setAlignment(Qt::AlignCenter);

	layout->addWidget(topSpacer);
	layout->addWidget(connectionError);
	layout->addWidget(connectionErrorSub);
	layout->addWidget(botSpacer);
}

LoadingWidget::LoadingWidget(QWidget *parent) : QWidget(parent)
{
	auto layout = new QVBoxLayout(this);

	auto topSpacer = new QWidget(this);
	topSpacer->setSizePolicy(QSizePolicy::Preferred,
				 QSizePolicy::Expanding);
	auto botSpacer = new QWidget(this);
	botSpacer->setSizePolicy(QSizePolicy::Preferred,
				 QSizePolicy::Expanding);

	auto loading = new QLabel(this);
	loading->setText("Loading all of the things");
	loading->setStyleSheet("QLabel {font-size: 18pt;}");
	loading->setAlignment(Qt::AlignCenter);

	auto loadingSub = new QLabel(this);
	loadingSub->setText(
		"This will be replaced with a nice animated loading widget.");
	loadingSub->setWordWrap(true);
	loadingSub->setAlignment(Qt::AlignCenter);

	layout->addWidget(topSpacer);
	layout->addWidget(loading);
	layout->addWidget(loadingSub);
	layout->addWidget(botSpacer);
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

void CloseElgatoCloudWindow()
{
	if (elgatoCloud->mainWindowOpen) {
		ElgatoCloudWindow::window->close();
	}
}

extern void InitElgatoCloud(obs_module_t *module)
{
	obs_log(LOG_INFO, "version: %s", "0.0.1");

	elgatoCloud = new ElgatoCloud(module);
	//QAction *action = (QAction *)obs_frontend_add_tools_menu_qaction(
	//	"Elgato Cloud Window");
	//action->connect(action, &QAction::triggered, OpenElgatoCloudWindow);
}

} // namespace elgatocloud
