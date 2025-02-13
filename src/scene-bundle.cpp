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

#include "scene-bundle.hpp"
#include "elgato-cloud-window.hpp"
#include <obs-module.h>
#include "obs-frontend-api.h"
#include <util/platform.h>
#include <util/config-file.h>
#include <QDialog>
#include <QApplication>
#include <QThread>
#include <QMetaObject>
#include <vector>
#include <string>
#include <stdio.h>
#include <algorithm>
#include <zip_file.hpp>

#include <plugin-support.h>
#include "platform.h"
#include "util.h"
#include "obs-utils.hpp"
#include "setup-wizard.hpp"

const std::map<std::string, std::string> extensionMap{
	{".jpg", "/images/"},     {".jpeg", "/images/"},
	{".gif", "/images/"},     {".png", "/images/"},
	{".bmp", "/images/"},     {".webm", "/video/"},
	{".mov", "/video/"},      {".mp4", "/video/"},
	{".mkv", "/video/"},      {".mp3", "/audio/"},
	{".wav", "/aduio/"},      {".effect", "/shaders/"},
	{".shader", "/shaders/"}, {".hlsl", "/shaders/"},
	{".lua", "/scripts/"},    {".py", "/scripts/"}};

// Filter IDs of incompatible filter types, e.g. filters
// that require external libraries or executables.
const std::vector<std::string> incompatibleFilters{"vst_filter"};

SceneBundle::SceneBundle() : _interrupt(false)
{
	obs_log(LOG_INFO, "SceneBundle Constructor Called");
}

SceneBundle::~SceneBundle()
{
	obs_log(LOG_INFO, "SceneBundle Destructor Called");
}

bool SceneBundle::FromCollection(std::string collection_name)
{
	_reset();
	// Get the path to the currently active scene collection file.
	std::string scene_collections_path = get_scene_collections_path();
	std::string file_name = get_current_scene_collection_filename();
	std::string collection_file_path = scene_collections_path + file_name;
	blog(LOG_INFO, "COLLECTION FILE PATH: %s",
	     collection_file_path.c_str());

	// Load the current collection file into a json object
	char *collection_str =
		os_quick_read_utf8_file(collection_file_path.c_str());
	try {
		_collection = nlohmann::json::parse(collection_str);
	} catch (const nlohmann::json::parse_error &e) {
		obs_log(LOG_ERROR, "Parsing Error.\n  message: %s\n  id: %i",
			e.what(), e.id);
		return false;
	}

	bfree(collection_str);
	blog(LOG_INFO, "scripts...");
	for (auto &script : _collection["modules"]["scripts-tool"]) {
		_ProcessJsonObj(script);
	}
	blog(LOG_INFO, "sources...");
	for (auto &source : _collection["sources"]) {
		_ProcessJsonObj(source);
	}
	for (auto &transition : _collection["transitions"]) {
		_ProcessJsonObj(transition);
	}

	return true;
}

bool SceneBundle::FromElgatoCloudFile(std::string filePath,
				      std::string packPath)
{
	_reset();
	//Handle the ZIP archive
	_packPath = packPath;
	miniz_cpp::zip_file file(filePath);

	std::string this_pack_dir = packPath;
	os_mkdirs(this_pack_dir.c_str());
	clear_dir(this_pack_dir);

	// TODO: Probe for a valid manifest.json and collection.json before extraction
	file.extractall(packPath);
	return true;
}

std::string SceneBundle::ExtractBundleInfo(std::string filePath)
{
	miniz_cpp::zip_file file(filePath);
	return file.read("bundle_info.json");
}

void SceneBundle::SceneCollectionCreated(enum obs_frontend_event event,
					 void *obj)
{
	if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED) {
		auto inst = static_cast<SceneBundle *>(obj);
		if (inst) {
			inst->_waiting = false;
		}
	}
}

void SceneBundle::SceneCollectionChanged(enum obs_frontend_event event,
					 void *obj)
{
	if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED) {
		auto inst = static_cast<SceneBundle *>(obj);
		if (inst) {
			inst->_waiting = false;
		}
	}
}

