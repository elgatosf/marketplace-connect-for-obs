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
#include "elgato-stream-deck-widgets.hpp"
#include <obs-module.h>
#include "obs-frontend-api.h"
#include <util/platform.h>
#include <util/config-file.h>
#include <QDialog>
#include <QApplication>
#include <QMessageBox>
#include <QThread>
#include <QMetaObject>
#include <vector>
#include <set>
#include <string>
#include <filesystem>
#include <stdio.h>
#include <algorithm>
#include "zip-archive.hpp"
//#include <zip_file.hpp>

#include <plugin-support.h>
#include "platform.h"
#include "util.h"
#include "obs-utils.hpp"
#include "setup-wizard.hpp"
#include "api.hpp"
#include "export-wizard.hpp"

const std::map<std::string, std::string> extensionMap{
	{".jpg", "/images/"},     {".jpeg", "/images/"},
	{".gif", "/images/"},     {".png", "/images/"},
	{".bmp", "/images/"},     {".webm", "/video/"},
	{".mov", "/video/"},      {".mp4", "/video/"},
	{".mkv", "/video/"},      {".mp3", "/audio/"},
	{".wav", "/audio/"},      {".effect", "/shaders/"},
	{".shader", "/shaders/"}, {".hlsl", "/shaders/"},
	{".lua", "/scripts/"},    {".py", "/scripts/"},
	{".html", "/browser-sources/"},
	{".htm", "/browser-sources/"}
};

// Filter IDs of incompatible filter types, e.g. filters
// that require external libraries or executables.
const std::vector<std::string> incompatibleFilters{"vst_filter"};

SceneBundle::SceneBundle(QObject *parent)
	: QObject(parent), _interrupt(false)
{

}

SceneBundle::~SceneBundle()
{
}

