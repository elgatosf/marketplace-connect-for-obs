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

#include "downloader.h"
#include "platform.h"
#include "util.h"
#include "plugin-support.h"
#include "elgato-product.hpp"
#include <cstring>
#include <curl/curl.h>

#include <QApplication>
#include <QThread>
#include <QMetaObject>
// TODO: Hash file downloads for safe resumes

std::shared_ptr<Downloader> Downloader::instance{nullptr};
std::mutex Downloader::lock;

size_t Downloader::DownloadEntry::write_data(void *ptr, size_t size,
					     size_t nmemb, void *userdata)
{
	DownloadEntry &self = *static_cast<DownloadEntry *>(userdata);
	//Sleep(100);
	std::unique_lock l(self.lock);
	self.downloaded += nmemb;
	if (self.progressCallback) {
		self.progressCallback(self.callbackData, false, true,
				      self.fileSize, nmemb, self.downloaded);
	}
	return fwrite(ptr, size, nmemb, self.file);
}

bool Downloader::DownloadEntry::handleContentDisposition(
	const std::string &headerData)
{
	char header[] = "attachment; filename=";
	if (sizeof(header) > headerData.size()) {
		return false;
	}
	if (strnicmp(header, headerData.c_str(), sizeof(header) - 1) != 0) {
		return false;
	}
	size_t offset = sizeof(header) - 1;
	if (headerData[offset] == '"') {
		offset += 1;
	}
	detectedFileName = headerData.substr(offset);
	auto quotePos = detectedFileName.find_last_not_of("\" \t\r\n");
	if (quotePos != std::string::npos) {
		detectedFileName.resize(quotePos + 1);
	}
	// Truncate any leading path info to avoid any funny business
	offset = detectedFileName.find_last_of("\\/");
	if (offset != std::string::npos) {
		detectedFileName = detectedFileName.substr(offset + 1);
	}
	return true;
}
bool Downloader::DownloadEntry::handleContentLength(
	const std::string &headerData)
{
	auto size = std::atoll(headerData.c_str());
	if (size < 0) {
		return false;
	}
	fileSize = (size_t)size;
	return true;
}

size_t Downloader::DownloadEntry::handle_header(void *ptr, size_t size,
						size_t nmemb, void *userdata)
{
	// Grab the filename sent from the server
	// TODO: support extended tags? e.g. explicitly marked UTF-8?

	DownloadEntry &self = *static_cast<DownloadEntry *>(userdata);
	std::unique_lock l(self.lock);

	std::string headerLine = std::string((char *)ptr, size * nmemb);
	auto breakPos = headerLine.find_first_of(':');
	if (breakPos == std::string::npos) {
		return size * nmemb;
	}

	auto headerName = headerLine.substr(0, breakPos);
	breakPos = headerLine.find_first_not_of(" \t\n\r", breakPos + 1);
	auto headerContent = headerLine.substr(breakPos);

	for (auto &c : headerName) {
		c = (char)tolower(c);
	}

	bool result = false;
	if (headerName == "content-disposition") {
		result = self.handleContentDisposition(headerContent);
	} else if (headerName == "content-length") {
		result = self.handleContentLength(headerContent);
	}

	return size * nmemb;
}

size_t Downloader::DownloadEntry::handle_progress(void *ptr, curl_off_t dltotal,
						  curl_off_t dlnow,
						  curl_off_t ultotal,
						  curl_off_t ulnow)
{
	DownloadEntry &self = *static_cast<DownloadEntry *>(ptr);
	std::unique_lock l(self.lock);
	UNUSED_PARAMETER(ultotal);
	UNUSED_PARAMETER(ulnow);
	//double pct = (int)((double)dlnow / (double)dltotal * 100.0);
	obs_log(LOG_INFO, "%s: %i of %i", self.url.c_str(), dlnow, dltotal);
	return CURL_PROGRESSFUNC_CONTINUE;
}

