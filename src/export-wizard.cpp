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

#include <algorithm>

#include <util/platform.h>

#include "export-wizard.hpp"
#include "elgato-styles.hpp"
#include "plugins.hpp"
#include "plugin-support.h"
#include <QMainWindow>
#include <QVBoxLayout>
#include <QPushButton>
#include <QListWidget>
#include <QLineEdit>
#include <QDir>
#include <QSvgRenderer>
#include <QPainter>

#include "obs-utils.hpp"
#include "util.h"

namespace elgatocloud {

SceneBundle *bundle = new SceneBundle();

SceneBundleStatus createBundle(std::string filename,
			       std::vector<std::string> plugins,
			       std::map<std::string, std::string> vidDevLabels)
{
	if (!bundle) {
		return SceneBundleStatus::InvalidBundle;
	}
	return bundle->ToElgatoCloudFile(filename, plugins, vidDevLabels);
}

// TODO: For MacOS the filename sigatures will be different
const std::vector<std::string> ExcludedModules{
	// OBS built-in modules
	"win-wasapi.dll", "win-dshow.dll", "win-capture.dll", "vlc-video.dll",
	"text-freetype2.dll", "rtmp-services.dll", "obs-x264.dll",
	"obs-websocket.dll", "obs-webrtc.dll", "obs-vst.dll",
	"obs-transitions.dll", "obs-text.dll", "obs-qsv11.dll",
	"obs-outputs.dll", "obs-nvenc.dll", "obs-filters.dll", "obs-ffmpeg.dll",
	"obs-browser.dll", "nv-filters.dll", "image-source.dll",
	"frontend-tools.dll", "decklink-output-ui.dll", "decklink-captions.dll",
	"coreaudio-encoder.dll", "elgato-marketplace.dll"};


ExportStepsSideBar::ExportStepsSideBar(std::string name, QWidget* parent)
	: QWidget(parent)
{
	QVBoxLayout* layout = new QVBoxLayout(this);
	//auto header = new StreamPackageHeader(this, name, thumbnailPath);
	setFixedWidth(240);

	// TODO- translations for steps
	std::vector<std::string> steps = { "Get started", "Collect media files", "Bundle required plugins", "Rename video sources" };

	std::string titleString = obs_module_text("ExportWizard.Export");
	titleString += " " + name;

	auto header = new StreamPackageHeader(this, titleString, "");

	_stepper = new Stepper(steps, this);
	layout->setSpacing(16);
	layout->setContentsMargins(8, 0, 24, 0);
	layout->addWidget(header);
	layout->addWidget(_stepper);
	layout->addStretch();
}

void ExportStepsSideBar::setStep(int step)
{
	_stepper->setStep(step);
}

void ExportStepsSideBar::incrementStep()
{
	_stepper->incrementStep();
}

void ExportStepsSideBar::decrementStep()
{
	_stepper->decrementStep();
}

StartExport::StartExport(std::string name, QWidget* parent)
	: QWidget(parent)
{
	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setSpacing(16);
	layout->addStretch();

	auto placeholder = new CameraPlaceholder(8, this);
	std::string imageBaseDir = GetDataPath();
	imageBaseDir += "/images/";
	std::string imgPath = imageBaseDir + "icon-scene-collection.svg";
	placeholder->setIcon(imgPath.c_str());
	placeholder->setFixedWidth(320);

	auto placeholderLayout = new QHBoxLayout();
	placeholderLayout->addStretch();
	placeholderLayout->addWidget(placeholder);
	placeholderLayout->addStretch();
	layout->addLayout(placeholderLayout);

	std::string titleString = obs_module_text("ExportWizard.Export");
	titleString += " " + name;
	auto title = new QLabel(titleString.c_str(), this);
	title->setStyleSheet(EWizardStepTitle);
	title->setAlignment(Qt::AlignCenter);
	title->setFixedWidth(320);
	title->setWordWrap(true);

	auto titleLayout = new QHBoxLayout();
	titleLayout->addStretch();
	titleLayout->addWidget(title);
	titleLayout->addStretch();
	layout->addLayout(titleLayout);

	auto subTitle = new QLabel(obs_module_text("ExportWizard.StartExport.Description"), this);
	subTitle->setStyleSheet(EWizardStepSubTitle);
	subTitle->setFixedWidth(320);
	subTitle->setAlignment(Qt::AlignCenter);
	subTitle->setWordWrap(true);

	auto subTitleLayout = new QHBoxLayout();
	subTitleLayout->addStretch();
	subTitleLayout->addWidget(subTitle);
	subTitleLayout->addStretch();
	layout->addLayout(subTitleLayout);

	QHBoxLayout* buttons = new QHBoxLayout();
	QPushButton* continueButton = new QPushButton(this);
	continueButton->setText(obs_module_text("ExportWizard.GetStartedButton"));
	continueButton->setStyleSheet(EWizardButtonStyle);

	connect(continueButton, &QPushButton::released, this,
		[this]() { emit continuePressed(); });
	buttons->addStretch();
	buttons->addWidget(continueButton);
	buttons->addStretch();
	layout->addLayout(buttons);
	layout->addStretch();
}

FileCollectionCheck::FileCollectionCheck(std::string name,
					 std::vector<std::string> files, QWidget* parent)
	: QWidget(parent),
	  _files(files)
{
	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	auto main = new QHBoxLayout();
	main->setAlignment(Qt::AlignTop);
	main->setContentsMargins(0, 0, 0, 0);
	auto sideBar = new ExportStepsSideBar(name, this);
	sideBar->setContentsMargins(0, 0, 0, 0);
	sideBar->setFixedWidth(240);
	sideBar->setStep(1);

	auto form = new QVBoxLayout();

	auto title = new QLabel(this);
	const char* titleLookup = _files.size() == 1 ? "ExportWizard.MediaFileCheck.TitleSingular" : "ExportWizard.MediaFileCheck.TitlePlural";
	const char* subtitleLookup = _files.size() == 1 ? "ExportWizard.MediaFileCheck.SubtitleSingular" : "ExportWizard.MediaFileCheck.SubtitlePlural";

	if (_files.size() > 0) {
		std::string titleText = std::to_string(_files.size()) + " " + std::string(obs_module_text(titleLookup));
		title->setText(titleText.c_str());
		title->setStyleSheet(EWizardStepTitle);
		std::string subtitleText = obs_module_text(subtitleLookup);
		auto subTitle = new QLabel(this);
		subTitle->setText(subtitleText.c_str());
		subTitle->setStyleSheet(EWizardStepSubTitle);
		subTitle->setWordWrap(true);

		auto fileList = new QListWidget(this);
		std::vector<std::string> browserSourceDirs;

		for (auto fileName : _files) {
			bool hasExtension = fileName.rfind(".") != std::string::npos;
			std::string extension =
				hasExtension ? os_get_path_extension(fileName.c_str())
				: "";
			if (extension == ".html" || extension == ".htm") {
				if (fileName.rfind("/") == std::string::npos) {
					obs_log(LOG_INFO, "Error exporting file- could not determine parent directory of file.");
					continue;
				}
				std::string parentDir = fileName.substr(0, fileName.rfind("/"));
				if (std::find(browserSourceDirs.begin(), browserSourceDirs.end(), parentDir) == browserSourceDirs.end()) {
					browserSourceDirs.push_back(parentDir);
					std::vector<std::string> parentDirFiles;
					_SubFiles(parentDirFiles, parentDir);
					for (auto parentFileName : parentDirFiles) {
						fileList->addItem(parentFileName.c_str());
					}
				}
			}
			else {
				fileList->addItem(fileName.c_str());
			}
		}
		fileList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		fileList->setStyleSheet(EListStyle);
		fileList->setSpacing(4);
		fileList->setSelectionMode(QAbstractItemView::NoSelection);
		fileList->setFocusPolicy(Qt::FocusPolicy::NoFocus);

		form->addWidget(title);
		form->addWidget(subTitle);
		form->addWidget(fileList);
	} else {
		form->addStretch();
		std::string titleText = "No " + std::string(obs_module_text(titleLookup));
		title->setText(titleText.c_str());
		title->setStyleSheet(EWizardStepTitle);
		title->setAlignment(Qt::AlignCenter);

		auto subTitle = new QLabel(this);
		subTitle->setText(obs_module_text("ExportWizard.MediaFileCheck.SubtitleNone"));
		subTitle->setStyleSheet(EWizardStepSubTitle);
		subTitle->setWordWrap(true);
		subTitle->setAlignment(Qt::AlignCenter);

		auto iconLayout = new QHBoxLayout();

		QSize iconSize(48, 48);  // Set desired icon size
		QPixmap pixmap(iconSize);
		pixmap.fill(Qt::transparent);  // Ensure transparent background

		std::string imageBaseDir = GetDataPath();
		imageBaseDir += "/images/";
		std::string imgPath = imageBaseDir + "icon-checkmark-circle.svg";

		QSvgRenderer renderer(QString(imgPath.c_str()), this);
		QPainter painter(&pixmap);
		renderer.render(&painter);

		// Display it in a QLabel
		QLabel* iconLabel = new QLabel;
		iconLabel->setPixmap(pixmap);
		iconLabel->setFixedSize(iconSize);
		iconLayout->addStretch();
		iconLayout->addWidget(iconLabel);
		iconLayout->addStretch();

		form->addLayout(iconLayout);
		form->addWidget(title);
		form->addWidget(subTitle);
		form->addStretch();
	}

	//form->addStretch();
	main->addWidget(sideBar);
	main->addLayout(form);

	QHBoxLayout *buttons = new QHBoxLayout();

	QPushButton *continueButton = new QPushButton(this);
	continueButton->setText(obs_module_text("ExportWizard.NextButton"));
	continueButton->setStyleSheet(EWizardButtonStyle);
	connect(continueButton, &QPushButton::released, this,
		[this]() { emit continuePressed(); });

	buttons->addStretch();
	buttons->addWidget(continueButton);
	layout->addLayout(main);
	layout->addLayout(buttons);
}

bool FileCollectionCheck::_SubFiles(std::vector<std::string>& files, std::string curDir)
{
	os_dir_t* dir = os_opendir(curDir.c_str());
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
				std::string dPath = curDir + "/" + dName;
				if (!_SubFiles(files, dPath)) {
					os_closedir(dir);
					return false;
				}
			} else {
				std::string filename = ent->d_name;
				std::string filePath = curDir + "/" + filename;
				files.push_back(filePath);
			}
		}
	} else {
		obs_log(LOG_ERROR, "Fatal: Could not open directory: %s",
			curDir.c_str());
		return false;
	}
	os_closedir(dir);
	return true;
}