bool SceneBundle::FromCollection(std::string collection_name)
{
	_reset();
	// Get the path to the currently active scene collection file.
	std::string scene_collections_path = get_scene_collections_path();
	std::string file_name = get_current_scene_collection_filename();
	std::string collection_file_path = scene_collections_path + file_name;

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
	if (_collection.contains("groups")) {
		for (auto& group : _collection["groups"]) {
			_ProcessJsonObj(group);
		}
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
	ZipArchive file;
	file.openExisting(filePath.c_str());

	std::string this_pack_dir = packPath;
	os_mkdirs(this_pack_dir.c_str());
	clear_dir(this_pack_dir);

	if(!file.contains("bundle_info.json") || !file.contains("collection.json"))
		return false;
	
	file.extractAllToFolder(packPath.c_str());
	return true;
}

SceneCollectionInfo SceneBundle::ExtractBundleInfo(std::string filePath)
{
	SceneCollectionInfo result;
	ZipArchive file;
	file.openExisting(filePath.c_str());

	auto bundleInfo = file.extractFileToString("bundle_info.json");
	result.bundleInfo = bundleInfo.toStdString();

	bool hasStreamDeck = false;
	for (const auto &entry : file.listEntries()) {
		if (entry.toStdString().rfind("Assets/stream-deck/", 0) ==
		    0) { // starts with
			hasStreamDeck = true;
			break;
		}
	}

	if (!hasStreamDeck) {
		return result;
	}

	QTemporaryDir tempDir;
	if (!tempDir.isValid()) {
		qWarning() << "Failed to create temporary directory";
		return result;
	}

	QString basePath = tempDir.path();
	// Keep the directory alive (release ownership so it persists)
	tempDir.setAutoRemove(false);

	// Extract all entries under Assets/stream-deck
	for (const auto &entry : file.listEntries()) {
		if (entry.toStdString().rfind("Assets/stream-deck/", 0) ==
		    0) {
			if (entry.back() == '/') {
				// It's a directory, ensure it exists
				QDir().mkpath(basePath + "/" +entry);
			} else {
				// It's a file
				QString outPath = basePath + "/" + entry;
				QFileInfo fi(outPath);
				QDir().mkpath(
					fi.path()); // Ensure directories exist

				QFile outFile(outPath);
				if (outFile.open(QIODevice::WriteOnly)) {
					auto bytes =
						file.extractFileToMemory(entry);
					outFile.write(bytes.data(),
								static_cast<qint64>(
									bytes.size()));
					outFile.close();
				} else {
					qWarning() << "Failed to write file:" << outPath;
				}
			}
		}
	}
	result.streamDeckPath = basePath.toStdString();

	return result;
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
bool SceneBundle::MergeCollection(std::string collection_name,
	std::vector<std::string> scenes,
	std::map<std::string, std::string> videoSettings,
	std::string audioSettings, std::string id)
{
	const auto userConf = GetUserConfig();
	_backupCurrentCollection();
	auto curCollectionPath = _currentCollectionPath();
	char* collection_str = os_quick_read_utf8_file(curCollectionPath.c_str());
	nlohmann::json currentCollectionJson = nlohmann::json::parse(collection_str);
	bfree(collection_str);
	
	std::vector<std::string> cColSourceNames;
	std::vector<std::string> cColUuids;
	std::vector<std::string> cColGroupNames;
	std::map<std::string, nlohmann::json> cVideoCaptureSources;

	// Grab all source names and UUIDs in the existing collection so that
	// we can make sure there are no name/id clashes.  UUIDs might clash
	// if the same scene collection is merged twice.
	if (currentCollectionJson.contains("sources")) {
		for (auto source : currentCollectionJson["sources"]) {
			cColSourceNames.push_back(source["name"]);
			cColUuids.push_back(source["uuid"]);
			if (source.contains("filters")) {
				for (auto filter : source["filters"]) {
					cColUuids.push_back(filter["uuid"]);
				}
			}

			// TODO: Mac Compatibility
			if (source["id"] == "dshow_input") {
				std::string vdi = source["settings"]["video_device_id"];
				cVideoCaptureSources[vdi] = source;
			}
		}
	}

	if (currentCollectionJson.contains("groups")) {
		for (auto group : currentCollectionJson["groups"]) {
			cColSourceNames.push_back(group["name"]);
			cColUuids.push_back(group["uuid"]);
			if (group.contains("filters")) {
				for (auto filter : group["filters"]) {
					cColUuids.push_back(filter["uuid"]);
				}
			}
		}
	}

	if (currentCollectionJson.contains("transitions")) {
		for (auto transition : currentCollectionJson["transitions"]) {
			cColSourceNames.push_back(transition["name"]);
		}
	}

	std::string collection_file_path = _packPath + "/collection.json";
	char* ecollection_str =
		os_quick_read_utf8_file(collection_file_path.c_str());
	std::string collectionData = ecollection_str;
	bfree(ecollection_str);

	std::string bundle_info_path = _packPath + "/bundle_info.json";
	char* bundle_info_str = os_quick_read_utf8_file(bundle_info_path.c_str());
	std::string bundleInfoData = bundle_info_str;
	bfree(bundle_info_str);

	// replace all uuid clashses with a new uuid
	for (auto uuid : cColUuids) {
		auto newUuid = gen_uuid();
		replace_all(collectionData, uuid, newUuid);
	}

	// Replace all name clashes in the new scene collection (names that
	// exist in the current collection)
	for (auto name : cColSourceNames) {
		int i = 2;
		std::string newName = name + "_" + std::to_string(i);
		while (
			std::find(cColSourceNames.begin(), cColSourceNames.end(), newName) != cColSourceNames.end()
			|| collectionData.find(std::string("\"" + newName + "\"")) != std::string::npos
			) {
			i += 1;
			newName = name + "_" + std::to_string(i);
		}
		for (auto& sceneName : scenes) {
			if (sceneName == name) {
				sceneName = newName;
			}
		}
		replace_all(collectionData, "\"" + name + "\"", "\"" + newName + "\"");
	}

	std::string needle = "{FILE}:";
	std::string word = _packPath + "/";
	replace_all(collectionData, needle, word);

	for (auto const& [sourceName, settings] : videoSettings) {
		needle = "\"{" + sourceName + "}\"";
		replace_all(collectionData, needle, settings);
	}

	needle = "\"{AUDIO_CAPTURE_SETTINGS}\"";
	replace_all(collectionData, needle, audioSettings);

	_collection = nlohmann::json::parse(collectionData);

	if (_collection.contains("modules") &&
		_collection["modules"].contains("elgato_marketplace_connect") &&
		_collection["modules"]["elgato_marketplace_connect"].contains("id")
		) { // This was a downloaded collection that has been exported
		auto api = elgatocloud::MarketplaceApi::getInstance();
		std::string currentId = api->id();
		std::string embeddedId = _collection["modules"]["elgato_marketplace_connect"]["id"];
		if (currentId != embeddedId) {
			obs_log(LOG_INFO, "Ids don't match");
			QMessageBox msgBox;
			msgBox.setWindowTitle("Alert");
			msgBox.setText("This scene collection file contains portions of a scene collection purchased on the Elgato Marketplace. To install it, you will need to be logged in to the original account that purchased the collection. Please log in through the Elgato Marketplace menu item, and try to install again.");
			msgBox.setIcon(QMessageBox::Information);
			msgBox.setStandardButtons(QMessageBox::Close);
			msgBox.exec();
			return false;
		}
		id = embeddedId;
	}

	// Code to swap out video capture devices in _collection,
	// with source clones if needed.

	for (auto& source : _collection["sources"]) {
		if (source["id"] == "dshow_input") {
			// If the user did not select a video device for this
			// source.. continue
			if (!source["settings"].contains("video_device_id")) {
				continue;
			}
			std::string vdi = source["settings"]["video_device_id"];
			if (cVideoCaptureSources.find(vdi) != cVideoCaptureSources.end()) {
				source["id"] = "source-clone";
				source["versioned_id"] = "source-clone";
				source["settings"] = {
					{"clone", cVideoCaptureSources[vdi]["name"]},
					{"audio", false},
					{"active_clone", false}
				};
			}
		}
	}

	std::vector<nlohmann::json> mergeSources = {};
	std::vector<nlohmann::json> mergeGroups = {};
	std::vector<nlohmann::json> mergeSceneOrder = {};
	if (scenes.size() == 0) {
		for (auto& source : _collection["sources"]) {
			mergeSources.push_back(source);
		}
		for (auto& group : _collection["groups"]) {
			mergeGroups.push_back(group);
		}
		for (auto& so : _collection["scene_order"]) {
			mergeSceneOrder.push_back(so);
		}
	} else {
		// 1. Get all source names in collection
		std::map<std::string, nlohmann::json> sourceNames;
		for (auto& source : _collection["sources"]) {
			sourceNames[source["name"]] = source;
		}

		std::map<std::string, nlohmann::json> groupNames;
		for (auto& group : _collection["groups"]) {
			groupNames[group["name"]] = group;
		}

		// 2. Determine names of all required sources
		std::set<std::string> requiredSources;
		std::set<std::string> requiredGroups;

		for (auto& sceneName : scenes) {
			addSources(
				sceneName,
				requiredSources,
				requiredGroups,
				sourceNames,
				groupNames
			);
		}
		
		// 3. Get scene order
		std::vector<nlohmann::json> colSceneOrder = _collection["scene_order"];
		std::copy_if(colSceneOrder.begin(), colSceneOrder.end(), std::back_inserter(mergeSceneOrder),
			[requiredSources](nlohmann::json const& scene) {
				return requiredSources.find(scene["name"]) != requiredSources.end();
			});

		// 4. Collect required sources
		for (auto sourceName : requiredSources) {
			mergeSources.push_back(sourceNames[sourceName]);
		}

		for (auto groupName : requiredGroups) {
			mergeGroups.push_back(groupNames[groupName]);
		}
	}

	for (auto& source : mergeSources) {
		currentCollectionJson["sources"].push_back(source);
	}

	for (auto& group : mergeGroups) {
		currentCollectionJson["groups"].push_back(group);
	}

	for (auto& transition : _collection["transitions"]) {
		currentCollectionJson["transitions"].push_back(transition);
	}

	currentCollectionJson["current_transition"] = _collection["current_transition"];
	currentCollectionJson["transition_duration"] = _collection["transition_duration"];
	
	for (auto& so : currentCollectionJson["scene_order"]) {
		mergeSceneOrder.push_back(so);
	}

	currentCollectionJson["scene_order"] = mergeSceneOrder;

	_bundleInfo = nlohmann::json::parse(bundleInfoData);

	nlohmann::json module_info = {
		{"first_run", true}
	};

	if (_bundleInfo.contains("third_party")) {
		module_info["third_party"] = _bundleInfo["third_party"];
	}

	if (!id.empty()) {
		module_info["id"] = id;
	}

	_collection = currentCollectionJson;

	_collection["modules"]["elgato_marketplace_connect"] = module_info;
	
	return _createSceneCollection(collection_name);
}

bool SceneBundle::ToCollection(std::string collection_name,
			       std::map<std::string, std::string> videoSettings,
			       std::string audioSettings, std::string id,
			       std::string productName, std::string productId,
			       std::string productSlug)
{
	const auto userConf = GetUserConfig();
	_backupCurrentCollection();

	std::string collection_file_path = _packPath + "/collection.json";
	std::string bundle_info_path = _packPath + "/bundle_info.json";
	char *collection_str =
		os_quick_read_utf8_file(collection_file_path.c_str());
	char* bundle_info_str = os_quick_read_utf8_file(bundle_info_path.c_str());
	std::string collectionData = collection_str;
	std::string bundleInfoData = bundle_info_str;
	bfree(collection_str);
	bfree(bundle_info_str);

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
	_bundleInfo = nlohmann::json::parse(bundleInfoData);

	if (_collection.contains("modules") &&
		_collection["modules"].contains("elgato_marketplace_connect") &&
		_collection["modules"]["elgato_marketplace_connect"].contains("id")
	) { // This was a downloaded collection that has been exported
		auto api = elgatocloud::MarketplaceApi::getInstance();
		std::string currentId = api->id();
		std::string embeddedId = _collection["modules"]["elgato_marketplace_connect"]["id"];
		if (currentId != embeddedId) {
			obs_log(LOG_INFO, "Ids don't match");
			QMessageBox msgBox;
			msgBox.setWindowTitle("Alert");
			msgBox.setText("This scene collection file contains portions of a scene collection purchased on the Elgato Marketplace. To install it, you will need to be logged in to the original account that purchased the collection. Please log in through the Elgato Marketplace menu item, and try to install again.");
			msgBox.setIcon(QMessageBox::Information);
			msgBox.setStandardButtons(QMessageBox::Close);
			msgBox.exec();
			return false;
		}
		id = embeddedId;
	}

	nlohmann::json module_info = {
		{"first_run", true}
	};
	
	if (_bundleInfo.contains("third_party")) {
		module_info["third_party"] = _bundleInfo["third_party"];
	}

	if (_bundleInfo.contains("version")) {
		module_info["version"] = _bundleInfo["version"];
	} else {
		module_info["version"] = "1.0";
	}

	if (_bundleInfo.contains("exported_with_plugin_version")) {
		module_info["exported_with_plugin_version"] = _bundleInfo["exported_with_plugin_version"];
	} else {
		module_info["exported_with_plugin_version"] = "1.0.0.0";
	}

	if (_bundleInfo.contains("stream_deck_actions")) {
		module_info["stream_deck_actions"] =
			_bundleInfo["stream_deck_actions"];
	} else {
		module_info["stream_deck_actions"] = nlohmann::json::array();
	}

	if (_bundleInfo.contains("stream_deck_profiles")) {
		module_info["stream_deck_profiles"] =
			_bundleInfo["stream_deck_profiles"];
	} else {
		module_info["stream_deck_profiles"] = nlohmann::json::array();
	}

	if (productId != "") {
		module_info["product_details"] = {
			{"name", productName},
			{"id", productId},
			{"slug", productSlug}
		};
	}

	module_info["pack_path"] = _packPath;

	if (!id.empty()) {
		module_info["id"] = id;
	}

	_collection["modules"]["elgato_marketplace_connect"] = module_info;

	return _createSceneCollection(collection_name);
}

bool SceneBundle::_createSceneCollection(std::string collection_name)
{
	char* current_collection = obs_frontend_get_current_scene_collection();
	const auto userConf = GetUserConfig();
	_collection["name"] = collection_name;

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

// 11. Replace newCollectionFileName with imported json data
obs_data_t* data =
obs_data_create_from_json(_collection.dump().c_str());
bool success = obs_data_save_json_safe(
	data, newCollectionFileName.c_str(), "tmp", "bak");
obs_data_release(data);

bfree(current_collection);

if (!success) {
	obs_log(LOG_ERROR, "Unable to create scene collection.");
	obs_frontend_remove_event_callback(SceneBundle::SceneCollectionChanged,
		this);
	return false;
}

_waiting = true;

// 12. Load in the new scene collection with the new data.
obs_frontend_set_current_scene_collection(newCollectionName.c_str());

while (_waiting)
;

// 13. Remove the callback
obs_frontend_remove_event_callback(SceneBundle::SceneCollectionChanged,
	this);

return true;
}

void SceneBundle::_backupCurrentCollection()
{
	auto curCollectionPath = _currentCollectionPath();

	std::string savePath = QDir::homePath().toStdString();
	savePath += "/AppData/Local/Elgato/MarketplaceConnect/SCBackups/";
	os_mkdirs(savePath.c_str());

	std::string backupFilename = get_current_scene_collection_filename();
	savePath += backupFilename;

	if (os_file_exists(savePath.c_str())) {
		os_unlink(savePath.c_str());
	}
	os_copyfile(curCollectionPath.c_str(), savePath.c_str());
}

std::string SceneBundle::_currentCollectionPath()
{
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
	return curCollectionPath;
}

SceneBundleStatus SceneBundle::ToElgatoCloudFile(
	std::string file_path, std::vector<std::string> plugins,
	std::vector<std::pair<std::string, std::string>> thirdParty,
	std::vector<SceneInfo> outputScenes,
	std::map<std::string, std::string> videoDeviceDescriptions,
	std::vector<SdaFileInfo> sdaFiles,
	std::vector<SdaFileInfo> sdProfileFiles, std::string version,
	void* data)
{
	_interrupt = false;
	ZipArchive ecFile;
	bool canceled = false;
	auto wizard = static_cast<elgatocloud::StreamPackageExportWizard*>(data);

	auto cancelCallback = connect(wizard, &elgatocloud::StreamPackageExportWizard::cancelOperation,
		this, [this, &ecFile, &canceled]() {
			ecFile.cancelOperation();
			canceled = true;
		});

	connect(&ecFile, &ZipArchive::overallProgress, this,
		[this, wizard](double progress) {
			QMetaObject::invokeMethod(
				QCoreApplication::instance()->thread(), // main GUI thread
				[this, wizard, progress]() {
					wizard->emitOverallProgress(progress * 100.0);
				}
			);
		});

	connect(&ecFile, &ZipArchive::fileProgress, this,
		[this, wizard](const QString& fileName, double progress) {
			QMetaObject::invokeMethod(
				QCoreApplication::instance()->thread(), // main GUI thread
				[this, wizard, fileName, progress]() {
					wizard->emitFileProgress(fileName, progress * 100.0);
				}
			);
		});

	// TODO: Let the bundle author specify the canvas dimensions,
	//       version, plugins required, etc..
	struct obs_video_info ovi = {};
	obs_get_video_info(&ovi);

	std::vector<std::map<std::string, std::string>> oScenes;
	for (auto const& scene : outputScenes) {
		std::map<std::string, std::string> s = {
			{"id", scene.id },
			{"name", scene.name }
		};
		oScenes.push_back(s);
	}

	std::vector<std::map<std::string, std::string>> thirdPartyReqs;
	for (auto const& req : thirdParty) {
		std::map<std::string, std::string> r = {
			{"name", req.first},
			{"url", req.second}
		};
		thirdPartyReqs.push_back(r);
	}

	std::vector<std::map<std::string, std::string>> zipSdaFiles;
	for (auto const &sda : sdaFiles) {
		auto filename = std::filesystem::path(sda.path.toStdString()).filename();
		std::map<std::string, std::string> details = {
			{"filename", filename.string()},
			{"label", sda.label.toStdString()}};
		zipSdaFiles.push_back(details);
	}

	std::vector<std::map<std::string, std::string>> zipSdProfileFiles;
	for (auto const &sdp : sdProfileFiles) {
		auto filename =
			std::filesystem::path(sdp.path.toStdString()).filename();
		std::map<std::string, std::string> details = {
			{"filename", filename.string()},
			{"label", sdp.label.toStdString()}};
		zipSdProfileFiles.push_back(details);
	}

	nlohmann::json bundleInfo;
	bundleInfo["canvas"]["width"] = ovi.base_width;
	bundleInfo["canvas"]["height"] = ovi.base_height;
	bundleInfo["version"] = version;
	bundleInfo["ec_version"] = "1.0";
	bundleInfo["id"] = gen_uuid();
	bundleInfo["plugins_required"] = plugins;
	bundleInfo["third_party"] = thirdPartyReqs;
	bundleInfo["video_devices"] = videoDeviceDescriptions;
	bundleInfo["output_scenes"] = oScenes;
	bundleInfo["stream_deck_actions"] = zipSdaFiles;
	bundleInfo["stream_deck_profiles"] = zipSdProfileFiles;
	std::string pluginVersion = PLUGIN_VERSION;
	bundleInfo["exported_with_plugin_version"] = pluginVersion;

	// Write the scene collection json file to zip archive.
	std::string collection_json = _collection.dump(2);
	std::string bundleInfo_json = bundleInfo.dump(2);

	std::vector<std::string> browserSourceDirs;

	ecFile.addString("collection.json", collection_json.c_str());
	ecFile.addString("bundle_info.json", bundleInfo_json.c_str());
	// Write all assets to zip archive.
	for (const auto &file : _fileMap) {
		std::string oFilename = file.first;

		std::string filename = oFilename.substr(oFilename.rfind("/") + 1);
		std::string parentDir = oFilename.substr(0, oFilename.rfind("/"));
		std::string zipParent = file.second.substr(0, file.second.rfind("/"));
		bool hasExtension = filename.rfind(".") != std::string::npos;
		std::string extension =
			hasExtension ? os_get_path_extension(oFilename.c_str())
			: "";
		bool isBrowserSource = extension == ".html" || extension == ".htm";
		bool addBrowser = isBrowserSource && std::find(browserSourceDirs.begin(), browserSourceDirs.end(), parentDir) == browserSourceDirs.end();
		if (addBrowser) {
			browserSourceDirs.push_back(parentDir);
		}
		struct stat st;
		os_stat(oFilename.c_str(), &st);
		if ((st.st_mode & S_IFMT) == S_IFDIR) {
			if (!_AddDirContentsToZip(file.first, file.second,
						  ecFile)) {
				bool wasInterrupted = _interrupt;
				_interrupt = false;
				if (wasInterrupted) {
					return _interruptReason;
				}
				return SceneBundleStatus::Error;
			}
		} else if (addBrowser) {
			if (!_AddBrowserSourceContentsToZip(parentDir, zipParent, ecFile)) {
				bool wasInterrupted = _interrupt;
				_interrupt = false;
				if (wasInterrupted) {
					return _interruptReason;
				}
				return SceneBundleStatus::Error;
			}
		} else if(!isBrowserSource) {
			if (!_AddFileToZip(file.first, file.second, ecFile)) {
				bool wasInterrupted = _interrupt;
				_interrupt = false;
				if (wasInterrupted) {
					return _interruptReason;
				}
				return SceneBundleStatus::Error;
			}
		}
	}

	for (auto const &sda : sdaFiles) {
		std::string path = sda.path.toStdString();
		std::string filename =
			std::filesystem::path(sda.path.toStdString())
				.filename()
				.string();
		std::string saveFile =
			"Assets/stream-deck/stream-deck-actions/" + filename;
		if (!_AddFileToZip(path, saveFile, ecFile)) {
			bool wasInterrupted = _interrupt;
			_interrupt = false;
			if (wasInterrupted) {
				return _interruptReason;
			}
			return SceneBundleStatus::Error;
		}
	}

	for (auto const &sdp : sdProfileFiles) {
		std::string path = sdp.path.toStdString();
		std::string filename =
			std::filesystem::path(sdp.path.toStdString())
				.filename()
				.string();
		std::string saveFile =
			"Assets/stream-deck/stream-deck-profiles/" + filename;
		if (!_AddFileToZip(path, saveFile, ecFile)) {
			bool wasInterrupted = _interrupt;
			_interrupt = false;
			if (wasInterrupted) {
				return _interruptReason;
			}
			return SceneBundleStatus::Error;
		}
	}

	ecFile.writeArchive(file_path.c_str());

	disconnect(cancelCallback);

	return canceled ? SceneBundleStatus::Cancelled : SceneBundleStatus::Success;
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
		bool isBrowserSource = extension == ".htm" || extension == ".html";
		if (isBrowserSource) {
			std::filesystem::path pathObj(value);
			auto parentPath = pathObj.parent_path();
			std::string parent = parentPath.filename().string();
			directory += parent + "/";
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
				ZipArchive &ecFile)
{
	if (_interrupt) {
		return false;
	}
	ecFile.addFile(zipPath.c_str(), filePath.c_str());
	return true;
}

bool SceneBundle::_AddBrowserSourceContentsToZip(std::string dirPath, std::string zipDir,
	ZipArchive &ecFile)
{
	// Iterate the files in the directory and add them to the zip.
	// Ignore sub-directories
	os_dir_t* dir = os_opendir(dirPath.c_str());
	if (dir) {
		struct os_dirent* ent;
		for (;;) {
			ent = os_readdir(dir);
			if (!ent)
				break;
			if (ent->directory) {
				std::string dName = ent->d_name;
				if (dName == "." || dName == "..") {
					continue;
				}
				std::string dPath = dirPath + "/" + dName;
				std::string zipDPath = zipDir + "/" + dName;
				if (!_AddBrowserSourceContentsToZip(dPath, zipDPath, ecFile)) {
					os_closedir(dir);
					return false;
				}
			} else {
				std::string filename = ent->d_name;
				std::string filePath = dirPath + "/" + filename;
				std::string zipFilePath = zipDir + "/" + filename;
				if (!_AddFileToZip(filePath, zipFilePath, ecFile)) {
					os_closedir(dir);
					return false;
				}
			}
		}
	}
	else {
		obs_log(LOG_ERROR, "Fatal: Could not open directory: %s",
			dirPath.c_str());
		return false;
	}

	os_closedir(dir);

	return true;
}

bool SceneBundle::_AddDirContentsToZip(std::string dirPath, std::string zipDir,
				       ZipArchive &ecFile)
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


void addSources(
	std::string sourceName,
	std::set<std::string>& requiredSources,
	std::set<std::string>& requiredGroups,
	std::map<std::string, nlohmann::json>& sourceNames,
	std::map<std::string, nlohmann::json>& groupNames
)
{
	bool isSource = sourceNames.find(sourceName) != sourceNames.end();
	bool isGroup = groupNames.find(sourceName) != groupNames.end();

	// Return if the source isn't in the json file.  This should
	// actually throw an error.
	if (!isSource && !isGroup) {
		return;
	}

	// Return if we've already added this source.
	if (isSource && requiredSources.find(sourceName) != requiredSources.end()) {
		return;
	}
	
	if (isGroup && requiredGroups.find(sourceName) != requiredGroups.end()) {
		return;
	}

	if (isSource)
		requiredSources.insert(sourceName);
	else
		requiredGroups.insert(sourceName);

	auto source = isSource ? sourceNames[sourceName].flatten() : groupNames[sourceName].flatten();

	for (auto field : source) {
		if (
			field.is_string() && 
			(sourceNames.find(field) != sourceNames.end() || groupNames.find(field) != groupNames.end())
		) {
			addSources(field, requiredSources, requiredGroups, sourceNames, groupNames);
		}
	}
}

SdaFile::SdaFile(const QString& path)
	: originalPath_(path)
{
	parse_();
}

void SdaFile::parse_()
{
	std::string imageBaseDir = GetDataPath();
	imageBaseDir += "/images/stream-deck-default-icons/";
	try {
		ZipArchive zip;
		zip.openExisting(originalPath_);

		// Check if this is a new or legacy file
		std::string packageJsonPath = "package.json";
		version_ = zip.contains(packageJsonPath.c_str())
				   ? SdFileVersion::Current
				   : SdFileVersion::Legacy;

		// Find the correct manifest.json under Profiles/*
		std::string chosenManifestPath;
		nlohmann::json manifest;

		for (auto& entry : zip.listEntries()) {
			if (entry.toStdString().find("Profiles/") !=
				    std::string::npos &&
			    entry.toStdString().find("manifest.json") !=
				    std::string::npos)
			{
				std::string jsonStr = zip.extractFileToString(entry).toStdString();
				auto j = nlohmann::json::parse(jsonStr, nullptr, false);

				if (!j.is_discarded() && j.contains("Controllers") && j["Controllers"].is_array()) {
					for (auto& controller : j["Controllers"]) {
						if (controller.contains("Type") && controller["Type"] == "Keypad" &&
							controller.contains("Actions") && !controller["Actions"].is_null()) {

							chosenManifestPath = entry.toStdString();
							manifest = j;
							break;
						}
					}
				}
			}
			if (!chosenManifestPath.empty())
				break;
		}

		if (chosenManifestPath.empty()) {
			qWarning() << "SdaFile:" << originalPath_ << "no valid manifest found";
			return;
		}

		// Extract Controllers Actions "0,0" first state
		auto& controllers = manifest["Controllers"];
		for (auto& controller : manifest["Controllers"]) {
			if (!controller.contains("Actions")) continue;
			auto& actions = controller["Actions"];
			if (!actions.contains("0,0")) continue;

			auto& action = actions["0,0"];
			if (!action.contains("States") || !action["States"].is_array() || action["States"].empty())
				continue;

			auto& stateJson = action["States"][0];
			bool hasImage = stateJson.contains("Image");
			bool hasTitle = stateJson.contains("Title");
			std::string actionId = action.contains("UUID") ? action["UUID"] : "";

			std::string relImagePath = hasImage ? stateJson["Image"] : "";
			std::string title = hasTitle ? stateJson["Title"] : "";

			// Build full relative path
			std::string manifestDir = chosenManifestPath.substr(0, chosenManifestPath.find_last_of('/') + 1);
			std::string imagePath = manifestDir + relImagePath;

			SdaState s;
			s.path = originalPath_;
			s.title = QString::fromStdString(title);
			s.titleAlign = SdaIconVerticalAlign::Bottom;
			if (hasTitle && stateJson.contains("TitleAlignment")) {
				std::string align = stateJson["TitleAlignment"];
				s.titleAlign = align == "top" ? SdaIconVerticalAlign::Top 
					: align == "middle" ? SdaIconVerticalAlign::Middle 
					: SdaIconVerticalAlign::Bottom;
			}
			s.relativeImagePath = QString::fromStdString(relImagePath);
			s.hasTitle = hasTitle;
			s.hasImage = hasImage;

			if (hasImage && zip.contains(imagePath.c_str())) {
				QByteArray imgBytes = zip.extractFileToMemory(imagePath.c_str());
				s.imageBytes = imgBytes;
			} else if (actionId != "") {
				std::string iconPath = imageBaseDir + actionId + ".png";
				std::string defaultIconPath = imageBaseDir + "default.png";
				if (std::filesystem::exists(iconPath) &&
					std::filesystem::is_regular_file(iconPath)) {
					QFile file(iconPath.c_str());
					if (file.open(QIODevice::ReadOnly)) {
						QByteArray data = file.readAll();
						s.imageBytes = data;
						s.hasImage = true;
						file.close();
					}
				} else if (std::filesystem::exists(defaultIconPath) &&
					std::filesystem::is_regular_file(
					defaultIconPath)) {
					QFile file(defaultIconPath.c_str());
					if (file.open(QIODevice::ReadOnly)) {
						QByteArray data =
							file.readAll();
						s.imageBytes = data;
						s.hasImage = true;
						file.close();
					}
				}
			}

			state_ = std::move(s);
			valid_ = true;
		}
	}
	catch (const std::exception& ex) {
		qWarning() << "SdaFile error parsing" << originalPath_ << ":" << ex.what();
	}
}

std::optional<SdaState> SdaFile::firstState() const
{
	return state_;
}

SdProfileFile::SdProfileFile(const QString &path) : originalPath_(path)
{
	parse_();
}

void SdProfileFile::parse_()
{
	try {
		ZipArchive zip;
		zip.openExisting(originalPath_);

		// Check if this is a new or legacy file
		std::string packageJsonPath = "package.json";
		version_ = zip.contains(packageJsonPath.c_str())
				   ? SdFileVersion::Current
				   : SdFileVersion::Legacy;

		// Find the correct manifest.json under Profiles/*

		if (version_ == SdFileVersion::Legacy) {
			std::string manifestPath;
			for (const auto &entry : zip.listEntries()) {
				std::string entryStr = entry.toStdString();
				// We only want "something/manifest.json" (but not Profiles/*)
				if (entryStr.find('/') !=
					    std::string::npos &&
				    entryStr.rfind(
					    "manifest.json") ==
					    entry.size() -
						    std::string("manifest.json")
							    .size()) {

					// Ensure it's not Profiles/.../manifest.json
					if (entryStr.find(
						    "Profiles/") ==
					    std::string::npos) {
						manifestPath = entryStr;
						break;
					}
				}
			}

			if (manifestPath.empty()) {
				errorMsg_ =
					"No top-level manifest.json found in " +
					originalPath_.toStdString();
				return;
			}

			std::string jsonStr = zip.extractFileToString(manifestPath.c_str()).toStdString();
			auto j = nlohmann::json::parse(jsonStr, nullptr, false);
			if (j.is_discarded()) {
				errorMsg_ =
					"Failed to parse manifest.json in " +
					manifestPath;
				return;
			}
			if (j.contains("Device") &&
			    j["Device"].contains("Model")) {
				std::string modelStr = j["Device"]["Model"];
				state_.model = modelStr.c_str();
			} else {
				state_.model = "Unknown Model";
			}

			if (j.contains("Name")) {
				std::string nameStr = j["Name"];
				state_.name = nameStr.c_str();
			} else {
				state_.name = "No Name";
			}
		} else {
			std::string packageContents;
			std::string manifestContents;
			const std::string packagePath = "package.json";
			bool packageFound = false;
			bool hasPackageJson = zip.contains(packagePath.c_str());
			if (hasPackageJson) {
				packageFound = true;
				packageContents =
					zip.extractFileToString(packagePath.c_str()).toStdString();
			} else {
				errorMsg_ =
					"No top-level package.json found in " +
					originalPath_.toStdString();
				return;
			}

			std::optional<std::string> manifestPath;

			static const std::string profilesPrefix = "Profiles/";
			static const std::string manifestSuffix = "/manifest.json";

			for (const auto &entry : zip.listEntries()) {
				if (!endsWith(entry.toStdString(),
					      manifestSuffix))
					continue;
				auto parts = splitPath(entry.toStdString());
				if (parts.size() == 3 &&
				    parts[0] == "Profiles" &&
				    parts[2] == "manifest.json") {
					manifestPath = entry.toStdString();
					break;
				}
			}

			if (manifestPath) {
				manifestContents = zip.extractFileToString((*manifestPath).c_str()).toStdString();
			} else {
				errorMsg_ =
					"No manifest.json found in " +
					originalPath_.toStdString();
				return;
			}

			auto package = nlohmann::json::parse(packageContents, nullptr, false);
			auto manifest = nlohmann::json::parse(manifestContents, nullptr, false);
			if (package.is_discarded()) {
				errorMsg_ = "Failed to parse package.json in " + packagePath;
				return;
			}
			if (manifest.is_discarded()) {
				errorMsg_ = "Failed to parse manifest.json in " +  *manifestPath;
				return;
			}

			if (package.contains("DeviceModel")) {
				std::string modelStr = package["DeviceModel"];
				state_.model = modelStr.c_str();
			} else {
				state_.model = "Unknown Model";
			}

			if (manifest.contains("Name")) {
				std::string nameStr = manifest["Name"];
				state_.name = nameStr.c_str();
			} else {
				state_.name = "No Name";
			}
		}

		state_.path = originalPath_;

		valid_ = true;
	} catch (const std::exception &ex) {
		errorMsg_ = std::string("Error opening zip file: ") + ex.what();
	}
}