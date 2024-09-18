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
#include "scene-bundle.hpp"

#include <obs-frontend-api.h>

#include "plugin-support.h"
#include "downloader.h"
#include "elgato-cloud-data.hpp"
#include "elgato-cloud-window.hpp"
#include "util.h"
#include "setup-wizard.hpp"

namespace elgatocloud {

ElgatoProduct::ElgatoProduct(nlohmann::json &productData)
{
	thumbnailPath = obs_get_module_data_path(obs_current_module());
	thumbnailPath += "/image_cache";
	os_mkdirs(thumbnailPath.c_str());
	std::string tmpPath = obs_get_module_data_path(obs_current_module());
	tmpPath += "/tmp";
	os_mkdirs(tmpPath.c_str());
	_thumbnailReady = false;

	name = productData["name"];
	thumbnailUrl = productData["thumbnail"];
	variantId = productData["variants"][0]["id"];

	auto found = thumbnailUrl.find_last_of("/");
	if (found == std::string::npos) {
		return;
	}
	auto filename = thumbnailUrl.substr(found + 1);
	thumbnailPath = thumbnailPath + "/" + filename;

	if (!os_file_exists(thumbnailPath.c_str())) {
		obs_log(LOG_INFO, "Downloading thumbnail %s", filename.c_str());
		_downloadThumbnail();
	} else {
		obs_log(LOG_INFO, "Thumbnail Exists %s", filename.c_str());
		_thumbnailReady = true;
	}
}

void ElgatoProduct::DownloadProduct()
{
	auto ec = GetElgatoCloud();
	nlohmann::json dlData = ec->GetPurchaseDownloadLink(variantId);
	std::string url = dlData["direct_link"];
	_fileSize = dlData["file_size"];
	obs_log(LOG_INFO, "Download Link: %s", url.c_str());

	if (url.find(".elgatoscene") == std::string::npos) {
		obs_log(LOG_INFO, "Invalid File");
		if (_productItem) {
			_productItem->resetDownload();
		}
		QMessageBox msgBox;
		msgBox.setText("Invalid File Type");
		msgBox.setInformativeText("File is not an .elgatoscene file.");
		msgBox.exec();
		return;
	}

	std::string savePath = obs_get_module_data_path(obs_current_module());
	savePath += "/tmp/";

	char *absPath = os_get_abs_path_ptr(savePath.c_str());
	savePath = std::string(absPath, strlen(absPath));
	bfree(absPath);

	obs_log(LOG_INFO, "Saving to: %s", savePath.c_str());

	std::shared_ptr<Downloader> dl = Downloader::getInstance("");
	dl->Enqueue(url, savePath, ElgatoProduct::DownloadProgress, this);
}

void ElgatoProduct::_downloadThumbnail()
{
	obs_log(LOG_INFO, "Downloading thumbnail: %s", thumbnailUrl.c_str());
	std::string savePath = obs_get_module_data_path(obs_current_module());
	savePath += "/image_cache/";

	char *absPath = os_get_abs_path_ptr(savePath.c_str());
	savePath = std::string(absPath, strlen(absPath));
	bfree(absPath);

	obs_log(LOG_INFO, "Saving to: %s", savePath.c_str());
	obs_log(LOG_INFO, "this: %i", reinterpret_cast<size_t>(this));

	std::shared_ptr<Downloader> dl = Downloader::getInstance("");
	dl->Enqueue(thumbnailUrl, savePath, ElgatoProduct::ThumbnailProgress,
		    this);
}

void ElgatoProduct::ThumbnailProgress(void *ptr, bool finished,
				      bool downloading, uint64_t fileSize,
				      uint64_t chunkSize, uint64_t downloaded)
{
	UNUSED_PARAMETER(ptr);
	UNUSED_PARAMETER(downloading);
	UNUSED_PARAMETER(fileSize);
	UNUSED_PARAMETER(chunkSize);
	UNUSED_PARAMETER(downloaded);
	if (finished) {
		obs_log(LOG_INFO, "FINISHED DOWNLOADING THUMBNAIL!");
		obs_log(LOG_INFO, "address: %i", reinterpret_cast<size_t>(ptr));
	}
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
	obs_log(LOG_INFO, "Thumbnail downloaded, %s", filename.c_str());
	obs_log(LOG_INFO, "data: %i", reinterpret_cast<std::size_t>(data));
}

void ElgatoProduct::Install(std::string filename_utf8, void *data)
{
	auto ep = static_cast<ElgatoProduct *>(data);
	const auto mainWindow =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (ep->_productItem) {
		QMetaObject::invokeMethod(
			QCoreApplication::instance()->thread(),
			[ep]() {
				ep->_productItem->resetDownload();
			});
	}
	const QRect &hostRect = mainWindow->geometry();
	if (GetSetupWizard()) {
		obs_log(LOG_INFO, "Setup Wizard already active.");
		return;
	}
	StreamPackageSetupWizard *setupWizard =
		new StreamPackageSetupWizard(mainWindow, ep, filename_utf8);
	setupWizard->setAttribute(Qt::WA_DeleteOnClose);
	setupWizard->show();
	setupWizard->move(hostRect.center() - setupWizard->rect().center());
}

} // namespace elgatocloud