VideoSourceLabels::VideoSourceLabels(std::string name,
				     std::map<std::string, std::string> devices, QWidget* parent)
	: QWidget(parent)
{
	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	auto main = new QHBoxLayout();
	main->setAlignment(Qt::AlignTop);
	main->setContentsMargins(0, 0, 0, 0);
	auto sideBar = new ExportStepsSideBar(name, this);
	sideBar->setContentsMargins(0, 0, 0, 0);
	sideBar->setFixedWidth(240);
	sideBar->setStep(3);

	auto formLayout = new QVBoxLayout();

	auto continueButton = new QPushButton(this);
	formLayout->setSpacing(4);
	if (devices.size() > 0) {
		auto title = new QLabel(obs_module_text("ExportWizard.VideoSourceLabels.Title"), this);
		title->setStyleSheet(EWizardStepTitle);
		auto subTitle = new QLabel(obs_module_text("ExportWizard.VideoSourceLabel.SubTitle"), this);
		subTitle->setStyleSheet(EWizardStepSubTitle);
		subTitle->setWordWrap(true);
		std::string imageBaseDir = GetDataPath();
		imageBaseDir += "/images/";
		std::string cameraIconPath = imageBaseDir + "icon-camera.svg";
		std::string arrowIconPath = imageBaseDir + "icon-arrow-right.svg";

		// TODO: Translation files for these two fields
		auto videoGrid = new QGridLayout;
		videoGrid->setContentsMargins(0, 16, 0, 0);
		videoGrid->setSpacing(12);
		auto currentLabel = new QLabel("Current", this);
		currentLabel->setStyleSheet(EWizardFieldLabel);
		auto newLabel = new QLabel("New", this);
		newLabel->setStyleSheet(EWizardFieldLabel);
		
		videoGrid->addWidget(currentLabel, 0, 0, Qt::AlignLeft);
		videoGrid->addWidget(newLabel, 0, 2);

		int i = 1;
		int row = 1;
		for (auto const &[key, val] : devices) {
			auto label = new IconLabel(cameraIconPath, val.c_str(), this);

			QLabel* arrow = new QLabel(this);
			QSize iconSize(20, 20);
			QPixmap pixmap(iconSize);
			pixmap.fill(Qt::transparent);

			QSvgRenderer renderer(QString(arrowIconPath.c_str()));
			QPainter painter(&pixmap);
			renderer.render(&painter);

			arrow->setPixmap(pixmap);
			arrow->setFixedSize(iconSize);

			auto field = new QLineEdit(val.c_str(), this);
			field->setPlaceholderText(
				obs_module_text("ExportWizard.VideoSourceLabels.InputPlaceholder"));
			field->setMaxLength(32);
			field->setStyleSheet(EWizardTextField);
			
			
			videoGrid->addWidget(label, row, 0, Qt::AlignLeft);
			videoGrid->addWidget(arrow, row, 1, Qt::AlignCenter);
			videoGrid->addWidget(field, row, 2);

			//formLayout->addWidget(label);
			//formLayout->addWidget(field);
			_labels[val] = val.c_str();
			connect(field, &QLineEdit::textChanged, this,
				[this, val,
				 continueButton](const QString &text) {
					_labels[val] = text.toStdString();
					bool enable = true;
					for (auto const &[n, l] : _labels) {
						if (l.size() == 0) {
							enable = false;
							break;
						}
					}
					continueButton->setDisabled(!enable);
				});
			row++;
		}
		formLayout->addWidget(title);
		formLayout->addWidget(subTitle);
		formLayout->addLayout(videoGrid);
	} else {
		continueButton->setDisabled(false);
		formLayout->addStretch();
		auto noVCDevices =
			new QLabel(
				obs_module_text("ExportWizard.VideoSourceLabels.NoCaptureSourcesText"), this);
		noVCDevices->setAlignment(Qt::AlignCenter);
		noVCDevices->setStyleSheet(EWizardStepTitle);

		auto iconLayout = new QHBoxLayout();

		QSize iconSize(48, 48);  // Set desired icon size
		QPixmap pixmap(iconSize);
		pixmap.fill(Qt::transparent);  // Ensure transparent background

		std::string imageBaseDir = GetDataPath();
		imageBaseDir += "/images/";
		std::string imgPath = imageBaseDir + "icon-checkmark-circle.svg";

		QSvgRenderer renderer(QString(imgPath.c_str()), this);
		QPainter painter(&pixmap);
		renderer.render(&painter);

		// Display it in a QLabel
		QLabel* iconLabel = new QLabel;
		iconLabel->setPixmap(pixmap);
		iconLabel->setFixedSize(iconSize);
		iconLayout->addStretch();
		iconLayout->addWidget(iconLabel);
		iconLayout->addStretch();

		auto noneLayout = new QHBoxLayout();
		

		auto noVCDevicesSub = new QLabel(obs_module_text("ExportWizard.VideoSourceLabels.NoCaptureSourcesSub"), this);
		noVCDevicesSub->setAlignment(Qt::AlignCenter);
		noVCDevicesSub->setStyleSheet(EWizardStepSubTitle);
		formLayout->addLayout(iconLayout);
		formLayout->addWidget(noVCDevices);
		formLayout->addWidget(noVCDevicesSub);
	}

	formLayout->addStretch();

	auto buttons = new QHBoxLayout();
	auto backButton = new QPushButton(this);
	backButton->setText(
		obs_module_text("ExportWizard.BackButton"));
	backButton->setStyleSheet(EWizardQuietButtonStyle);
	connect(backButton, &QPushButton::released, this,
		[this]() { emit backPressed(); });

	continueButton->setText(
		obs_module_text("ExportWizard.NextButton"));
	continueButton->setStyleSheet(EWizardButtonStyle);

	connect(continueButton, &QPushButton::released, this,
		[this]() { emit continuePressed(); });
	buttons->addStretch();
	buttons->addWidget(backButton);
	buttons->addWidget(continueButton);
	main->addWidget(sideBar);
	main->addLayout(formLayout);
	layout->addLayout(main);
	layout->addLayout(buttons);
}