Downloader::DownloadEntry::DownloadEntry(Downloader *parent, std::string url,
					 std::string targetPath,
					 ProgressCallbackFn pc,
					 void *callbackDat)
	: url(url),
	  file(nullptr),
	  fileSize(0),
	  downloaded(0),
	  status(Downloader::Status::QUEUED),
	  references(0),
	  removed(false),
	  progressCallback(pc),
	  callbackData(callbackDat),
	  parent(parent)
{
	handle = curl_easy_init();
	curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, static_cast<void *>(this));
	curl_easy_setopt(handle, CURLOPT_USERAGENT, "elgato-cloud 0.0");
	curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, handle_header);
	curl_easy_setopt(handle, CURLOPT_HEADERDATA, static_cast<void *>(this));
	curl_easy_setopt(handle, CURLOPT_PRIVATE, static_cast<void *>(this));

	file = open_tmp_file("wb", tmpTargetName);
	status = Downloader::Status::DOWNLOADING;

	if (!targetPath.empty()) {
		auto slashPos = targetPath.find_last_of("\\/");
		if (slashPos != std::string::npos) {
			if (slashPos == targetPath.size() - 1) {
				// This is a directory, we'll need to generate the name.
				targetDirectory = targetPath;
			} else if (is_directory(targetPath)) {
				// This is also a directory, we'll need to generate the name.
				targetDirectory = targetPath + '/';
			} else {
				// Harvest name
				fileName = targetPath.substr(slashPos + 1);
				targetDirectory =
					targetPath.substr(0, slashPos + 1);
			}
		}
	}
	// Extract file name from URL
	if (!url.empty()) {
		auto querypos = url.find_first_of('?');
		auto slashpos = url.find_last_of('/', querypos);
		auto start = slashpos;
		auto count = querypos;
		if (start != std::string::npos) {
			start += 1;
			if (count != std::string::npos) {
				count = count - start;
			}
			detectedFileName = url.substr(start, count);
			obs_log(LOG_INFO, "Detected filename: %s",
				detectedFileName.c_str());
		}
	}

	curl_multi_add_handle(parent->handle, handle);
}

Downloader::DownloadEntry::~DownloadEntry()
{

	std::unique_lock l(lock);
	if (status == Status::DOWNLOADING) {
		curl_multi_remove_handle(parent->handle, handle);
		curl_easy_cleanup(handle);
	}
}

void Downloader::DownloadEntry::Finish()
{
	std::unique_lock l(lock);
	status = Status::FINISHED;
	curl_multi_remove_handle(parent->handle, handle);
	curl_easy_cleanup(handle);
	fclose(file);
	if (fileName == "") {
		fileName = detectedFileName;
	}

	if (targetDirectory != "") {
		obs_log(LOG_INFO, "Saving file to: %s",
			(targetDirectory + fileName).c_str());
		if (fileName == "") {
			fileName = random_name();
		}
		parent->moveRequests.push_back({tmpTargetName,
						targetDirectory + fileName,
						callbackData});
	}
	if (progressCallback) {
		progressCallback(callbackData, true, false, fileSize, 0,
				 fileSize);
	}
}
void Downloader::DownloadEntry::updateDownloadedHistory()
{
	std::unique_lock l(lock);
	auto threshold =
		std::chrono::steady_clock::now() - std::chrono::seconds(5);
	while (downloadedHistory.size() > 4 &&
	       downloadedHistory.front().first < threshold) {
		downloadedHistory.pop_front();
	}
	downloadedHistory.push_back(
		{std::chrono::steady_clock::now(), downloaded});
}

void Downloader::loadConfig()
{
	// TODO: Implement
}
void Downloader::dumpConfig()
{
	// TODO: Implement
}
Downloader::Downloader(std::string configLocation)
	: idCounter(1),
	  concurrentLimit(10),
	  configLocation(configLocation),
	  working(true)
{
	loadConfig();
	handle = curl_multi_init();
	workerThread = std::thread{&Downloader::workerJob, this};
}
Downloader::~Downloader()
{
	std::unique_lock lockerA(lock);
	working = false;
	lockerA.unlock();

	workerThread.join();

	curl_multi_cleanup(handle);
}

std::shared_ptr<Downloader> Downloader::getInstance(std::string configLocation)
{
	std::lock_guard<std::mutex> l(lock);
	if (instance == nullptr) {
		std::shared_ptr<Downloader> newInstance(
			new Downloader(configLocation));
		newInstance.swap(instance);
	}
	return instance;
}

Downloader::Entry Downloader::Enqueue(std::string url, std::string targetPath,
				      ProgressCallbackFn pc, void *callbackDat)
{
	std::unique_lock l(lock);

	auto dlentry = std::make_shared<DownloadEntry>(this, url, targetPath,
						       pc, callbackDat);
	auto result = queue.emplace(idCounter++, dlentry);
	if (result.first == queue.end() || result.second == false) {
		throw "TODO";
	}

	Entry e{};
	e.id = result.first->first;
	e.parent = this;
	fillEntry(e, *result.first->second);
	result.first->second->references++;
	l.unlock();
	return e;
}
Downloader::Entry Downloader::Lookup(size_t id)
{
	std::unique_lock l(lock);
	auto dle = queue.find(id);
	Entry result{};
	if (dle != queue.end()) {
		if (dle->second->removed) {
			return result;
		}
		fillEntry(result, *dle->second);
		dle->second->references++;
	}
	l.unlock();
	return result;
}
std::vector<Downloader::Entry> Downloader::Enumerate(size_t limit)
{

	std::unique_lock l(lock);
	std::vector<Entry> result;
	for (auto &e : queue) {
		if (e.second->removed) {
			continue;
		}
		result.push_back({});
		auto &entry = result.back();
		entry.id = e.first;
		entry.parent = this;
		fillEntry(entry, *e.second);
		e.second->references++;
		if (limit-- == 0) {
			break;
		}
	}
	l.unlock();
	return result;
}