void SceneBundle::ToCollection(std::string collection_name,
			       std::map<std::string, std::string> videoSettings,
			       std::string audioSettings, QDialog *dialog)
{
	dialog->close();
	elgatocloud::CloseElgatoCloudWindow();

	const auto userConf = GetUserConfig();

	std::string curCollectionName =
		config_get_string(userConf, "Basic", "SceneCollection");
	std::string curCollectionFileName =
		get_current_scene_collection_filename();
	curCollectionFileName =
		get_scene_collections_path() + curCollectionFileName;

	char* ccpath = os_get_abs_path_ptr(curCollectionFileName.c_str());
	std::string curCollectionPath = std::string(ccpath);

	size_t pos = 0;
	std::string from = "\\";
	std::string to = "/";
	while ((pos = curCollectionPath.find(from, pos)) != std::string::npos) {
		curCollectionPath.replace(pos, from.length(), to);
		pos += to.length();
	}

	bfree(ccpath);

	std::string savePath = QDir::homePath().toStdString();
	savePath += "/AppData/Local/Elgato/DeepLinking/SCBackups/";
	os_mkdirs(savePath.c_str());

	std::string backupFilename = get_current_scene_collection_filename();
	savePath += backupFilename;

	if (os_file_exists(savePath.c_str())) {
		os_unlink(savePath.c_str());
	}
	os_copyfile(curCollectionPath.c_str(), savePath.c_str());

	char *current_collection = obs_frontend_get_current_scene_collection();
	std::string collection_file_path = _packPath + "/collection.json";
	char *collection_str =
		os_quick_read_utf8_file(collection_file_path.c_str());
	std::string collectionData = collection_str;
	bfree(collection_str);

	std::string needle = "{FILE}:";
	std::string word = _packPath + "/";
	replace_all(collectionData, needle, word);

	for (auto const &[sourceName, settings] : videoSettings) {
		needle = "\"{" + sourceName + "}\"";
		replace_all(collectionData, needle, settings);
	}

	needle = "\"{AUDIO_CAPTURE_SETTINGS}\"";
	replace_all(collectionData, needle, audioSettings);

	_collection = nlohmann::json::parse(collectionData);
	_collection["name"] = collection_name;

	// TODO: Replacements for OS-specific interfaces, or make built-in OBS
	//       importers work. Built-in OBS importers would require json11

	// 1. Add callback listener for scene collection list changed event
	obs_frontend_add_event_callback(SceneBundle::SceneCollectionCreated,
					this);

	// 2. Set waiting to true, to block execution until scene collection created.
	_waiting = true;

	// 3. Create a new, blank scene collection with the proper name.
	obs_frontend_add_scene_collection(collection_name.c_str());
	// 4. Wait until _wating is set to false
	while (_waiting)
		; // do nothing

	// 5. Remove the callback
	obs_frontend_remove_event_callback(SceneBundle::SceneCollectionCreated,
					   this);

	// 6. Get new collection name and filename
	std::string newCollectionName =
		config_get_string(userConf, "Basic", "SceneCollection");
	std::string newCollectionFileName =
		get_current_scene_collection_filename();
	newCollectionFileName =
		get_scene_collections_path() + newCollectionFileName;

	//config_set_string(userConf, "Basic", "SceneCollection", "elgatompplugintemp");
	//config_set_string(userConf, "Basic", "SceneCollectionFile", "elgatompplugintemp");

	// 7. Set waiting to true to block execution until we switch back to the old scene collection
	_waiting = true;

	// 8. Add a callback for scene collection change
	obs_frontend_add_event_callback(SceneBundle::SceneCollectionChanged,
					this);

	// 9. Switch back to old scene collection so we can manually write the new
	//    collection json file
	obs_frontend_set_current_scene_collection(current_collection);

	// 10. Wait until _wating is set to false
	while (_waiting)
		; // do nothing

	// 11. Remove the callback
	obs_frontend_remove_event_callback(SceneBundle::SceneCollectionChanged,
					   this);

	// 12. Replace newCollectionFileName with imported json data
	obs_data_t *data =
		obs_data_create_from_json(_collection.dump().c_str());
	bool success = obs_data_save_json_safe(
		data, newCollectionFileName.c_str(), "tmp", "bak");
	obs_log(LOG_INFO, "Saved new full collection at: %s",
		newCollectionFileName.c_str());
	obs_data_release(data);

	// 13. Load in the new scene collection with the new data.
	obs_frontend_set_current_scene_collection(newCollectionName.c_str());

	if (!success) {
		obs_log(LOG_ERROR, "Unable to create scene collection.");
	}

	bfree(current_collection);
}