RequiredPlugins::RequiredPlugins(std::string name,
				 std::vector<obs_module_t *> installedPlugins, QWidget* parent)
	: QWidget(parent)
{
	PluginInfo pi;

	auto installed = pi.installed();
	// Set up the paths for checked/unchecked images.
	// TODO: Refactor this, there must be a better way.
	//std::string imagesPath = obs_get_module_data_path(obs_current_module());
	//imagesPath += "/images/";
	std::string imagesPath = getImagesPath();
	std::string checkedImage = imagesPath + "checkbox_checked.png";
	std::string uncheckedImage = imagesPath + "checkbox_unchecked.png";
	QString checklistStyle = EWizardChecklistStyle;
	obs_log(LOG_INFO, "checked image: %s", checkedImage.c_str());
	checklistStyle.replace("${checked-img}", checkedImage.c_str());
	checklistStyle.replace("${unchecked-img}", uncheckedImage.c_str());
	obs_log(LOG_INFO, "style: %s", checklistStyle.toStdString().c_str());

	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	auto main = new QHBoxLayout();
	main->setAlignment(Qt::AlignTop);
	main->setContentsMargins(0, 0, 0, 0);
	auto sideBar = new ExportStepsSideBar(name, this);
	sideBar->setContentsMargins(0, 0, 0, 0);
	sideBar->setFixedWidth(240);
	sideBar->setStep(2);

	auto formLayout = new QVBoxLayout();

	if (installed.size() > 0) {
		std::string titleText = installed.size() == 1 ? obs_module_text("ExportWizard.RequiredPlugins.SingleTitle") : std::to_string(installed.size()) + " " + obs_module_text("ExportWizard.RequiredPlugins.PluralTitle");
		auto title = new QLabel(titleText.c_str(), this);
		title->setStyleSheet(EWizardStepTitle);
		
		auto subTitle = new QLabel(this);
		subTitle->setText(obs_module_text("ExportWizard.RequiredPlugins.SubTitle"));
		subTitle->setStyleSheet(EWizardStepSubTitle);

		auto pluginList = new QListWidget(this);
		pluginList->setSizePolicy(QSizePolicy::Preferred,
					  QSizePolicy::Expanding);
		pluginList->setSpacing(8);
		pluginList->setStyleSheet(checklistStyle);
		pluginList->setSelectionMode(QAbstractItemView::NoSelection);
		pluginList->setFocusPolicy(Qt::FocusPolicy::NoFocus);

		for (auto module : installed) {

			_pluginStatus[module.name] =
				std::pair<bool, std::string>(false,
							     module.files[0]);
			pluginList->addItem(module.name.c_str());
		}

		QListWidgetItem *item = nullptr;
		for (int i = 0; i < pluginList->count(); ++i) {
			item = pluginList->item(i);
			item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
			item->setCheckState(Qt::Unchecked);
		}

		connect(pluginList, &QListWidget::itemChanged, this,
			[this](QListWidgetItem *item) {
				std::string filename =
					item->text().toStdString();
				_pluginStatus[filename].first =
					item->checkState() == Qt::Checked;
			});

		formLayout->addWidget(title);
		formLayout->addWidget(subTitle);
		formLayout->addWidget(pluginList);
	} else {
		auto topSpace = new QWidget(this);
		topSpace->setSizePolicy(QSizePolicy::Preferred,
					QSizePolicy::Expanding);
		formLayout->addWidget(topSpace);
		auto noPlugins =
			new QLabel(
				obs_module_text("ExportWizard.RequiredPlugins.NoPluginsFound"), this);
		noPlugins->setAlignment(Qt::AlignCenter);
		noPlugins->setStyleSheet("QLabel{font-size: 18pt;}");
		formLayout->addWidget(noPlugins);
		formLayout->addStretch();
	}

	auto buttons = new QHBoxLayout();
	auto backButton = new QPushButton(this);
	backButton->setText(
		obs_module_text("ExportWizard.BackButton"));
	backButton->setStyleSheet(EWizardQuietButtonStyle);
	connect(backButton, &QPushButton::released, this,
		[this]() { emit backPressed(); });

	auto continueButton = new QPushButton(this);
	continueButton->setText(
		obs_module_text("ExportWizard.NextButton"));
	continueButton->setStyleSheet(EWizardButtonStyle);

	connect(continueButton, &QPushButton::released, this,
		[this]() { emit continuePressed(); });
	buttons->addStretch();
	buttons->addWidget(backButton);
	buttons->addWidget(continueButton);

	main->addWidget(sideBar);
	main->addLayout(formLayout);
	layout->addLayout(main);
	layout->addLayout(buttons);
}

