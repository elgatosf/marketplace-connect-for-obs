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

#include <Qt>
#include <QGraphicsOpacityEffect>
#include <QHoverEvent>
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
#include <QMenu>
#include <QPixmap>
#include "elgato-product.hpp"
#include "elgato-cloud-config.hpp"
#include "flowlayout.h"
#include "ui_elgato-cloud-window.h"

namespace elgatocloud {

class Avatar : public QWidget {
	Q_OBJECT
public:
	Avatar(QWidget *parent);
	void update();

signals:
	void clicked();

protected:
	void paintEvent(QPaintEvent *e);
	void mousePressEvent(QMouseEvent* event) override;

private:
	std::string _character;
	std::string _bgColor;
};

class AvatarImage : public QWidget {
	Q_OBJECT
public:
	AvatarImage(QWidget* parent);
	void update();

signals:
	void clicked();

protected:
	void mousePressEvent(QMouseEvent* event) override;

private:
	QPixmap _setupImage(std::string imagePath);

	QLabel* _avatarImg;
};

class UserMenu : public QWidget {
	Q_OBJECT
public:
	UserMenu(QWidget* parent);

signals:
	void viewAccountClicked();
	void signOutClicked();

private:
	QMenu* _menu;
	QStackedWidget* _stack;
	Avatar* _plainAvatar;
	AvatarImage* _imgAvatar;
	QAction* _accountTitle;

private slots:
	void _showModalMenu();

};

class ProgressThumbnail : public QLabel {
	Q_OBJECT
public:
	ProgressThumbnail(float hoverOpacity, bool hoverDisabled, QWidget* parent = nullptr);
	void setCustomPixmap(const QPixmap& pixmap);
	void setCustomPixmap(const QPixmap& pixmap, const QSize& size);
	void setProgress(float progress);
	void setDownloading(bool downloading);
	void onHoverEnter(QHoverEvent* event);
	void onHoverLeave(QHoverEvent* event);
	void disableHover();
	void enableHover();
	QSize sizeHint() const override;
	virtual void setDisabled(bool disabled);
protected:
	void paintEvent(QPaintEvent* event) override;
	virtual void resizeEvent(QResizeEvent* event);

private:
	QPixmap _pixmap, _pixmapScaled, _pixmapScaledDisabled;
	float _progress = 0.5f;
	float _hoverOpacity;
	float _opacity;
	bool _hoverDisabled;
	bool _disabled = false;
	bool _downloading = false;
};

class ProductThumbnail : public QWidget {
	Q_OBJECT

public:
	ProductThumbnail(QWidget* parent, const QPixmap& pixmap);
	~ProductThumbnail();
	void setPixmap(const QPixmap& pixmap);
	inline void updateDownloadProgress(float progress) { _thumbnail->setProgress(progress); }
	void disable(bool disable);
	void enable(bool enable);
	void setDownloading(bool downloading);
	QSize sizeHint() const override;

signals:
	void downloadClicked();
	void cancelDownloadClicked();

protected:
	bool event(QEvent* e);
	void resizeEvent(QResizeEvent* event) override;

private:
	void updateButton_();
	ProgressThumbnail* _thumbnail;
	QPushButton* _downloadButton;
	QPushButton* _stopDownloadButton;
	bool _disable = false;
	bool _downloading = false;
};

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
	void closing();

private:
	ElgatoProduct* _product;
	ProductThumbnail* _labelImg;
	QPixmap _setupImage(std::string imagePath);
};

class WindowToolBar : public QWidget {
	Q_OBJECT

public:
	WindowToolBar(QWidget *parent);
	~WindowToolBar();
	void updateState();
	void disableLogout(bool disabled);

private:
	QHBoxLayout *_layout;
	QLabel *_logo;
	QPushButton *_settingsButton;
	QPushButton *_storeButton;
	QPushButton *_logInButton;
	QPushButton *_logOutButton;
	Avatar *_avatar;
	AvatarImage* _avatarImage;
	UserMenu* _userMenu;

signals:
	void settingsClicked();
};

class ProductGrid : public QWidget {
	Q_OBJECT
public:
	ProductGrid(QWidget *parent);
	~ProductGrid();
	size_t loadProducts();
	void disableDownload(ElgatoProductItem* skip = nullptr);
	void enableDownload();
	void resetDownloads();
	void closing();

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

class CurrentCollection : public QWidget {
	Q_OBJECT
public:
	CurrentCollection(std::string scName, nlohmann::json data, QWidget *parent = nullptr);
};

class OwnedProducts : public QWidget {
	Q_OBJECT
public:
	OwnedProducts(QWidget *parent);
	~OwnedProducts();

	void refreshProducts();
	void closing();
	void resetDownloads();

private:
	QHBoxLayout *_layout = nullptr;
	QListWidget *_sideMenu = nullptr;
	QStackedWidget *_content = nullptr;
	Placeholder *_installed = nullptr;
	ProductGrid *_purchased = nullptr;
	size_t _numProducts = 0;
};

class LoginNeeded : public QWidget {
	Q_OBJECT
public:
	LoginNeeded(QWidget *parent);
};

class LoginError : public QWidget {
	Q_OBJECT
public:
	LoginError(QWidget* parent);
};

class LoggingIn : public QWidget {
	Q_OBJECT
public:
	LoggingIn(QWidget* parent);
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
	bool loading = true;

	explicit ElgatoCloudWindow(QWidget *parent = nullptr);
	~ElgatoCloudWindow();

	void initialize();
	void setLoggedIn();
	void setLoading();
	void setupOwnedProducts();
	void resetDownloads();

	static ElgatoCloudWindow *window;

public slots:
	void on_logInButton_clicked();
	void on_logOutButton_clicked();

protected:
	void closeEvent(QCloseEvent* event) override;

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
ElgatoCloudWindow *GetElgatoCloudWindow();
void ElgatoCloudWindowSetEnabled(bool enable);
void CheckForUpdates(bool forceCheck);
void CheckForUpdatesOnLaunch(enum obs_frontend_event event, void* private_data);


void OpenElgatoDDTestWindow();

class DraggableListWidget : public QListWidget
{
	Q_OBJECT
public:
	using QListWidget::QListWidget;

protected:
	void startDrag(Qt::DropActions supportedActions) override;
};


class ElgatoDDTest : public QDialog {
	Q_OBJECT

public:
	explicit ElgatoDDTest(QWidget* parent = nullptr);
	~ElgatoDDTest();

protected:
	void dragEnterEvent(QDragEnterEvent* event) override;
	void dragMoveEvent(QDragMoveEvent* event) override;
	void dropEvent(QDropEvent* event) override;
	void closeEvent(QCloseEvent* event) override;  // cleanup hook

private:
	QListWidget* fileList_;
	QLabel* hint_;

	void cleanupTempFiles();
};

} // namespace elgatocloud
