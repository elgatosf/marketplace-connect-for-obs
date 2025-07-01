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

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QListWidget>
#include <QScrollArea>
#include <QDesktopServices>
#include <QPixmap>
#include <QPainter>
#include <QString>
#include <QPalette>
#include <QPainterPath>
#include <QLineEdit>
#include <QApplication>
#include <QThread>
#include <QMetaObject>
#include <QMessageBox>
#include <QMainWindow>

#include "elgato-styles.hpp"
#include <plugin-support.h>
#include "setup-wizard.hpp"
#include "elgato-product.hpp"
#include "scene-bundle.hpp"
#include "obs-utils.hpp"
#include "util.h"
#include "platform.h"
#include "api.hpp"

namespace elgatocloud {

extern void ElgatoCloudWindowSetEnabled(bool enable);
StreamPackageSetupWizard *setupWizard = nullptr;

StreamPackageSetupWizard *GetSetupWizard()
{
	return setupWizard;
}

std::string GetBundleInfo(std::string filename)
{
	SceneBundle bundle;
	std::string data;
	try {
		data = bundle.ExtractBundleInfo(filename);
	} catch (...) {
		data = "{\"Error\": \"Incompatible File\"}";
	}
	return data;
}


StepsSideBar::StepsSideBar(std::vector<std::string> const& steps, std::string name, std::string thumbnailPath, QWidget* parent)
	: QWidget(parent)
{
	std::string nameText = obs_module_text("SetupWizard.ImportTitlePrefix");
	nameText += " " + name;

	QVBoxLayout* layout = new QVBoxLayout(this);
	auto header = new StreamPackageHeader(this, nameText, thumbnailPath);

	_stepper = new Stepper(steps, this);
	layout->setSpacing(16);
	layout->setContentsMargins(8, 0, 24, 0);
	layout->addWidget(header);
	layout->addWidget(_stepper);
	layout->addStretch();
}

void StepsSideBar::setStep(int step)
{
	_stepper->setStep(step);
}

void StepsSideBar::incrementStep()
{
	_stepper->incrementStep();
}

void StepsSideBar::decrementStep()
{
	_stepper->decrementStep();
}

MissingPlugins::MissingPlugins(QWidget *parent, std::string name,
			       std::string thumbnailPath,
			       std::vector<PluginDetails> &missing)
	: QWidget(parent)
{
	QVBoxLayout *layout = new QVBoxLayout(this);
	StreamPackageHeader *header = new StreamPackageHeader(parent, name, thumbnailPath);
	layout->addWidget(header);
	QLabel *title = new QLabel(this);
	title->setText(obs_module_text("SetupWizard.MissingPlugins.Title"));
	title->setAlignment(Qt::AlignCenter);
	title->setStyleSheet("QLabel {font-size: 14pt;}");
	layout->addWidget(title);

	QLabel *subTitle = new QLabel(this);
	subTitle->setText(obs_module_text("SetupWizard.MissingPlugins.Text"));
	subTitle->setAlignment(Qt::AlignCenter);
	subTitle->setWordWrap(true);
	subTitle->setStyleSheet("QLabel {font-size: 12pt;}");
	layout->addWidget(subTitle);

	auto pluginList = new QListWidget(this);
	for (auto &plugin : missing) {
		auto item = new QListWidgetItem();
		item->setSizeHint(QSize(0, 60));
		auto itemWidget = new MissingPluginItem(
			this, plugin.name.c_str(), plugin.url.c_str());
		pluginList->addItem(item);
		pluginList->setItemWidget(item, itemWidget);
	}

	pluginList->setStyleSheet(EMissingPluginsStyle);
	pluginList->setSpacing(4);
	pluginList->setSelectionMode(QAbstractItemView::NoSelection);
	pluginList->setFocusPolicy(Qt::FocusPolicy::NoFocus);

	layout->addWidget(pluginList);
}

MissingPluginItem::MissingPluginItem(QWidget *parent, std::string label,
				     std::string url)
	: QWidget(parent)
{
	auto layout = new QHBoxLayout(this);
	auto itemLabel = new QLabel(this);
	setStyleSheet("background-color: #232323;");
	itemLabel->setText(label.c_str());
	itemLabel->setStyleSheet(EWizardFieldLabel);
	layout->addWidget(itemLabel);

	auto spacer = new QWidget(this);
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	layout->addWidget(spacer);

	auto downloadButton = new QPushButton(this);
	downloadButton->setText(obs_module_text("SetupWizard.MissingPlugins.DownloadButton"));
	downloadButton->setStyleSheet(EWizardButtonStyle);

	connect(downloadButton, &QPushButton::released, this,
		[url]() { QDesktopServices::openUrl(QUrl(url.c_str())); });
	layout->addWidget(downloadButton);
}

MissingSourceClone::MissingSourceClone(std::string name, std::string thumbnailPath, QWidget* parent)
	: QWidget(parent)
{
	auto layout = new QVBoxLayout(this);
	layout->setSpacing(16);
	layout->addStretch();

	if (thumbnailPath != "") {
		auto thumbnail = new RoundedImageLabel(8, this);
		QPixmap image(thumbnailPath.c_str());
		thumbnail->setImage(image);
		thumbnail->setFixedWidth(320);
		//thumbnail->setMaximumWidth(320);
		thumbnail->setAlignment(Qt::AlignCenter);

		auto thumbLayout = centeredWidgetLayout(thumbnail);

		layout->addLayout(thumbLayout);
	}
	else {
		auto placeholder = new CameraPlaceholder(8, this);
		std::string imageBaseDir = GetDataPath();
		imageBaseDir += "/images/";
		std::string imgPath = imageBaseDir + "icon-scene-collection.svg";
		placeholder->setIcon(imgPath.c_str());
		placeholder->setFixedWidth(320);

		auto placeholderLayout = centeredWidgetLayout(placeholder);

		layout->addLayout(placeholderLayout);
	}

	QLabel* title = new QLabel(this);
	title->setText(obs_module_text("SetupWizard.MissingSourceClone.Title"));
	title->setStyleSheet(EWizardStepTitle);
	title->setAlignment(Qt::AlignCenter);
	title->setFixedWidth(320);
	title->setWordWrap(true);
	auto titleLayout = centeredWidgetLayout(title);
	layout->addLayout(titleLayout);

	auto subTitle = new QLabel(obs_module_text("SetupWizard.MissingSourceClone.Description"), this);
	subTitle->setStyleSheet(EWizardStepSubTitle);
	subTitle->setFixedWidth(320);
	subTitle->setAlignment(Qt::AlignCenter);
	subTitle->setWordWrap(true);

	auto subTitleLayout = centeredWidgetLayout(subTitle);
	layout->addLayout(subTitleLayout);

	// Setup New and Existing buttons.
	QHBoxLayout* buttons = new QHBoxLayout(this);
	QPushButton* backButton = new QPushButton(this);
	backButton->setText(obs_module_text("SetupWizard.BackButton"));
	backButton->setStyleSheet(EWizardQuietButtonStyle);

	connect(backButton, &QPushButton::released, this,
		[this]() { emit backPressed(); });

	QPushButton* getButton = new QPushButton(this);
	std::string sourceCloneUrl = "https://obsproject.com/forum/resources/source-clone.1632/";
	getButton->setText(obs_module_text("SetupWizard.MissingPlugins.DownloadButton"));
	getButton->setStyleSheet(EWizardButtonStyle);
	connect(getButton, &QPushButton::released, this,
		[sourceCloneUrl]() { 
			QDesktopServices::openUrl(QUrl(sourceCloneUrl.c_str()));
		});
	buttons->addStretch();
	buttons->addWidget(backButton);
	buttons->addWidget(getButton);
	buttons->addStretch();

	layout->addLayout(buttons);
}

InstallType::InstallType(QWidget *parent, std::string name,
			 std::string thumbnailPath)
	: QWidget(parent)
{
	QVBoxLayout *layout = new QVBoxLayout(this);
	StreamPackageHeader *header =
		new StreamPackageHeader(parent, name, thumbnailPath);
	layout->addWidget(header);
	QLabel *selectInstall = new QLabel(this);
	selectInstall->setText(obs_module_text("SetupWizard.SelectInstall.Title"));
	selectInstall->setAlignment(Qt::AlignCenter);
	selectInstall->setStyleSheet("QLabel {font-size: 14pt;}");
	layout->addWidget(selectInstall);

	// Setup New and Existing buttons.
	QHBoxLayout *buttons = new QHBoxLayout(this);
	QPushButton *newButton = new QPushButton(this);
	newButton->setText(obs_module_text("SetupWizard.SelectInstall.NewInstallButton"));
	newButton->setStyleSheet(EPushButtonDarkStyle);

	connect(newButton, &QPushButton::released, this,
		[this]() { emit newCollectionPressed(); });
	QPushButton *existingButton = new QPushButton(this);
	existingButton->setText(obs_module_text("SetupWizard.SelectInstall.ExistingInstallButton"));
	existingButton->setStyleSheet(EPushButtonDarkStyle);
	existingButton->setDisabled(true);
	connect(existingButton, &QPushButton::released, this,
		[this]() { emit existingCollectionPressed(); });
	buttons->addWidget(newButton);
	buttons->addWidget(existingButton);

	layout->addLayout(buttons);
}

StartInstall::StartInstall(QWidget* parent, std::string name,
	std::string thumbnailPath)
	: QWidget(parent)
{
	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setSpacing(16);
	layout->addStretch();

	if (thumbnailPath != "") {
		auto thumbnail = new RoundedImageLabel(8, this);
		QPixmap image(thumbnailPath.c_str());
		thumbnail->setImage(image);
		thumbnail->setFixedWidth(320);
		//thumbnail->setMaximumWidth(320);
		thumbnail->setAlignment(Qt::AlignCenter);

		auto thumbLayout = centeredWidgetLayout(thumbnail);

		layout->addLayout(thumbLayout);
	} else {
		auto placeholder = new CameraPlaceholder(8, this);
		std::string imageBaseDir = GetDataPath();
		imageBaseDir += "/images/";
		std::string imgPath = imageBaseDir + "icon-scene-collection.svg";
		placeholder->setIcon(imgPath.c_str());
		placeholder->setFixedWidth(320);

		auto placeholderLayout = centeredWidgetLayout(placeholder);

		layout->addLayout(placeholderLayout);
	}

	std::string nameText = obs_module_text("SetupWizard.ImportTitlePrefix");
	nameText += " " + name;
	auto title = new QLabel(nameText.c_str(), this);
	title->setStyleSheet(EWizardStepTitle);
	title->setAlignment(Qt::AlignCenter);
	title->setFixedWidth(320);
	title->setWordWrap(true);

	auto titleLayout = centeredWidgetLayout(title);
	layout->addLayout(titleLayout);

	auto subTitle = new QLabel(obs_module_text("SetupWizard.StartInstallation.Description"), this);
	subTitle->setStyleSheet(EWizardStepSubTitle);
	subTitle->setFixedWidth(320);
	subTitle->setAlignment(Qt::AlignCenter);
	subTitle->setWordWrap(true);

	auto subTitleLayout = centeredWidgetLayout(subTitle);
	layout->addLayout(subTitleLayout);

	QHBoxLayout* buttons = new QHBoxLayout();
	QPushButton* newCollectionButton = new QPushButton(this);
	newCollectionButton->setText(obs_module_text("SetupWizard.NewCollection"));
	newCollectionButton->setStyleSheet(EWizardButtonStyle);
	connect(newCollectionButton, &QPushButton::released, this,
		[this]() { emit newCollectionPressed(); });

	QPushButton* mergeCollectionButton = new QPushButton(this);
	mergeCollectionButton->setText(obs_module_text("SetupWizard.MergeCollection"));
	mergeCollectionButton->setStyleSheet(EWizardQuietButtonStyle);
	connect(mergeCollectionButton, &QPushButton::released, this,
		[this]() { emit mergeCollectionPressed(); });

	buttons->addStretch();
	buttons->addWidget(newCollectionButton);
	buttons->addWidget(mergeCollectionButton);
	buttons->addStretch();
	layout->addLayout(buttons);
	layout->addStretch();
}

NewCollectionName::NewCollectionName(std::string titleText, std::string subTitleText, std::vector<std::string> const& steps, int step, std::string name,
				     std::string thumbnailPath, QWidget* parent)
	: QWidget(parent)
{
	_existingCollections = GetSceneCollectionNames();
	// TODO- check _existingCollections to see if name is in it.
	std::string nameValue = name;
	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	auto main = new QHBoxLayout();
	main->setAlignment(Qt::AlignTop);
	main->setContentsMargins(0, 0, 0, 0);
	auto sideBar = new StepsSideBar(steps, name, thumbnailPath, this);
	sideBar->setContentsMargins(0, 0, 0, 0);
	sideBar->setFixedWidth(240);
	sideBar->setStep(step);
	auto form = new QVBoxLayout();
	auto title = new QLabel(titleText.c_str(), this);
	//auto title = new QLabel(obs_module_text("SetupWizard.CreateCollection.Title"), this);
	title->setStyleSheet(EWizardStepTitle);
	auto subTitle = new QLabel(subTitleText.c_str(), this);
	//auto subTitle = new QLabel(obs_module_text("SetupWizard.CreateCollection.SubTitle"), this);
	subTitle->setStyleSheet(EWizardStepSubTitle);
	auto image = new RoundedImageLabel(8, this);
	std::string imageBaseDir = GetDataPath();
	imageBaseDir += "/images/";
	std::string imgPath = imageBaseDir + "new-sc-example.png";
	QPixmap sampleImage(imgPath.c_str());
	image->setImage(sampleImage);
	image->setFixedWidth(364);
	image->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
	
	auto fieldLabel = new QLabel(obs_module_text("SetupWizard.CreateCollection.NewNameLabel"));
	fieldLabel->setStyleSheet(EWizardFieldLabel);
	_nameField = new QLineEdit(this);
	_nameField->setText(nameValue.c_str());
	_nameField->setStyleSheet(EWizardTextField);
	_nameField->setMaxLength(64);
	layout->addWidget(_nameField);
	connect(_nameField, &QLineEdit::textChanged,
		[this](const QString text) {
			_proceedButton->setEnabled(text.length() > 0);
		});
	form->addWidget(title);
	form->addWidget(subTitle);
	form->addWidget(image);
	form->addWidget(fieldLabel);
	form->addWidget(_nameField);
	form->addStretch();
	main->addWidget(sideBar);
	main->addLayout(form);

	auto buttons = new QHBoxLayout();
	buttons->setContentsMargins(0, 0, 0, 0);
	buttons->addStretch();

	QPushButton* backButton = new QPushButton(this);
	backButton->setText(obs_module_text("SetupWizard.BackButton"));
	backButton->setStyleSheet(EWizardQuietButtonStyle);
	connect(backButton, &QPushButton::released, this,
		[this]() { emit backPressed(); });

	_proceedButton = new QPushButton(this);
	_proceedButton->setText(obs_module_text("SetupWizard.NextButton"));
	_proceedButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	_proceedButton->setStyleSheet(EWizardButtonStyle);
	connect(_proceedButton, &QPushButton::released, this, [this]() {
	QString qName = _nameField->text();
	std::string name = qName.toUtf8().constData();
	if (std::find(_existingCollections.begin(),
			      _existingCollections.end(),
			      name) != _existingCollections.end()) {
			QMessageBox::warning(
				this, 
				obs_module_text("SetupWizard.NameInUseError.Title"),
				obs_module_text("SetupWizard.NameInUseError.Text"));
			return;
		}
		emit proceedPressed(name.c_str());
	});
	buttons->addWidget(backButton);
	buttons->addWidget(_proceedButton);

	layout->addLayout(main);
	layout->addStretch();
	layout->addLayout(buttons);
}

VideoSetup::VideoSetup(std::vector<std::string> const& steps,
			   int step,
			   std::string name,
		       std::string thumbnailPath,
		       std::map<std::string, std::string> videoSourceLabels,
	           QWidget* parent)
	: QWidget(parent)
{
	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	auto main = new QHBoxLayout();
	main->setAlignment(Qt::AlignTop);
	main->setContentsMargins(0, 0, 0, 0);
	auto sideBar = new StepsSideBar(steps, name, thumbnailPath, this);
	sideBar->setContentsMargins(0, 0, 0, 0);
	sideBar->setFixedWidth(240);
	sideBar->setStep(step);

	obs_data_t *config = elgatoCloud->GetConfig();

	std::string videoSettingsJson =
		obs_data_get_string(config, "DefaultVideoCaptureSettings");

	obs_data_t *videoData =
		videoSettingsJson != ""
			? obs_data_create_from_json(videoSettingsJson.c_str())
			: nullptr;

	auto form = new QVBoxLayout();
	if (videoSourceLabels.size() == 1) {
		auto title = new QLabel(obs_module_text("SetupWizard.VideoSetup.Title.Singular"), this);
		title->setStyleSheet(EWizardStepTitle);
		title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		auto subTitle = new QLabel(obs_module_text("SetupWizard.VideoSetup.SubTitle.Singular"), this);
		subTitle->setStyleSheet(EWizardStepSubTitle);
		subTitle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		form->addWidget(title);
		form->addWidget(subTitle);
		auto it = videoSourceLabels.begin();
		auto sourceName = it->first;
		auto label = it->second;

		auto vSource = new VideoCaptureSourceSelector(
			this, label, sourceName, videoData);
		form->addWidget(vSource);
		_videoSelectors.push_back(vSource);
	} else if (videoSourceLabels.size() > 1) {
		std::string titleText = obs_module_text("SetupWizard.VideoSetup.Title.Plural");
		titleText = std::to_string(videoSourceLabels.size()) + " " + titleText;
		auto title = new QLabel(titleText.c_str());
		title->setStyleSheet(EWizardStepTitle);
		auto subTitle = new QLabel(obs_module_text("SetupWizard.VideoSetup.SubTitle.Plural"), this);
		subTitle->setStyleSheet(EWizardStepSubTitle);
		form->addWidget(title);
		form->addWidget(subTitle);

		auto sourceWidget = new QWidget(this);
		sourceWidget->setSizePolicy(QSizePolicy::Expanding,
					    QSizePolicy::Preferred);
		sourceWidget->setContentsMargins(0, 0, 0, 0);
		auto sourceGrid = new QGridLayout(sourceWidget);
		sourceGrid->setContentsMargins(0, 0, 0, 0);
		sourceGrid->setSpacing(18);

		int i = 0;
		int j = 0;
		bool first = true;
		for (auto const &[sourceName, label] : videoSourceLabels) {
			auto vSource = new VideoCaptureSourceSelector(
				sourceWidget, label, sourceName,
				first ? videoData : nullptr);
			vSource->setFixedWidth(170);
			vSource->setFixedHeight(170);
			sourceGrid->addWidget(vSource, i, j);
			i += j % 2;
			j += 1;
			j %= 2;
			_videoSelectors.push_back(vSource);
			first = false;
		}

		auto scrollArea = new QScrollArea(this);
		scrollArea->setContentsMargins(0, 0, 0, 0);
		scrollArea->setWidget(sourceWidget);
		scrollArea->setSizePolicy(QSizePolicy::Expanding,
					  QSizePolicy::Expanding);
		scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		scrollArea->setStyleSheet("QScrollArea { border: none; }");
		form->addWidget(scrollArea);
	}

	form->addStretch();

	main->addWidget(sideBar);
	main->addLayout(form);


	QPushButton *backButton = new QPushButton(this);
	backButton->setText(obs_module_text("SetupWizard.BackButton"));
	backButton->setStyleSheet(EWizardQuietButtonStyle);

	QPushButton *proceedButton = new QPushButton(this);
	proceedButton->setText(obs_module_text("SetupWizard.NextButton"));
	proceedButton->setStyleSheet(EWizardButtonStyle);

	auto buttons = new QHBoxLayout();
	buttons->setContentsMargins(0, 0, 0, 0);
	buttons->addStretch();
	buttons->addWidget(backButton);
	buttons->addWidget(proceedButton);
	connect(proceedButton, &QPushButton::released, this, [this]() {
		std::map<std::string, std::string> res;
		int i = 0;
		for (auto const &source : this->_videoSelectors) {
			std::string settings = source->GetSettings();
			std::string sourceName = source->GetSourceName();
			res[sourceName] = settings;
		}
		emit proceedPressed(res);
	});
	connect(backButton, &QPushButton::released, this,
		[this]() { emit backPressed(); });

	layout->addLayout(main);
	layout->addStretch();
	layout->addLayout(buttons);
	obs_data_release(config);
	obs_data_release(videoData);
}

VideoSetup::~VideoSetup() {}

void VideoSetup::DefaultVideoUpdated(void *data, calldata_t *params)
{
	UNUSED_PARAMETER(params);
	UNUSED_PARAMETER(data);
}

void VideoSetup::DisableTempSources()
{
	for (auto const &source : this->_videoSelectors) {
		source->DisableTempSource();
	}
}

void VideoSetup::EnableTempSources()
{
	for (auto const &source : this->_videoSelectors) {
		source->EnableTempSource();
	}
}

void VideoSetup::OpenConfigVideoSource()
{
	if (!_videoCaptureSource) {
		return;
	}
	obs_frontend_open_source_properties(_videoCaptureSource);

	obs_properties_t *props = obs_source_properties(_videoCaptureSource);
	obs_property_t *devices = obs_properties_get(props, "video_device_id");
	for (size_t i = 0; i < obs_property_list_item_count(devices); i++) {
		std::string name = obs_property_list_item_name(devices, i);
		std::string id = obs_property_list_item_string(devices, i);
	}
	obs_properties_destroy(props);
}

AudioSetup::AudioSetup(std::vector<std::string> const& steps, int step, std::string name,
		       std::string thumbnailPath, QWidget* parent)
	: QWidget(parent)
{
	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	auto main = new QHBoxLayout();
	main->setAlignment(Qt::AlignTop);
	main->setContentsMargins(0, 0, 0, 0);
	auto sideBar = new StepsSideBar(steps, name, thumbnailPath, this);
	sideBar->setContentsMargins(0, 0, 0, 0);
	sideBar->setFixedWidth(240);
	sideBar->setStep(step);

	obs_data_t* config = elgatoCloud->GetConfig();

	auto form = new QVBoxLayout();

	QLabel *audioDeviceLabel = new QLabel(this);
	audioDeviceLabel->setText(obs_module_text("SetupWizard.AudioSetup.Device.Text"));
	_audioSources = new QComboBox(this);
	_audioSources->setStyleSheet(EWizardComboBoxStyle);

	_levelsWidget = new SimpleVolumeMeter(this, _volmeter);
	// Add Dropdown and meter

	form->addWidget(audioDeviceLabel);
	form->addWidget(_audioSources);
	form->addWidget(_levelsWidget);
	form->addStretch();

	main->addWidget(sideBar);
	main->addLayout(form);

	std::string audioSettingsJson =
		obs_data_get_string(config, "DefaultAudioCaptureSettings");

	obs_data_t *audioSettings =
		audioSettingsJson != ""
			? obs_data_create_from_json(audioSettingsJson.c_str())
			: nullptr;

	_audioCaptureSource = nullptr;
	_setupTempSources(audioSettings);
	SetupVolMeter();

	obs_data_release(audioSettings);

	connect(_audioSources, &QComboBox::currentIndexChanged, this, [this](int index) {
		obs_data_t *aSettings =
			obs_source_get_settings(_audioCaptureSource);
		std::string id = _audioSourceIds[index];
		obs_data_set_string(aSettings, "device_id", id.c_str());

		if (_volmeter) {
			obs_volmeter_remove_callback(
				_volmeter, AudioSetup::OBSVolumeLevel, this);
			obs_volmeter_destroy(_volmeter);
			_volmeter = nullptr;
		}

		// For some reason, we need to completely deconstruct the temporary
		// audio capture source in order to connect the new device vol meter
		// to the volume meter in the UI. It should be possible to inject
		// the newly selected device, and re-connect the _volmeter callbacks
		_setupTempSources(aSettings);
		obs_data_release(aSettings);
		SetupVolMeter();
	});

	auto buttons = new QHBoxLayout();

	QPushButton *backButton = new QPushButton(this);
	backButton->setText(obs_module_text("SetupWizard.BackButton"));
	backButton->setStyleSheet(EWizardQuietButtonStyle);

	QPushButton *proceedButton = new QPushButton(this);
	proceedButton->setText(obs_module_text("SetupWizard.InstallButton"));
	proceedButton->setStyleSheet(EWizardButtonStyle);

	buttons->addStretch();
	buttons->addWidget(backButton);
	buttons->addWidget(proceedButton);
	connect(proceedButton, &QPushButton::released, this, [this]() {
		obs_data_t *aSettings =
			obs_source_get_settings(_audioCaptureSource);
		std::string aJson = obs_data_get_json(aSettings);
		obs_data_release(aSettings);
		emit proceedPressed(aJson);
	});
	connect(backButton, &QPushButton::released, this,
		[this]() { emit backPressed(); });

	layout->addLayout(main);
	layout->addStretch();
	layout->addLayout(buttons);
	obs_data_release(config);
}

void AudioSetup::_setupTempSources(obs_data_t *audioSettings)
{
	if (_audioCaptureSource) {
		obs_source_release(_audioCaptureSource);
		obs_source_remove(_audioCaptureSource);
	}

	const char *audioSourceId = "wasapi_input_capture";
	const char *aId = obs_get_latest_input_type_id(audioSourceId);
	_audioCaptureSource = obs_source_create_private(
		aId, "elgato-cloud-audio-config", audioSettings);

	obs_properties_t *aProps = obs_source_properties(_audioCaptureSource);
	obs_property_t *aDevices = obs_properties_get(aProps, "device_id");
	if (_audioSourceIds.size() == 0) {
		for (size_t i = 0; i < obs_property_list_item_count(aDevices);
		     i++) {
			std::string name =
				obs_property_list_item_name(aDevices, i);
			std::string id =
				obs_property_list_item_string(aDevices, i);
			_audioSourceIds.push_back(id);
			_audioSources->addItem(name.c_str());
		}
	}
	obs_properties_destroy(aProps);

	obs_data_t *aSettings = obs_source_get_settings(_audioCaptureSource);
	std::string aDevice = obs_data_get_string(aSettings, "device_id");
	if (aDevice != "") {
		auto it = std::find(_audioSourceIds.begin(),
				    _audioSourceIds.end(), aDevice);
		if (it != _audioSourceIds.end()) {
			_audioSources->setCurrentIndex(
				static_cast<int>(it - _audioSourceIds.begin()));
		}
	}
	obs_data_release(aSettings);

	_levelsWidget->show();
}

void AudioSetup::SetupVolMeter()
{
	_volmeter = obs_volmeter_create(OBS_FADER_LOG);
	obs_volmeter_attach_source(_volmeter, _audioCaptureSource);
	obs_volmeter_add_callback(_volmeter, AudioSetup::OBSVolumeLevel, this);
}

void AudioSetup::DefaultAudioUpdated(void *data, calldata_t *params)
{
	UNUSED_PARAMETER(params);
	auto config = static_cast<AudioSetup *>(data);
	config->SetupVolMeter();
}

void AudioSetup::OpenConfigAudioSource()
{
	if (!_audioCaptureSource) {
		return;
	}
	obs_frontend_open_source_properties(_audioCaptureSource);
	obs_properties_t *props = obs_source_properties(_audioCaptureSource);
	obs_property_t *devices = obs_properties_get(props, "device_id");
	for (size_t i = 0; i < obs_property_list_item_count(devices); i++) {
		std::string name = obs_property_list_item_name(devices, i);
		std::string id = obs_property_list_item_string(devices, i);
	}
	obs_properties_destroy(props);
}

void AudioSetup::OBSVolumeLevel(void *data,
				const float magnitude[MAX_AUDIO_CHANNELS],
				const float peak[MAX_AUDIO_CHANNELS],
				const float inputPeak[MAX_AUDIO_CHANNELS])
{
	UNUSED_PARAMETER(peak);
	UNUSED_PARAMETER(inputPeak);
	auto config = static_cast<AudioSetup *>(data);
	float mag = magnitude[0];
	float pk = peak[0];
	float ip = inputPeak[0];
	config->_levelsWidget->setLevel(mag, pk, ip);
	QMetaObject::invokeMethod(QCoreApplication::instance()->thread(),
				  [config]() {
					  if (!setupWizard) {
						  return;
					  }
					  config->_levelsWidget->update();
				  });
}

AudioSetup::~AudioSetup()
{
	if (_volmeter) {
		obs_volmeter_detach_source(_volmeter);
		obs_volmeter_remove_callback(_volmeter,
					     AudioSetup::OBSVolumeLevel, this);
		obs_volmeter_destroy(_volmeter);
	}
	if (_audioCaptureSource) {
		obs_source_release(_audioCaptureSource);
		obs_source_remove(_audioCaptureSource);
		//signal_handler_t *audioSigHandler =
		//	obs_source_get_signal_handler(_audioCaptureSource);
		//signal_handler_disconnect(audioSigHandler, "update",
		//			  AudioSetup::DefaultAudioUpdated,
		//			  this);
	}
}

MergeSelectScenes::MergeSelectScenes(std::vector<OutputScene>& outputScenes, std::vector<std::string> const& steps, int step, std::string name,
	std::string thumbnailPath, QWidget* parent)
	: QWidget(parent), _outputScenes(outputScenes)
{
	std::string imageBaseDir = GetDataPath();
	imageBaseDir += "/images/";
	std::string checkedImage = imageBaseDir + "checkbox_checked.png";
	std::string uncheckedImage = imageBaseDir + "checkbox_unchecked.png";
	QString checklistStyle = EWizardChecklistStyle;
	checklistStyle.replace("${checked-img}", checkedImage.c_str());
	checklistStyle.replace("${unchecked-img}", uncheckedImage.c_str());

	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	auto main = new QHBoxLayout();
	main->setAlignment(Qt::AlignTop);
	main->setContentsMargins(0, 0, 0, 0);
	auto sideBar = new StepsSideBar(steps, name, thumbnailPath, this);
	sideBar->setContentsMargins(0, 0, 0, 0);
	sideBar->setFixedWidth(240);
	sideBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
	sideBar->setStep(step);

	auto form = new QVBoxLayout();
	auto title = new QLabel(obs_module_text("SetupWizard.MergeSelectScenes.Title"), this);
	title->setStyleSheet(EWizardStepTitle);
	auto subTitle = new QLabel(obs_module_text("SetupWizard.MergeSelectScenes.SubTitle"), this);
	subTitle->setStyleSheet(EWizardStepSubTitle);
	subTitle->setWordWrap(true);
	form->addWidget(title);
	form->addWidget(subTitle);

	auto allScenes = new QCheckBox(
		obs_module_text("SetupWizard.MergeSelectScenes.AllScenes"), this);
	allScenes->setChecked(true);

	QString checkBoxStyle = EWizardCheckBoxStyle;
	checkBoxStyle.replace("${checked-img}", checkedImage.c_str());
	checkBoxStyle.replace("${unchecked-img}", uncheckedImage.c_str());
	allScenes->setStyleSheet(checkBoxStyle);
	allScenes->setToolTip(obs_module_text("SetupWizard.MergeSelectScenes.AllScenes.Tooltip"));
	form->addWidget(allScenes);

	if (_outputScenes.size() > 0) {
		auto sceneList = new QListWidget(this);
		sceneList->setSizePolicy(QSizePolicy::Preferred,
			QSizePolicy::Expanding);
		sceneList->setSpacing(8);
		sceneList->setStyleSheet(checklistStyle);
		sceneList->setSelectionMode(QAbstractItemView::NoSelection);
		sceneList->setFocusPolicy(Qt::FocusPolicy::NoFocus);
		sceneList->setDisabled(true);

		for (auto scene : _outputScenes) {
			sceneList->addItem(scene.name.c_str());
		}

		QListWidgetItem* item = nullptr;
		for (int i = 0; i < sceneList->count(); ++i) {
			item = sceneList->item(i);
			item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
			item->setCheckState(_outputScenes[i].enabled ? Qt::Checked : Qt::Unchecked);
		}

		connect(allScenes, &QCheckBox::stateChanged, [sceneList](int state) {
			bool all = state == Qt::Checked;
			sceneList->setDisabled(state);
			});

		connect(sceneList, &QListWidget::itemChanged, this,
			[this, sceneList](QListWidgetItem* item) {
				int index = sceneList->row(item);
				bool outputScene = item->checkState() == Qt::Checked;
				_outputScenes[index].enabled = outputScene;
			});
		form->addWidget(sceneList);
	} else {
		allScenes->setDisabled(true);
		auto subTitle = new QLabel(obs_module_text("SetupWizard.MergeSelectScenes.NoCustomMergeAvailable"), this);
		subTitle->setStyleSheet(EWizardStepSubTitle);
		subTitle->setFixedWidth(320);
		subTitle->setAlignment(Qt::AlignCenter);
		subTitle->setWordWrap(true);

		auto subTitleLayout = centeredWidgetLayout(subTitle);
		form->addStretch();
		form->addLayout(subTitleLayout);
		form->addStretch();
	}

	main->addWidget(sideBar);
	main->addLayout(form);

	auto buttons = new QHBoxLayout();

	QPushButton* backButton = new QPushButton(this);
	backButton->setText(obs_module_text("SetupWizard.BackButton"));
	backButton->setStyleSheet(EWizardQuietButtonStyle);

	QPushButton* proceedButton = new QPushButton(this);
	proceedButton->setText(obs_module_text("SetupWizard.NextButton"));
	proceedButton->setStyleSheet(EWizardButtonStyle);

	buttons->addStretch();
	buttons->addWidget(backButton);
	buttons->addWidget(proceedButton);
	connect(proceedButton, &QPushButton::released, this, [this]() {
		emit proceedPressed();
		});
	connect(backButton, &QPushButton::released, this,
		[this]() { emit backPressed(); });

	layout->addLayout(main);
	layout->addStretch();
	layout->addLayout(buttons);
}

MergeSelectScenes::~MergeSelectScenes()
{

}

std::vector<std::string> MergeSelectScenes::getSelectedScenes()
{
	std::vector<std::string> scenes;
	for (auto const& scene : _outputScenes) {
		if (scene.enabled) {
			scenes.push_back(scene.name);
		}
	}
	return scenes;
}


Loading::Loading(QWidget *parent) : QWidget(parent)
{
	QVBoxLayout *layout = new QVBoxLayout(this);

	layout->addStretch();

	auto spinner = new SmallSpinner(this);
	auto spinnerLayout = centeredWidgetLayout(spinner);
	layout->addLayout(spinnerLayout);

	auto title = new QLabel(this);
	title->setText(obs_module_text("SetupWizard.Loading.Title"));
	title->setStyleSheet(EBlankSlateTitleStyle);
	title->setAlignment(Qt::AlignCenter);
	title->setFixedWidth(360);
	auto titleLayout = centeredWidgetLayout(title);
	layout->addLayout(titleLayout);

	auto subTitle = new QLabel(this);
	subTitle->setText(
		obs_module_text("SetupWizard.Loading.Text"));
	subTitle->setStyleSheet(EBlankSlateSubTitleStyle);
	subTitle->setAlignment(Qt::AlignCenter);
	subTitle->setWordWrap(true);
	subTitle->setFixedWidth(360);
	auto subTitleLayout = centeredWidgetLayout(subTitle);
	layout->addLayout(subTitleLayout);

	layout->addStretch();
	layout->setSpacing(16);
}

StreamPackageSetupWizard::StreamPackageSetupWizard(QWidget *parent,
						   ElgatoProduct *product,
						   std::string filename,
						   bool deleteOnClose)
	: QDialog(parent),
	  _thumbnailPath(product->thumbnailPath),
	  _productName(product->name),
	  _filename(filename),
	  _deleteOnClose(deleteOnClose),
	  _installStarted(false)
{
	_curCollectionFileName =
		get_current_scene_collection_filename();
	_curCollectionFileName =
		get_scene_collections_path() + _curCollectionFileName;

	setModal(true);
	_buildBaseUI();
}

StreamPackageSetupWizard::~StreamPackageSetupWizard()
{
	if (!_installStarted) { // We've not yet handed control over
		                    // too install routine.
		if (_deleteOnClose) {
			// Delete the scene collection file
			os_unlink(_filename.c_str());
		}
		obs_enum_sources(
			&StreamPackageSetupWizard::
			EnableVideoCaptureSourcesActive,
			this);
	}
	setupWizard = nullptr;
}

void StreamPackageSetupWizard::OpenArchive()
{
	_future =
		QtConcurrent::run(GetBundleInfo, _filename)
			.then([this](std::string bundleInfoStr) {
				// We are now in a different thread, so we need to execute this
				// back in the gui thread.  See, threading hell.
				QMetaObject::invokeMethod(
					QCoreApplication::instance()
						->thread(), // main GUI thread
					[this, bundleInfoStr]() {
						nlohmann::json bundleInfo;
						bool error = false;
						try {
							bundleInfo = nlohmann::
								json::parse(
									bundleInfoStr);
						} catch (
							const nlohmann::json::
								parse_error &e) {
							obs_log(LOG_ERROR,
								"Parsing Error.\n  message: %s\n  id: %i",
								e.what(), e.id);
							error = true;
						}
						if (error ||
						    bundleInfo.contains(
							    "Error")) {
							obs_log(LOG_ERROR,
								"Invalid file.");
							int ret = QMessageBox::warning(
								this,
								obs_module_text("SetupWizard.IncompatibleFile.Title"),
								obs_module_text("SetupWizard.IncompatibleFile.Text"),
								QMessageBox::Ok);
							close();
							return;
						}

						std::map<std::string,
							 std::string>
							videoSourceLabels = bundleInfo
								["video_devices"];
						std::vector<std::string>
							requiredPlugins = bundleInfo
								["plugins_required"];
						std::vector<OutputScene> outputScenes = {};
						if (bundleInfo.contains("output_scenes")) {
							for (auto const& outputScene : bundleInfo["output_scenes"]) {
								if (outputScene.contains("name") && outputScene.contains("id")) {
									outputScenes.push_back({
										outputScene["id"],
										outputScene["name"],
										true
									});
								}
							}
						}

						// Disable all video capture sources so that single-thread
						// capture sources, such as the Elgato Facecam, can be properly
						// selected in the wizard.  Will re-enable any disabled sources
						// in the wizard destructor.
						obs_enum_sources(
							&StreamPackageSetupWizard::
								DisableVideoCaptureSources,
							this);

						PluginInfo pi;
						std::vector<PluginDetails>
							missing = pi.missing(
								requiredPlugins);
						if (missing.size() > 0) {
							_buildMissingPluginsUI(
								missing);
						} else {
							_buildSetupUI(
								videoSourceLabels, outputScenes);
						}
					});
			})
			.onCanceled([]() {

			});
}

bool StreamPackageSetupWizard::DisableVideoCaptureSources(void *data,
							  obs_source_t *source)
{
	auto wizard = static_cast<StreamPackageSetupWizard *>(data);
	std::string sourceType = obs_source_get_id(source);
	if (sourceType == "dshow_input") {
		auto settings = obs_source_get_settings(source);
		bool active = obs_data_get_bool(settings, "active");
		obs_data_release(settings);
		if (active) {
			calldata_t cd = {};
			calldata_set_bool(&cd, "active", false);
			proc_handler_t *ph =
				obs_source_get_proc_handler(source);
			proc_handler_call(ph, "activate", &cd);
			calldata_free(&cd);
			std::string id = obs_source_get_uuid(source);
			wizard->_toEnable.push_back(id);
		}
	}
	return true;
}

bool StreamPackageSetupWizard::EnableVideoCaptureSourcesActive(void* data,
							   obs_source_t* source)
{
	auto wizard = static_cast<StreamPackageSetupWizard*>(data);
	std::string sourceType = obs_source_get_id(source);
	if (sourceType == "dshow_input") {
		std::string id = obs_source_get_uuid(source);
		if (std::find(wizard->_toEnable.begin(), wizard->_toEnable.end(), id) != wizard->_toEnable.end()) {
			auto settings = obs_source_get_settings(source);
			bool active = obs_data_get_bool(settings, "active");
			obs_data_release(settings);
			if (!active) {
				calldata_t cd = {};
				calldata_set_bool(&cd, "active", true);
				proc_handler_t* ph =
					obs_source_get_proc_handler(source);
				proc_handler_call(ph, "activate", &cd);
				calldata_free(&cd);
			}
		}
	}
	return true;
}

void StreamPackageSetupWizard::_buildMissingPluginsUI(
	std::vector<PluginDetails> &missing)
{
	setFixedSize(500, 350);
	//QVBoxLayout *layout = new QVBoxLayout(this);

	auto missingPlugins =
		new MissingPlugins(this, _productName, _thumbnailPath, missing);
	_container->addWidget(missingPlugins);
	_container->setCurrentIndex(1);
}

void StreamPackageSetupWizard::_buildBaseUI()
{
	setFixedSize(640, 448);
	QVBoxLayout *layout = new QVBoxLayout(this);
	_container = new QStackedWidget(this);

	auto loading = new Loading(this);
	_container->addWidget(loading);

	setupWizard = this;
	setWindowTitle(obs_module_text("SetupWizard.WindowTitle"));
	setStyleSheet("background-color: #151515;");

	layout->addWidget(_container);
}

void StreamPackageSetupWizard::_buildSetupUI(
	std::map<std::string, std::string> &videoSourceLabels, std::vector<OutputScene>& outputScenes)
{
	setFixedSize(640, 448);

	StartInstall* startInstall =
		new StartInstall(this, _productName, _thumbnailPath);
	connect(startInstall, &StartInstall::newCollectionPressed, this,
		[this]() {
			_setup.installType = InstallTypes::NewCollection;
			_container->setCurrentIndex(2);
		});
	connect(startInstall, &StartInstall::mergeCollectionPressed, this,
		[this]() {
			_setup.installType = InstallTypes::AddToCollection;
			_container->setCurrentIndex(3);
		});
	_container->addWidget(startInstall);

	_buildNewCollectionUI(videoSourceLabels);
	_buildMergeCollectionUI(videoSourceLabels, outputScenes);

	_container->setCurrentIndex(1);
}

void StreamPackageSetupWizard::_buildNewCollectionUI(std::map<std::string, std::string>& videoSourceLabels)
{
	std::vector<std::string> steps = {
		obs_module_text("SetupWizard.NewCollectionSteps.GetStarted"),
		obs_module_text("SetupWizard.NewCollectionSteps.NameSceneCollection"),
		obs_module_text("SetupWizard.NewCollectionSteps.SetUpCameras"),
		obs_module_text("SetupWizard.NewCollectionSteps.ChooseMicrophone")
	};
	
	_newCollectionSteps = new QStackedWidget(this);
	// Step 1- Provide a name for the new collection (step index: 0)
	auto *newName =
		new NewCollectionName(
			obs_module_text("SetupWizard.CreateCollection.Title"),
			obs_module_text("SetupWizard.CreateCollection.SubTitle"),
			steps, 1, _productName, _thumbnailPath, this);

	connect(newName, &NewCollectionName::proceedPressed, this,
		[this, videoSourceLabels](std::string name) {
			_setup.collectionName = name;
			if (videoSourceLabels.size() > 0) {
				_newCollectionSteps->setCurrentIndex(1);
				_vSetup->EnableTempSources();
			} else {
				_newCollectionSteps->setCurrentIndex(2);
			}
		});
	connect(newName, &NewCollectionName::backPressed, this, [this]() {
			_container->setCurrentIndex(1);
		});
	_newCollectionSteps->addWidget(newName);

	// Step 2- Set up Video inputs (step index: 1)
	_vSetup = new VideoSetup(steps, 2, _productName, _thumbnailPath,
				 videoSourceLabels, this);
	_vSetup->DisableTempSources();
	_newCollectionSteps->addWidget(_vSetup);
	connect(_vSetup, &VideoSetup::proceedPressed, this,
		[this](std::map<std::string, std::string> settings) {
			_setup.videoSettings = settings;
			_newCollectionSteps->setCurrentIndex(2);
			_vSetup->DisableTempSources();
		});
	connect(_vSetup, &VideoSetup::backPressed, this, [this]() {
		_newCollectionSteps->setCurrentIndex(0);
		_vSetup->DisableTempSources();
	});

	// Step 3- Setup Audio Inputs (step index: 2)
	auto aSetup = new AudioSetup(steps, 3, _productName, _thumbnailPath, this);
	_newCollectionSteps->addWidget(aSetup);
	connect(aSetup, &AudioSetup::proceedPressed, this,
		[this](std::string settings) {
			_setup.audioSettings = settings;
			// Nuke the video preview window
			_installStarted = true;
			installStreamPackage(_setup, _filename, _deleteOnClose, _toEnable);
		});
	connect(aSetup, &AudioSetup::backPressed, this,
		[this, videoSourceLabels]() {
			if (videoSourceLabels.size() > 0) {
				_newCollectionSteps->setCurrentIndex(1);
				_vSetup->EnableTempSources();
			} else {
				_newCollectionSteps->setCurrentIndex(0);
			}
		});
	_newCollectionSteps->setCurrentIndex(0);
	_container->addWidget(_newCollectionSteps);
}

void StreamPackageSetupWizard::_buildMergeCollectionUI(std::map<std::string, std::string>& videoSourceLabels, std::vector<OutputScene>& outputScenes)
{
	_mergeCollectionSteps = new QStackedWidget(this);

	PluginInfo pi;
	std::vector<PluginDetails>
		missing = pi.missing({ "source-clone.dll" });
	if (missing.size() > 0) {
		auto missingSourceClone = new MissingSourceClone(_productName, _thumbnailPath, this);
		connect(missingSourceClone, &MissingSourceClone::backPressed, this, [this]() {
			_container->setCurrentIndex(1);
		});

		_mergeCollectionSteps->addWidget(missingSourceClone);
		_mergeCollectionSteps->setCurrentIndex(0);
		_container->addWidget(_mergeCollectionSteps);
		return;
	}

	std::vector<std::string> steps = {
		obs_module_text("SetupWizard.MergeCollectionSteps.GetStarted"),
		obs_module_text("SetupWizard.NewCollectionSteps.NameSceneCollection"),
		obs_module_text("SetupWizard.MergeCollectionSteps.SelectScenes"),
		obs_module_text("SetupWizard.MergeCollectionSteps.SetUpCameras"),
		obs_module_text("SetupWizard.MergeCollectionSteps.ChooseMicrophone")
	};

	// Step 1- Provide a name for the new collection (step index: 0)
	auto* newName =
		new NewCollectionName(
			obs_module_text("SetupWizard.MergeCollectionName.Title"),
			obs_module_text("SetupWizard.MergeCollectionName.SubTitle"),
			steps, 1, _productName, _thumbnailPath, this);

	connect(newName, &NewCollectionName::proceedPressed, this,
		[this, videoSourceLabels](std::string name) {
			_setup.collectionName = name;
			_mergeCollectionSteps->setCurrentIndex(1);
			_vSetup->EnableTempSources();
		});
	connect(newName, &NewCollectionName::backPressed, this, [this]() {
		_container->setCurrentIndex(1);
		});
	_mergeCollectionSteps->addWidget(newName);

	// Step 1- select scenes to merge (step index: 1)
	auto mergeScenes = new MergeSelectScenes(outputScenes, steps, 2, _productName, _thumbnailPath, this);
	_mergeCollectionSteps->addWidget(mergeScenes);
	connect(mergeScenes, &MergeSelectScenes::proceedPressed, this,
		[this, mergeScenes, videoSourceLabels]() {
			if (videoSourceLabels.size() > 0) {
				_vSetupMerge->EnableTempSources();
				_mergeCollectionSteps->setCurrentIndex(2);
			} else {
				_mergeCollectionSteps->setCurrentIndex(3);
			}
			_setup.scenesToMerge = mergeScenes->getSelectedScenes();
		});

	connect(mergeScenes, &MergeSelectScenes::backPressed, this,
		[this]() {
			_mergeCollectionSteps->setCurrentIndex(0);
		});

	// Step 2- Set up Video inputs (step index: 2)
	_vSetupMerge = new VideoSetup(steps, 3, _productName, _thumbnailPath,
		videoSourceLabels, this);
	_vSetupMerge->DisableTempSources();
	_mergeCollectionSteps->addWidget(_vSetupMerge);
	connect(_vSetupMerge, &VideoSetup::proceedPressed, this,
		[this](std::map<std::string, std::string> settings) {
			_setup.videoSettings = settings;
			_mergeCollectionSteps->setCurrentIndex(3);
			_vSetupMerge->DisableTempSources();
		});
	connect(_vSetupMerge, &VideoSetup::backPressed, this, [this]() {
		//_container->setCurrentIndex(0);
		_mergeCollectionSteps->setCurrentIndex(1);
		_vSetupMerge->DisableTempSources();
		});

	// Step 2- Setup Audio Inputs (step index: 3)
	auto aSetup = new AudioSetup(steps, 4, _productName, _thumbnailPath, this);
	_mergeCollectionSteps->addWidget(aSetup);
	connect(aSetup, &AudioSetup::proceedPressed, this,
		[this](std::string settings) {
			_setup.audioSettings = settings;
			// Nuke the video preview window
			_installStarted = true;
			mergeStreamPackage(_setup, _filename, _deleteOnClose, _toEnable);
		});
	connect(aSetup, &AudioSetup::backPressed, this,
		[this, videoSourceLabels]() {
			if (videoSourceLabels.size() > 0) {
				_mergeCollectionSteps->setCurrentIndex(2);
				_vSetupMerge->EnableTempSources();
			} else {
				_mergeCollectionSteps->setCurrentIndex(1);
			}
		});
	_mergeCollectionSteps->setCurrentIndex(0);
	_container->addWidget(_mergeCollectionSteps);
}

void mergeStreamPackage(Setup setup, std::string filename, bool deleteOnClose, std::vector<std::string> toEnable)
{
	// TODO: Clean up this mess of setting up the pack install path.
	obs_data_t* config = elgatoCloud->GetConfig();
	auto curFileName = get_current_scene_collection_filename();
	curFileName = get_scene_collections_path() + curFileName;

	std::string path = obs_data_get_string(config, "InstallLocation");
	os_mkdirs(path.c_str());
	std::string unsafeDirName(setup.collectionName);
	std::string safeDirName;
	generate_safe_path(unsafeDirName, safeDirName);
	std::string bundlePath = path + "/" + safeDirName;

	// TODO: Add dialog with progress indicator.  Put unzipping and
	//       scene collection loading code into new threads to stop
	//       from blocking.
	SceneBundle bundle;
	if (!bundle.FromElgatoCloudFile(filename, bundlePath)) {
		obs_log(LOG_WARNING, "Elgato Install: Could not install %s",
			filename.c_str());
		return;
	}
	obs_data_release(config);
	std::string id = "";
	if (deleteOnClose) {
		auto api = MarketplaceApi::getInstance();
		id = api->id();
	}

	bool collectionChanged = bundle.MergeCollection(
		setup.collectionName,
		setup.scenesToMerge,
		setup.videoSettings,
		setup.audioSettings,
		id);

	if (deleteOnClose) {
		// Delete the scene collection file
		os_unlink(filename.c_str());
	}

	// Handle resetting sources.
	if (!collectionChanged) {
		obs_enum_sources(
			&EnableVideoCaptureSourcesActive,
			&toEnable);
	} else {
		EnableVideoCaptureSourcesJson(toEnable, curFileName);
	}
}

void installStreamPackage(Setup setup, std::string filename, bool deleteOnClose, std::vector<std::string> toEnable)
{
	// TODO: Clean up this mess of setting up the pack install path.
	obs_data_t *config = elgatoCloud->GetConfig();
	auto curFileName = get_current_scene_collection_filename();
	curFileName = get_scene_collections_path() + curFileName;

	std::string path = obs_data_get_string(config, "InstallLocation");
	os_mkdirs(path.c_str());
	std::string unsafeDirName(setup.collectionName);
	std::string safeDirName;
	generate_safe_path(unsafeDirName, safeDirName);
	std::string bundlePath = path + "/" + safeDirName;

	// TODO: Add dialog with progress indicator.  Put unzipping and
	//       scene collection loading code into new threads to stop
	//       from blocking.
	SceneBundle bundle;
	if (!bundle.FromElgatoCloudFile(filename, bundlePath)) {
		obs_log(LOG_WARNING, "Elgato Install: Could not install %s",
			filename.c_str());
		return;
	}
	obs_data_release(config);
	std::string id = "";
	if (deleteOnClose) {
		auto api = MarketplaceApi::getInstance();
		id = api->id();
	}

	bool collectionChanged = bundle.ToCollection(
			    setup.collectionName,
				setup.videoSettings,
			    setup.audioSettings,
				id);

	if (deleteOnClose) {
		// Delete the scene collection file
		os_unlink(filename.c_str());
	}

	// Handle resetting sources.
	if (!collectionChanged) {
		obs_enum_sources(
			&EnableVideoCaptureSourcesActive,
			&toEnable);
	} else {
		EnableVideoCaptureSourcesJson(toEnable, curFileName);
	}
}

bool EnableVideoCaptureSourcesActive(void* data, obs_source_t* source)
{
	const std::vector<std::string> &toEnable = *static_cast<std::vector<std::string>*>(data);
	std::string sourceType = obs_source_get_id(source);
	if (sourceType == "dshow_input") {
		std::string id = obs_source_get_uuid(source);
		if (std::find(toEnable.begin(), toEnable.end(), id) != toEnable.end()) {
			auto settings = obs_source_get_settings(source);
			bool active = obs_data_get_bool(settings, "active");
			obs_data_release(settings);
			if (!active) {
				calldata_t cd = {};
				calldata_set_bool(&cd, "active", true);
				proc_handler_t* ph =
					obs_source_get_proc_handler(source);
				proc_handler_call(ph, "activate", &cd);
				calldata_free(&cd);
			}
		}
	}
	return true;
}

void EnableVideoCaptureSourcesJson(std::vector<std::string> sourceIds, std::string curFileName)
{
	char* absPath = os_get_abs_path_ptr(curFileName.c_str());
	std::string path = absPath;
	bfree(absPath);

	char* collectionStr =
		os_quick_read_utf8_file(path.c_str());
	std::string collectionData = collectionStr;
	bfree(collectionStr);

	try {
		nlohmann::json collectionJson = nlohmann::json::parse(collectionData);
		if (collectionJson.contains("sources")) {
			auto &sources = collectionJson["sources"];
			for (auto &it : sources) {
				if (it.contains("uuid") && std::find(sourceIds.begin(), sourceIds.end(), it["uuid"]) != sourceIds.end()) {
					it["settings"]["active"] = true;
				}
			}
		}
		std::string activatedCollection = collectionJson.dump();
		obs_data_t* data =
			obs_data_create_from_json(activatedCollection.c_str());
		bool success = obs_data_save_json_safe(
			data, path.c_str(), "tmp", "bak");
		obs_data_release(data);
	} catch (...) {
		obs_log(LOG_ERROR, "Could not re-activate video sources in previous scene collection.");
	}
}

} // namespace elgatocloud