SceneBundleStatus SceneBundle::ToElgatoCloudFile(
	std::string file_path, std::vector<std::string> plugins,
	std::map<std::string, std::string> videoDeviceDescriptions)
{
	_interrupt = false;
	miniz_cpp::zip_file ecFile;

	// TODO: Let the bundle author specify the canvas dimensions,
	//       version, plugins required, etc..
	struct obs_video_info ovi = {};
	obs_get_video_info(&ovi);

	nlohmann::json bundleInfo;
	bundleInfo["canvas"]["width"] = ovi.base_width;
	bundleInfo["canvas"]["height"] = ovi.base_height;
	bundleInfo["version"] = "1.0";
	bundleInfo["ec_version"] = "1.0";
	bundleInfo["id"] = gen_uuid();
	bundleInfo["plugins_required"] = plugins;
	bundleInfo["video_devices"] = videoDeviceDescriptions;

	// Write the scene collection json file to zip archive.
	std::string collection_json = _collection.dump(2);
	std::string bundleInfo_json = bundleInfo.dump(2);

	ecFile.writestr("collection.json", collection_json);
	ecFile.writestr("bundle_info.json", bundleInfo_json);
	blog(LOG_INFO, "Adding files to zip...");
	// Write all assets to zip archive.
	for (const auto &file : _fileMap) {
		std::string oFilename = file.first;
		struct stat st;
		os_stat(oFilename.c_str(), &st);
		if ((st.st_mode & S_IFMT) == S_IFDIR) {
			obs_log(LOG_INFO, "Found a directory: %s",
				oFilename.c_str());
			if (!_AddDirContentsToZip(file.first, file.second,
						  ecFile)) {
				bool wasInterrupted = _interrupt;
				_interrupt = false;
				if (wasInterrupted) {
					return _interruptReason;
				}
				return SceneBundleStatus::Error;
			}
		} else if (!_AddFileToZip(file.first, file.second, ecFile)) {
			bool wasInterrupted = _interrupt;
			_interrupt = false;
			if (wasInterrupted) {
				return _interruptReason;
			}
			return SceneBundleStatus::Error;
		}
	}

	ecFile.save(file_path);

	return SceneBundleStatus::Success;
}

std::vector<std::string> SceneBundle::FileList()
{
	std::vector<std::string> files;
	for (auto const &[key, val] : _fileMap) {
		files.push_back(key);
	}
	return files;
}

std::map<std::string, std::string> SceneBundle::VideoCaptureDevices()
{
	return _videoCaptureDevices;
}

bool SceneBundle::FileCheckDialog()
{
	// TODO: Convert this dialog to a QDialog instead of QMessageBox.
	//       Use a table to show all imported files
	//       And show a warning about any incompatible plugins/attached files
	//       that were found. (_skippedFilters data).

	std::string prompt =
		std::to_string(_fileMap.size()) +
		" media files were found to bundle. Does this look correct? (Click 'Show Details' to see file list)";
	std::string fileList = "";
	for (auto const &[key, val] : _fileMap) {
		fileList += key + "\n";
	}
	QWidget *window = (QWidget *)obs_frontend_get_main_window();

	QMessageBox alertBox(window);
	alertBox.setText("Packaging scene collection...");
	alertBox.setInformativeText(prompt.c_str());
	alertBox.setStandardButtons(QMessageBox::StandardButton::Yes |
				    QMessageBox::StandardButton::No);
	alertBox.setDefaultButton(QMessageBox::StandardButton::Yes);
	alertBox.setDetailedText(fileList.c_str());
	alertBox.setStyleSheet("QLabel{min-width: 700px;}");
	auto result = alertBox.exec();
	return result == QMessageBox::StandardButton::Yes;
}

void SceneBundle::_ProcessJsonObj(nlohmann::json &obj)
{
	std::string settingsRepalce = "";
	std::string idKey = "id";
	std::string nameKey = "name";
	if (obj.contains(std::string{idKey})) {
		std::string name = obj[nameKey];
		if (obj[idKey] == "dshow_input") {
			obj.erase("settings");
			obj["settings"] = "{" + name + "}";
			if (!obj.contains("uuid")) {
				char *uuid = os_generate_uuid();
				obj["uuid"] = std::string(uuid);
				bfree(uuid);
			}
			_videoCaptureDevices[obj["uuid"]] = obj["name"];
		} else if (obj[idKey] == "wasapi_input_capture") {
			obj.erase("settings");
			obj["settings"] = "{AUDIO_CAPTURE_SETTINGS}";
		} // TODO: add for Mac/Linux
	}

	for (auto &[key, item] : obj.items()) {
		if (item.is_string()) {
			std::string value = item.template get<std::string>();
			if (os_file_exists(value.c_str())) {
				_CreateFileMap(item);
				std::string item_value =
					item.template get<std::string>();
				obs_log(LOG_INFO, "New Filename: %s",
					item_value.c_str());
			}
		} else if (item.is_object()) {
			_ProcessJsonObj(item);
		} else if (item.is_array()) {
			// TODO: Add code to handle Adv SS macros that require external programs
			//       e.g. the Run filter and action in AdvSS.
			if (key == "filters") {
				// Determine if a filter is incompatible, and remove from filter list
				std::vector<size_t> to_remove;
				int idx = 0;
				for (auto &filter : item) {
					try {
						std::string filter_id =
							filter.at("id")
								.template get<
									std::string>();
						if (std::find(
							    incompatibleFilters
								    .begin(),
							    incompatibleFilters
								    .end(),
							    filter_id) !=
						    incompatibleFilters.end()) {
							// insert at beginning of vector, so that ids will
							// end up in descending order.
							to_remove.insert(
								to_remove.begin(),
								idx);
							// Get the current source and filter name
							std::string filter_name =
								filter.at("name")
									.template get<
										std::string>();
							std::string source_name =
								obj.at("name")
									.template get<
										std::string>();
							_skippedFilters.push_back(
								std::pair<
									std::string,
									std::string>(
									source_name,
									filter_name));
							obs_log(LOG_INFO,
								"FOUND INCOMPATIBLE FILTER: %s on source %s",
								filter_name
									.c_str(),
								source_name
									.c_str());
						}
					} catch (const nlohmann::json::
							 out_of_range) {
						// filter doesn't have an id.
					}
					idx++;
				}
				for (auto &id : to_remove) {
					item.erase(id);
					obs_log(LOG_INFO,
						"Removed incompatible filter.");
				}
			}
			_ProcessJsonObj(item);
		}
	}
}

