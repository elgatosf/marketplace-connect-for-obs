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
#include <vector>
#include <deque>
#include <map>
#include <chrono>
#include <thread>
#include <mutex>
#include <memory>
#include <curl/curl.h>

// Requirements:
// Download to tmp file, move to final location
// Configurable automatic suspension if there is an attempt to download a file above a certain size threshold
// Track download progress and speed
// Allow download cancellation + cleanup
// Thread safe access on all public functions
// Internal downloads are done on separate thread
// Arbitrary limit on concurrent downloads
// State must be serializeable to disk and restored between sessions

// inputs- downloadFinished, downloading, fileSize, chunk, 0
typedef void (*ProgressCallbackFn)(void *, bool, bool, uint64_t, uint64_t,
				   uint64_t);

struct MoveRequestData {
	std::string first;
	std::string second;
	void *data;
};

class Downloader {
public:
	enum class Status : char {
		QUEUED,
		ERRORED,
		STOPPED,
		DOWNLOADING,
		FINISHED,
		FAILED
	};

private:
	class DownloadEntry {
	public:
		std::string url, fileName, detectedFileName;
		std::string targetDirectory,
			tmpTargetName; // Download to tmpTarget. Move to targetName unless targetName is empty
		FILE *file;
		uint64_t fileSize, downloaded;
		uint64_t logCalls;
		std::deque<std::pair<std::chrono::steady_clock::time_point,
				     uint64_t>>
			downloadedHistory;
		Downloader::Status status;
		size_t references;
		bool removed;
		std::mutex lock;

		CURL *handle;
		Downloader *parent;
		ProgressCallbackFn progressCallback;
		void *callbackData;

		DownloadEntry(Downloader *parent, std::string url,
			      std::string targetPath,
			      ProgressCallbackFn pc = nullptr,
			      void *callbackDat = nullptr);
		~DownloadEntry();
		void Finish();
		static size_t write_data(void *ptr, size_t size, size_t nmemb,
					 void *userdata);
		static size_t handle_header(void *ptr, size_t size,
					    size_t nmemb, void *userdata);
		static size_t handle_progress(void *ptr, curl_off_t dltotal,
					      curl_off_t dlnow,
					      curl_off_t ultotal,
					      curl_off_t ulnow);
		bool handleContentDisposition(const std::string &headerData);
		bool handleContentLength(const std::string &headerData);
		void updateDownloadedHistory();
	};

	friend DownloadEntry;

	std::map<size_t, std::shared_ptr<DownloadEntry>> queue;
	//std::vector<std::pair<std::string, std::string>> moveRequests;
	std::vector<MoveRequestData> moveRequests;

	size_t idCounter;
	size_t concurrentLimit;
	std::string configLocation;

	std::thread workerThread;
	static std::mutex lock;
	static std::shared_ptr<Downloader> instance;
	bool working;

	CURLM *handle;

	Downloader(std::string configLocation);

	void loadConfig();
	void dumpConfig();
	void workerJob();
	void tryDelete(decltype(queue)::iterator iter);

public:
	struct Entry {
		size_t id;
		Downloader *parent;

		std::string fileName, url;
		uint64_t fileSize, downloaded, speedBps; // bytes per second
		Status status;
		// Update the data in this struct
		void Update();
		// Stop the download
		void Stop();
		// Start or attempt to resume the download
		void Start();
		// Removes a download entirely from the list of downloads
		void Remove();

		~Entry();
	};
	friend Entry;
	~Downloader();

	static std::shared_ptr<Downloader> getInstance(std::string configPath);

	// If targetPath is empty, the file will remain as a temporary file
	// If targetPath ends with a / or \ character, the name will be automatically set
	Entry Enqueue(std::string url, std::string targetPath = "",
		      ProgressCallbackFn pc = nullptr,
		      void *callbackDat = nullptr);
	Entry Lookup(size_t id);
	std::vector<Entry> Enumerate(size_t limit = -1);

private:
	void fillEntry(Entry &dst, DownloadEntry &src);
};