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

#include "elgato-product.hpp"
#include <QMainWindow>
#include <obs-module.h>
#include <util/platform.h>
#include <curl/curl.h>
#include <memory>
#include <fstream>
#include <iterator>
#include <iostream>
#include <QApplication>
#include <QThread>
#include <QMetaObject>
#include <QInputDialog>
#include <QDir>
#include "scene-bundle.hpp"

#include <obs-frontend-api.h>

#include <plugin-support.h>
#include "elgato-cloud-data.hpp"
#include "elgato-cloud-window.hpp"
#include "util.h"
#include "setup-wizard.hpp"


namespace elgatocloud {

ElgatoProduct::ElgatoProduct(nlohmann::json &productData) : _fileSize(0)
{
	thumbnailPath = QDir::homePath().toStdString();
	thumbnailPath += "/AppData/Local/Elgato/MarketplaceConnect/Thumbnails";
	os_mkdirs(thumbnailPath.c_str());

	std::string savePath = QDir::homePath().toStdString();
	savePath += "/AppData/Local/Elgato/MarketplaceConnect/Downloads";
	os_mkdirs(savePath.c_str());

	_thumbnailReady = false;

	name = productData["name"];
	thumbnailUrl = productData["thumbnail_cdn"];
	variantId = productData["variants"][0]["id"];

	auto found = thumbnailUrl.find_last_of("/");
	if (found == std::string::npos) {
		return;
	}
	auto filename = thumbnailUrl.substr(found + 1);
	thumbnailPath = thumbnailPath + "/" + filename;

	if (!os_file_exists(thumbnailPath.c_str())) {
		_downloadThumbnail();
	} else {
		_thumbnailReady = true;
	}
}

ElgatoProduct::ElgatoProduct(std::string collectionName)
	: name(collectionName),
	  thumbnailUrl(""),
	  variantId(""),
	  _fileSize(0)
{
	thumbnailPath = "";
	_thumbnailReady = true;
}

bool ElgatoProduct::DownloadProduct()
{
	auto ec = GetElgatoCloud();
	nlohmann::json dlData = ec->GetPurchaseDownloadLink(variantId);
	if (dlData.contains("error")) {
		// Pop up a modal telling the user the download couldn't happen.
		QMessageBox msgBox;
		msgBox.setText(
			obs_module_text("MarketplaceWindow.DownloadProduct.NetworkError.Title"));
		msgBox.setInformativeText(
			obs_module_text("MarketplaceWindow.DownloadProduct.NetworkError.Subtitle"));
		msgBox.exec();
		return false;
	}
	std::string url = dlData["direct_link"];
	_fileSize = dlData["file_size"];

	if (url.find(".elgatoscene") == std::string::npos) {
		obs_log(LOG_INFO, "Invalid File");
		if (_productItem) {
			_productItem->resetDownload();
		}
		QMessageBox msgBox;
		msgBox.setText(
			obs_module_text("MarketplaceWindow.DownloadProduct.InvalidFiletype.Title"));
		msgBox.setInformativeText(
			obs_module_text("MarketplaceWindow.DownloadProduct.InvalidFiletype.Subtitle"));
		msgBox.exec();
		return false;
	}

	std::string savePath = QDir::homePath().toStdString();
	savePath += "/AppData/Local/Elgato/MarketplaceConnect/Downloads/";
	os_mkdirs(savePath.c_str());

	std::shared_ptr<Downloader> dl = Downloader::getInstance("");
	auto download = dl->Enqueue(url, savePath, ElgatoProduct::DownloadProgress, nullptr, this);
	downloadId_ = download.id;
	return true;
}

void ElgatoProduct::StopProductDownload()
{
	std::shared_ptr<Downloader> dl = Downloader::getInstance("");
	auto download = dl->Lookup(downloadId_);
	download.Stop();
}

void ElgatoProduct::_downloadThumbnail()
{
	std::string savePath = QDir::homePath().toStdString();
	savePath += "/AppData/Local/Elgato/MarketplaceConnect/Thumbnails/";
	std::shared_ptr<Downloader> dl = Downloader::getInstance("");
	dl->Enqueue(thumbnailUrl, savePath, ElgatoProduct::ThumbnailProgress, ElgatoProduct::SetThumbnail,
		    this);
}

void ElgatoProduct::ThumbnailProgress(void *ptr, bool finished,
				      bool downloading, uint64_t fileSize,
				      uint64_t chunkSize, uint64_t downloaded)
{
	UNUSED_PARAMETER(ptr);
	UNUSED_PARAMETER(finished);
	UNUSED_PARAMETER(downloading);
	UNUSED_PARAMETER(fileSize);
	UNUSED_PARAMETER(chunkSize);
	UNUSED_PARAMETER(downloaded);
}

void ElgatoProduct::DownloadProgress(void *ptr, bool finished, bool downloading,
				     uint64_t fileSize, uint64_t chunkSize,
				     uint64_t downloaded)
{
	UNUSED_PARAMETER(finished);
	UNUSED_PARAMETER(downloading);
	UNUSED_PARAMETER(chunkSize);
	double percent = 100.0 * static_cast<double>(downloaded) /
			 static_cast<double>(fileSize);
	int pct = static_cast<int>(percent);
	ElgatoProduct &self = *static_cast<ElgatoProduct *>(ptr);
	QMetaObject::invokeMethod(QCoreApplication::instance()->thread(),
				  [self, downloading, pct]() {
					  self._productItem->UpdateDownload(
						  downloading, pct);
				  });
}

void ElgatoProduct::SetThumbnail(std::string filename, void *data)
{
	auto ep = static_cast<ElgatoProduct *>(data);
	ep->_thumbnailReady = true;
	if (ep->_productItem) {
		QMetaObject::invokeMethod(
			QCoreApplication::instance()->thread(),
			[ep]() { ep->_productItem->updateImage(); });
	}
}

void ElgatoProduct::Install(std::string filename_utf8, void *data,
			    bool fromDownload)
{
	auto ep = static_cast<ElgatoProduct *>(data);
	const auto mainWindow =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (ep->_productItem) {
		QMetaObject::invokeMethod(
			QCoreApplication::instance()->thread(),
			[ep]() { ep->_productItem->resetDownload(); });
	}
	const QRect &hostRect = mainWindow->geometry();
	if (GetSetupWizard()) {
		return;
	}
	StreamPackageSetupWizard *setupWizard = new StreamPackageSetupWizard(
		mainWindow, ep, filename_utf8, fromDownload);
	setupWizard->setAttribute(Qt::WA_DeleteOnClose);
	setupWizard->show();
	setupWizard->move(hostRect.center() - setupWizard->rect().center());
	setupWizard->OpenArchive();
}

} // namespace elgatocloud
