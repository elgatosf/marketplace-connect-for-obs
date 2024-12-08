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

#include "plugin-support.h"
#include "platform.h"
#include "util.h"
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
	std::string file_name =
		config_get_string(obs_frontend_get_global_config(), "Basic",
				  "SceneCollectionFile");
	std::string collection_file_path =
		scene_collections_path + file_name + ".json";

	// Save the current scene collection to ensure our output is the latest
	obs_frontend_save();

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

	for (auto &script : _collection["modules"]["scripts-tool"]) {
		_ProcessJsonObj(script);
	}

	for (auto &source : _collection["sources"]) {
		_ProcessJsonObj(source);
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

void SceneBundle::ToCollection(std::string collection_name,
			       std::map<std::string, std::string> videoSettings,
			       std::string audioSettings, QDialog *dialog)
{
	dialog->close();
	elgatocloud::CloseElgatoCloudWindow();

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

	// Create a new collection so that it is added to the collections menu
	// and grab the expected file name of the new collection.
	obs_frontend_add_scene_collection(collection_name.c_str());
	std::string file_name =
		config_get_string(obs_frontend_get_global_config(), "Basic",
				  "SceneCollectionFile");
	obs_log(LOG_INFO, "Created new blank collection at: %s",
		file_name.c_str());
	// Switch back to old scene collection, so that we can write new
	// .json file for new scene collection without it being overwritten
	// with blank scene collection.
	obs_frontend_set_current_scene_collection(current_collection);

	// TODO: Make sure file name is safe
	std::string scene_collections_path = get_scene_collections_path();
	std::string collection_json_file_path =
		scene_collections_path + file_name + ".json";

	// TODO: Add calls to GetUnusedName and GetUnusedSceneCollectionFile *or* a dialog
	//       to warn of overwriting
	std::string collection_out = _collection.dump();

	// Convert to a native OBS Data object.
	obs_data_t *data = obs_data_create_from_json(collection_out.c_str());
	// So that we can use OBS's save_safe function
	bool success = obs_data_save_json_safe(
		data, collection_json_file_path.c_str(), "tmp", "bak");
	obs_log(LOG_INFO, "Saved new full collection at: %s",
		collection_json_file_path.c_str());
	obs_data_release(data);
	// Finally, switch back to the new scene collection.
	obs_frontend_set_current_scene_collection(collection_name.c_str());

	if (!success) {
		obs_log(LOG_ERROR, "Unable to create scene collection.");
	}
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
				char* uuid = os_generate_uuid();
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
	obs_log(LOG_INFO, "_CreateFileMap: %s", value.c_str());
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
			newFileName = directory + base + "_" +
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
	blog(LOG_INFO, "Adding File %s", filePath.c_str());
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
