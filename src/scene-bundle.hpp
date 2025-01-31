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

#ifndef SCENEBUNDLE_H
#define SCENEBUNDLE_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <utility>
#include <nlohmann/json.hpp>
#include <QMessageBox>
#include <QFileDialog>
#include <obs-frontend-api.h>

namespace miniz_cpp {
class zip_file;
}

enum class SceneBundleStatus {
	Success,
	Cancelled,
	CallerDestroyed,
	InvalidBundle,
	Error
};

class SceneBundle {
private:
	// Key: original file path, Value: new file name
	std::map<std::string, std::string> _fileMap;
	// [0]: Source, [1]: Filter
	std::vector<std::pair<std::string, std::string>> _skippedFilters;
	std::map<std::string, std::string> _videoCaptureDevices;
	nlohmann::json _collection;
	nlohmann::json _bundleInfo;
	std::string _packPath;
	bool _interrupt;
	SceneBundleStatus _interruptReason;
	bool _waiting;

public:
	SceneBundle();
	~SceneBundle();

	bool FromCollection(std::string collection_name);
	bool FromElgatoCloudFile(std::string file_path,
				 std::string destination);
	void ToCollection(std::string collection_name,
			  std::map<std::string, std::string> videoSettings,
			  std::string audioSettings, QDialog *dialog);
	SceneBundleStatus ToElgatoCloudFile(
		std::string file_path, std::vector<std::string> plugins,
		std::map<std::string, std::string> videoDeviceDescriptions);

	bool FileCheckDialog();

	inline void interrupt(SceneBundleStatus reason)
	{
		_interruptReason = reason;
		_interrupt = true;
	}

	std::string ExtractBundleInfo(std::string filePath);

	std::vector<std::string> FileList();
	std::map<std::string, std::string> VideoCaptureDevices();
	static void SceneCollectionCreated(enum obs_frontend_event event, void* obj);
	static void SceneCollectionChanged(enum obs_frontend_event event, void* obj);

private:
	void _ProcessJsonObj(nlohmann::json &obj);
	void _CreateFileMap(nlohmann::json &item);
	bool _AddFileToZip(std::string filePath, std::string zipPath,
			   miniz_cpp::zip_file &ecFile);
	bool _AddDirContentsToZip(std::string dirPath, std::string zipDir,
				  miniz_cpp::zip_file &ecFile);
	void _reset();
};

#endif // SCENEBUNDLE_H