ExportComplete::ExportComplete(std::string name, QWidget *parent) : QWidget(parent)
{

	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setSpacing(16);
	layout->addStretch();

	auto placeholder = new CameraPlaceholder(8, this);
	std::string imageBaseDir = GetDataPath();
	imageBaseDir += "/images/";
	std::string imgPath = imageBaseDir + "icon-scene-collection.svg";
	placeholder->setIcon(imgPath.c_str());
	placeholder->setFixedWidth(320);

	auto placeholderLayout = new QHBoxLayout();
	placeholderLayout->addStretch();
	placeholderLayout->addWidget(placeholder);
	placeholderLayout->addStretch();
	layout->addLayout(placeholderLayout);

	std::string titleString = obs_module_text("ExportWizard.Export");
	std::string complete = obs_module_text("ExportWizard.Complete");
	titleString += " " + name + " " + complete;
	auto title = new QLabel(titleString.c_str(), this);
	title->setStyleSheet(EWizardStepTitle);
	title->setAlignment(Qt::AlignCenter);
	title->setFixedWidth(320);
	title->setWordWrap(true);

	auto titleLayout = new QHBoxLayout();
	titleLayout->addStretch();
	titleLayout->addWidget(title);
	titleLayout->addStretch();
	layout->addLayout(titleLayout);

	auto subTitle = new QLabel(obs_module_text("ExportWizard.ExportComplete.Text"), this);
	subTitle->setStyleSheet(EWizardStepSubTitle);
	subTitle->setFixedWidth(320);
	subTitle->setAlignment(Qt::AlignCenter);
	subTitle->setWordWrap(true);

	auto subTitleLayout = new QHBoxLayout();
	subTitleLayout->addStretch();
	subTitleLayout->addWidget(subTitle);
	subTitleLayout->addStretch();
	layout->addLayout(subTitleLayout);

	QHBoxLayout* buttons = new QHBoxLayout();
	auto closeButton = new QPushButton(this);
	closeButton->setText(
		obs_module_text("ExportWizard.CloseButton"));
	closeButton->setStyleSheet(EWizardButtonStyle);
	connect(closeButton, &QPushButton::released, this,
		[this]() { emit closePressed(); });

	connect(closeButton, &QPushButton::released, this,
		[this]() { emit closePressed(); });

	buttons->addStretch();
	buttons->addWidget(closeButton);
	buttons->addStretch();
	layout->addLayout(buttons);
	layout->addStretch();
}

