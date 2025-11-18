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

struct SdaFileInfo;

enum class SdFileVersion {
	Legacy,
	Current
};

struct SceneInfo {
	std::string name;
	std::string id;
	bool outputScene;
};

struct ThirdPartyRequirement {
	std::string name;
	std::string url;
};

struct SceneCollectionInfo {
	std::string bundleInfo;
	std::string streamDeckPath;
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
	bool ToCollection(std::string collection_name,
			  std::map<std::string, std::string> videoSettings,
			  std::string audioSettings, std::string id,
			  std::string productName, std::string productId,
			  std::string productSlug);
	bool MergeCollection(std::string collection_name,
			  std::vector<std::string> scenes,
			  std::map<std::string, std::string> videoSettings,
			  std::string audioSettings, std::string id);
	SceneBundleStatus ToElgatoCloudFile(
		std::string file_path, std::vector<std::string> plugins,
		std::vector<std::pair<std::string, std::string>> thirdParty,
		std::vector<SceneInfo> outputScenes,
		std::map<std::string, std::string> videoDeviceDescriptions,
		std::vector<SdaFileInfo> sdaFiles,
		std::vector<SdaFileInfo> sdProfileFiles,
		std::string version);

	bool FileCheckDialog();

	inline void interrupt(SceneBundleStatus reason)
	{
		_interruptReason = reason;
		_interrupt = true;
	}

	SceneCollectionInfo ExtractBundleInfo(std::string filePath);

	std::vector<std::string> FileList();
	std::map<std::string, std::string> VideoCaptureDevices();
	static void SceneCollectionCreated(enum obs_frontend_event event,
					   void *obj);
	static void SceneCollectionChanged(enum obs_frontend_event event,
					   void *obj);

private:
	void _ProcessJsonObj(nlohmann::json &obj);
	void _CreateFileMap(nlohmann::json &item);
	bool _AddFileToZip(std::string filePath, std::string zipPath,
			   miniz_cpp::zip_file &ecFile);
	bool _AddDirContentsToZip(std::string dirPath, std::string zipDir,
				  miniz_cpp::zip_file &ecFile);
	bool _AddBrowserSourceContentsToZip(std::string dirPath, std::string zipDir,
		miniz_cpp::zip_file& ecFile);
	bool _createSceneCollection(std::string collectionName);
	void _reset();
	void _backupCurrentCollection();

	std::string _currentCollectionPath();
};

void addSources(std::string sourceName, std::set<std::string>& requiredSources, std::set<std::string>& requiredGroups, std::map<std::string, nlohmann::json>& sourceNames, std::map<std::string, nlohmann::json>& groupNames);

enum class SdaIconVerticalAlign {
	Top,
	Middle,
	Bottom
};

/// Represents a parsed state inside the SDA file.
struct SdaState {
	QString title;
	SdaIconVerticalAlign titleAlign;
	QString relativeImagePath;
	QByteArray imageBytes;
	bool hasImage;
	bool hasTitle;
	QString path;
};

class SdaFile
{
public:
	explicit SdaFile(const QString& path);

	bool isValid() const { return valid_; }

	QString originalPath() const { return originalPath_; }

	std::optional<SdaState> firstState() const;
	SdFileVersion fileVersion() const { return version_; }

private:
	void parse_();

	QString originalPath_;
	bool valid_{ false };
	std::optional<SdaState> state_;
	SdFileVersion version_;
};

struct SdProfileState {
	QString name;
	QString model;
	QString path;
};

class SdProfileFile
{
public:
	explicit SdProfileFile(const QString &path);
	bool isValid() const { return valid_; }
	QString originalPath() const { return originalPath_; }
	SdProfileState state() const { return state_; }
	SdFileVersion fileVersion() const { return version_; }

private:
	void parse_();

	QString originalPath_;
	bool valid_{false};

	std::string errorMsg_;
	SdProfileState state_;
	SdFileVersion version_;
};



#endif // SCENEBUNDLE_H
