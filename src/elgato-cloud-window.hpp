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
#include <QStackedWidget>
#include <QProgressBar>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QListWidget>
#include <QPixmap>
#include "elgato-product.hpp"
#include "elgato-cloud-config.hpp"
#include "flowlayout.h"
#include "ui_elgato-cloud-window.h"

namespace elgatocloud {

class ElgatoProductItem : public QWidget {
	Q_OBJECT

public:
	ElgatoProductItem(QWidget *parent, ElgatoProduct *product);
	~ElgatoProductItem();
	void UpdateDownload(bool downloading, int progress);
	void updateImage();
	void resetDownload();
	void disableDownload();
	void enableDownload();

private:
	ElgatoProduct *_product;
	QStackedWidget *_downloadWidget;
	QProgressBar *_downloadProgress;
	QPushButton* _downloadButton;
	QLabel *_labelImg;
	QStackedWidget *_labelDownload;
	QPixmap _setupImage(std::string imagePath);
};

class WindowToolBar : public QWidget {
	Q_OBJECT

public:
	WindowToolBar(QWidget *parent);
	~WindowToolBar();
	void updateState();

private:
	QHBoxLayout *_layout;
	QLabel *_logo;
	QWidget *_spacer;
	QPushButton *_settingsButton;
	QPushButton *_storeButton;
	QLineEdit *_searchBar;
	QPushButton *_logInButton;
	QPushButton *_logOutButton;

signals:
	void settingsClicked();
};

class ProductGrid : public QWidget {
	Q_OBJECT
public:
	ProductGrid(QWidget *parent);
	~ProductGrid();
	void loadProducts();
	void disableDownload();
	void enableDownload();
private:
	FlowLayout *_layout;
};

class Placeholder : public QWidget {
	Q_OBJECT
public:
	Placeholder(QWidget *parent, std::string message);
	~Placeholder();

private:
	QVBoxLayout *_layout;
};

class OwnedProducts : public QWidget {
	Q_OBJECT
public:
	OwnedProducts(QWidget *parent);
	~OwnedProducts();

	void refreshProducts();

private:
	QHBoxLayout *_layout = nullptr;
	QListWidget *_sideMenu = nullptr;
	QStackedWidget *_content = nullptr;
	Placeholder *_installed = nullptr;
	ProductGrid *_purchased = nullptr;
};

class LoginNeeded : public QWidget {
	Q_OBJECT
public:
	LoginNeeded(QWidget *parent);
};

class ConnectionError : public QWidget {
	Q_OBJECT
public:
	ConnectionError(QWidget *parent);
};

class LoadingWidget : public QWidget {
	Q_OBJECT
public:
	LoadingWidget(QWidget *parent);
};

/***************************************/
/* Elgato Cloud Window                 */
/***************************************/
class ElgatoCloudWindow : public QDialog {
	Q_OBJECT

public:
	//std::unique_ptr<Ui_ElgatoCloudWindow> ui;
	bool loading = true;

	explicit ElgatoCloudWindow(QWidget *parent = nullptr);
	~ElgatoCloudWindow();

	void initialize();
	void setLoggedIn();
	void setupOwnedProducts();

	static ElgatoCloudWindow *window;

public slots:
	void on_logInButton_clicked();
	void on_logOutButton_clicked();

private:
	QVBoxLayout *_layout = nullptr;
	QWidget *_mainWidget = nullptr;
	QWidget *_configWidget = nullptr;

	WindowToolBar *_toolbar = nullptr;
	QStackedWidget *_stackedContent = nullptr;
	OwnedProducts *_ownedProducts = nullptr;
	ElgatoCloudConfig *_config = nullptr;
};

void OpenElgatoCloudWindow();
void CloseElgatoCloudWindow();
//QWidget *GetElgatoCloudWindow();

} // namespace elgatocloud