std::vector<std::string> RequiredPlugins::RequiredPluginList()
{
	std::vector<std::string> reqList;
	for (auto const &[key, val] : _pluginStatus) {
		if (val.first) {
			reqList.push_back(val.second);
		}
	}
	return reqList;
}

Exporting::Exporting(std::string name, QWidget *parent) : QWidget(parent)
{
	QVBoxLayout *layout = new QVBoxLayout(this);

	auto spacerTop = new QWidget(this);
	spacerTop->setSizePolicy(QSizePolicy::Preferred,
				 QSizePolicy::Expanding);
	layout->addWidget(spacerTop);

	std::string imagesPath = obs_get_module_data_path(obs_current_module());
	imagesPath += "/images/";
	std::string spinnerImage = imagesPath + "spinner-white.gif";

	_indicator = new QMovie(spinnerImage.c_str());

	auto spinner = new QLabel(this);
	spinner->setMovie(_indicator);
	_indicator->start();
	spinner->setAlignment(Qt::AlignCenter);
	layout->addWidget(spinner);

	auto title = new QLabel(this);
	title->setText(
		obs_module_text("ExportWizard.Exporting.Title"));
	title->setStyleSheet(ETitleStyle);
	title->setAlignment(Qt::AlignCenter);
	layout->addWidget(title);

	auto subTitle = new QLabel(this);
	subTitle->setText(
		obs_module_text("ExportWizard.Exporting.Text"));
	subTitle->setStyleSheet(
		"QLabel{ font-size: 11pt; font-style: italic; }");
	subTitle->setAlignment(Qt::AlignCenter);
	subTitle->setWordWrap(true);
	layout->addWidget(subTitle);

	auto spacerBottom = new QWidget(this);
	spacerBottom->setSizePolicy(QSizePolicy::Preferred,
				    QSizePolicy::Expanding);
	layout->addWidget(spacerBottom);
}