void Downloader::workerJob()
{
	int active_transfers;
	do {
		curl_multi_wait(handle, NULL, 0, 1000, NULL);
		curl_multi_perform(handle, &active_transfers);
		std::unique_lock l(lock);
		for (auto &entry : queue) {
			entry.second->updateDownloadedHistory();
		}
		int msgs = 0;
		CURLMsg *msg = nullptr;
		while (true) {
			msg = curl_multi_info_read(handle, &msgs);
			if (!msg) {
				break;
			}
			if (msg->msg == CURLMSG_DONE) {
				void *info;
				curl_easy_getinfo(msg->easy_handle,
						  CURLINFO_PRIVATE, &info);
				if (info == nullptr) {
					continue;
				}
				DownloadEntry &dle = *(DownloadEntry *)info;
				dle.Finish();
			}
		}
		auto newMoveRequests = std::move(moveRequests);
		l.unlock();
		for (auto &mr : newMoveRequests) {
			std::string file = move_file_safe(mr.first, mr.second);
			auto pos = file.rfind(".");
			if (pos != std::string::npos &&
			    file.substr(pos + 1) == "elgatoscene") {
				QMetaObject::invokeMethod(
					QCoreApplication::instance()->thread(),
					[file, mr]() {
						elgatocloud::ElgatoProduct::
							Install(file, mr.data);
					});
			} else {
				// We are downloading a thumbnail.
				elgatocloud::ElgatoProduct::SetThumbnail(
					file, mr.data);
			}
		}
	} while (working);
}

void Downloader::fillEntry(Downloader::Entry &dst,
			   Downloader::DownloadEntry &src)
{
	dst.fileName = src.fileName;
	dst.url = src.url;
	dst.fileSize = src.fileSize;
	dst.downloaded = src.downloaded;
	dst.status = src.status;

	if (src.downloadedHistory.size() > 1) {
		auto first = src.downloadedHistory.front();
		auto last = src.downloadedHistory.back();
		auto interval =
			std::chrono::duration_cast<std::chrono::seconds>(
				last.first - first.first)
				.count();
		auto bytesDownloaded = last.second - first.second;
		dst.speedBps = bytesDownloaded / interval;
	}
}

void Downloader::Entry::Update()
{
	std::unique_lock l(parent->lock);
	auto dle = parent->queue.find(id);
	if (dle != parent->queue.end()) {
		parent->fillEntry(*this, *dle->second);
	}
}

void Downloader::Entry::Stop()
{
	std::unique_lock l(parent->lock);
	auto dlentry = parent->queue.find(id);
	if (dlentry != parent->queue.end()) {
		if (dlentry->second->status == Status::DOWNLOADING) {
			curl_multi_remove_handle(parent->handle,
						 dlentry->second->handle);
			dlentry->second->status = Status::STOPPED;
		}
	}
}
void Downloader::Entry::Start()
{
	std::unique_lock l(parent->lock);
	auto dlentry = parent->queue.find(id);
	if (dlentry != parent->queue.end()) {
		if (dlentry->second->status == Status::STOPPED) {
			curl_multi_add_handle(parent->handle,
					      dlentry->second->handle);
			dlentry->second->status = Status::DOWNLOADING;
		}
	}
}
void Downloader::Entry::Remove()
{
	std::unique_lock l(parent->lock);
	auto dlentry = parent->queue.find(id);
	if (dlentry != parent->queue.end()) {
		dlentry->second->removed = true;
	}
}

void Downloader::tryDelete(decltype(Downloader::queue)::iterator iter)
{
	if (iter->second->references <= 0 && iter->second->removed) {
		queue.erase(iter);
	}
}

Downloader::Entry::~Entry()
{
	std::unique_lock l(parent->lock);
	auto dlentry = parent->queue.find(id);
	if (dlentry != parent->queue.end()) {
		dlentry->second->references--;
		parent->tryDelete(dlentry);
	}
}