void SceneBundle::_CreateFileMap(nlohmann::json &item)
{
	std::string value = item.template get<std::string>();

	size_t pos = 0;
	std::string from = "\\";
	std::string to = "/";
	while ((pos = value.find(from, pos)) != std::string::npos) {
		value.replace(pos, from.length(), to);
		pos += to.length();
	}

	struct stat st;
	if (os_stat(value.c_str(), &st) == 0 && st.st_mode & S_IEXEC) {
		obs_log(LOG_INFO, "_CreativeFileMap: IS EXEC");
	}
	if (_fileMap.find(value) == _fileMap.end()) {
		// file is new
		std::string filename = value.substr(value.rfind("/") + 1);
		bool hasExtension = filename.rfind(".") != std::string::npos;
		std::string extension =
			hasExtension ? os_get_path_extension(value.c_str())
				     : "";
		std::string base =
			extension != ""
				? filename.substr(0, filename.rfind(extension))
				: filename;
		std::string directory = "/misc/";
		if (extension != "" &&
		    extensionMap.find(extension) != extensionMap.end()) {
			directory = extensionMap.at(extension);
		} else {
			directory = "/misc/";
		}
		std::string newFileName = "Assets" + directory + filename;
		auto result =
			std::find_if(std::begin(_fileMap), std::end(_fileMap),
				     [newFileName](const auto &fmv) {
					     return fmv.second == newFileName;
				     });
		int i = 1;
		while (result != std::end(_fileMap)) {
			newFileName = "Assets" + directory + base + "_" +
				      std::to_string(i) + extension;
			result = std::find_if(std::begin(_fileMap),
					      std::end(_fileMap),
					      [newFileName](const auto &fmv) {
						      return fmv.second ==
							     newFileName;
					      });
			i++;
		}
		_fileMap[value] = newFileName;
	}
	item = "{FILE}:" + _fileMap.at(value);
}

bool SceneBundle::_AddFileToZip(std::string filePath, std::string zipPath,
				miniz_cpp::zip_file &ecFile)
{
	if (_interrupt) {
		return false;
	}
	blog(LOG_INFO, "Adding File %s as %s", filePath.c_str(),
	     zipPath.c_str());
	ecFile.write(filePath, zipPath);
	return true;
}

bool SceneBundle::_AddDirContentsToZip(std::string dirPath, std::string zipDir,
				       miniz_cpp::zip_file &ecFile)
{
	// Iterate the files in the directory and add them to the zip.
	// Ignore sub-directories
	os_dir_t *dir = os_opendir(dirPath.c_str());
	if (dir) {
		struct os_dirent *ent;
		for (;;) {
			ent = os_readdir(dir);
			if (!ent)
				break;
			if (ent->directory)
				continue;
			std::string filename = ent->d_name;
			std::string filePath = dirPath + "/" + filename;
			std::string zipFilePath = zipDir + "/" + filename;
			if (!_AddFileToZip(filePath, zipFilePath, ecFile)) {
				os_closedir(dir);
				return false;
			}
		}
	} else {
		obs_log(LOG_ERROR, "Fatal: Could not open directory: %s",
			dirPath.c_str());
		return false;
	}

	os_closedir(dir);

	return true;
}

void SceneBundle::_reset()
{
	_fileMap.clear();
	_skippedFilters.clear();
	_videoCaptureDevices.clear();
}