Exporting::~Exporting() {}

StreamPackageExportWizard::StreamPackageExportWizard(QWidget *parent)
	: QDialog(parent),
	  _steps(nullptr)
{
	// Save the current scene collection to ensure our output is the latest
	obs_frontend_save();

	// Since scene collection save is threaded as a Queued connection
	// queue the SetupUI to execute *after* the frontend is completely saved.
	QMetaObject::invokeMethod(this, "SetupUI", Qt::QueuedConnection);
}

void StreamPackageExportWizard::SetupUI()
{
	obs_enum_modules(StreamPackageExportWizard::AddModule, this);
	setWindowTitle("Elgato Marketplace Scene Collection Export");
	setStyleSheet("background-color: #151515");
	std::string homeDir = QDir::homePath().toStdString();

	char* currentCollection = obs_frontend_get_current_scene_collection();
	std::string collectionName = currentCollection;
	bfree(currentCollection);

	if (!bundle->FromCollection(collectionName)) {
		return;
	}
	std::vector<std::string> fileList = bundle->FileList();
	std::map<std::string, std::string> vidDevs =
		bundle->VideoCaptureDevices();

	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setContentsMargins(8, 8, 8, 8);
	_steps = new QStackedWidget(this);
	_steps->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	// Step 1- Getting started (step index: 0)
	auto startExport = new StartExport(collectionName, this);
	connect(startExport, &StartExport::continuePressed, this,
		[this]() { _steps->setCurrentIndex(1); });
	_steps->addWidget(startExport);


	// Step 2- check the media files to be bundled (step index: 1)
	auto fileCheck = new FileCollectionCheck(collectionName, fileList, this);
	connect(fileCheck, &FileCollectionCheck::continuePressed, this,
		[this]() { _steps->setCurrentIndex(2); });
	connect(fileCheck, &FileCollectionCheck::cancelPressed, this,
		[this]() { close(); });
	_steps->addWidget(fileCheck);

	// Step 3- Specify required plugins (step index: 2)
	auto reqPlugins = new RequiredPlugins(collectionName, _modules, this);
	connect(reqPlugins, &RequiredPlugins::continuePressed, this, [this]() {
		_steps->setCurrentIndex(3);
		});
	connect(reqPlugins, &RequiredPlugins::backPressed, this,
		[this]() { _steps->setCurrentIndex(1); });
	_steps->addWidget(reqPlugins);

	// Step 4- Add labels for each video capture device source (step index: 3)
	auto videoLabels = new VideoSourceLabels(collectionName, vidDevs, this);
	connect(videoLabels, &VideoSourceLabels::continuePressed, this,
		[this, videoLabels, reqPlugins]() {
			
			auto vidDevLabels = videoLabels->Labels();
			auto plugins = reqPlugins->RequiredPluginList();
			QWidget* window = (QWidget*)obs_frontend_get_main_window();
			QString filename = QFileDialog::getSaveFileName(
				window, "Save As...", QString(), "*.elgatoscene");
			if (filename == "") {
				_steps->setCurrentIndex(3);
				return;
			}
			if (!filename.endsWith(".elgatoscene")) {
				filename += ".elgatoscene";
			}
			_steps->setCurrentIndex(4);
			std::string filename_utf8 = filename.toUtf8().constData();
			// This is a bit of threading hell.
			// Find a better way to set this up, perhaps in an
			// installer handler thread handling object.
			_future =
				QtConcurrent::run(createBundle, filename_utf8, plugins,
					vidDevLabels)
				.then([this](SceneBundleStatus status) {
				// We are now in a different thread, so we need to execute this
				// back in the gui thread.  See, threading hell.
				QMetaObject::invokeMethod(
					QCoreApplication::instance()
					->thread(), // main GUI thread
					[this, status]() {
						if (status ==
							SceneBundleStatus::
							Success) {
							_steps->setCurrentIndex(5);
						}
						else if (
							status ==
							SceneBundleStatus::
							Cancelled) {
							_steps->setCurrentIndex(3);
						}
					});
					})
				.onCanceled([]() {

				});
			//createBundle(filename_utf8, plugins, vidDevLabels);
		});


	connect(videoLabels, &VideoSourceLabels::backPressed, this,
		[this]() { _steps->setCurrentIndex(2); });
	_steps->addWidget(videoLabels);

	auto exporting = new Exporting(collectionName, this);
	_steps->addWidget(exporting);

	// Successful Export
	auto success = new ExportComplete(collectionName, this);
	connect(success, &ExportComplete::closePressed, this,
		[this]() { close(); });
	_steps->addWidget(success);
	layout->addWidget(_steps);
}

StreamPackageExportWizard::~StreamPackageExportWizard()
{
	bundle->interrupt(SceneBundleStatus::CallerDestroyed);
}

void StreamPackageExportWizard::AddModule(void *data, obs_module_t *module)
{
	auto wizard = static_cast<StreamPackageExportWizard *>(data);
	std::string filename = obs_get_module_file_name(module);
	if (std::find(ExcludedModules.begin(), ExcludedModules.end(),
		      filename) != ExcludedModules.end()) {
		return;
	}
	wizard->_modules.push_back(module);
}

void OpenExportWizard()
{
	const auto mainWindow =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());
	const QRect &hostRect = mainWindow->geometry();

	auto window = new StreamPackageExportWizard(mainWindow);
	window->setAttribute(Qt::WA_DeleteOnClose);
	window->setMinimumWidth(800);
	window->setMinimumHeight(600);
	window->show();
	window->move(hostRect.center() - window->rect().center());
}

} // namespace elgatocloud
