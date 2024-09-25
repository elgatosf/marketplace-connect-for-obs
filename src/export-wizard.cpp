#include <algorithm>

#include "export-wizard.hpp"
#include <QMainWindow>
#include <QVBoxLayout>
#include <QPushButton>
#include <QListWidget>
#include <QLineEdit>

namespace elgatocloud {

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
	"coreaudio-encoder.dll",
	// This plugin
	"elgato-deeplinking.dll"};

FileCollectionCheck::FileCollectionCheck(QWidget *parent,
					 std::vector<std::string> files)
	: QWidget(parent),
	  _files(files)
{
	QVBoxLayout *layout = new QVBoxLayout(this);

	auto title = new QLabel(this);
	title->setText("Media File Check");
	title->setStyleSheet("QLabel{ font-size: 18pt; font-weight: bold;}");
	layout->addWidget(title);

	std::string titleText =
		std::to_string(_files.size()) +
		" media files were found to bundle. If this is correct, click 'Next' below.";
	auto subTitle = new QLabel(this);
	subTitle->setText(titleText.c_str());
	subTitle->setStyleSheet(
		"QLabel {font-size: 11pt; font-style: italic;}");
	subTitle->setWordWrap(true);
	layout->addWidget(subTitle);

	auto fileList = new QListWidget(this);
	for (auto fileName : _files) {
		fileList->addItem(fileName.c_str());
	}

	layout->addWidget(fileList);

	QHBoxLayout *buttons = new QHBoxLayout(this);
	QPushButton *cancelButton = new QPushButton(this);
	cancelButton->setText("Cancel");
	connect(cancelButton, &QPushButton::released, this,
		[this]() { emit cancelPressed(); });

	QPushButton *continueButton = new QPushButton(this);
	continueButton->setText("Next");
	connect(continueButton, &QPushButton::released, this,
		[this]() { emit continuePressed(); });
	buttons->addWidget(cancelButton);
	buttons->addWidget(continueButton);

	layout->addLayout(buttons);
}

VideoSourceLabels::VideoSourceLabels(QWidget *parent,
				     std::map<std::string, std::string> devices)
	: QWidget(parent)
{
	QVBoxLayout *layout = new QVBoxLayout(this);

	std::string titleText = "Video Source Labels";
	auto title = new QLabel(this);
	title->setStyleSheet("QLabel{ font-size: 18pt; font-weight: bold;}");
	title->setText(titleText.c_str());
	layout->addWidget(title);

	auto description = new QLabel(this);
	description->setText(
		"Please add a description for each video capture source in your scene collection. These descriptions will help the user set up their video capture devices when they install the bundle.");
	description->setStyleSheet(
		"QLabel {font-size: 11pt; font-style: italic;}");
	description->setWordWrap(true);
	layout->addWidget(description);

	// Declare continue button so we can enable/disable it
	// on keypress in label list.
	auto continueButton = new QPushButton(this);

	if (devices.size() > 0) {
		auto formGrid = new QGridLayout(this);
		formGrid->setColumnMinimumWidth(0, 200);
		auto nameHeader = new QLabel("Source Name", this);
		nameHeader->setStyleSheet(
			"QLabel { font-size: 14pt; font-weight: bold; text-decoration: underline;}");
		auto descriptionHeader = new QLabel("Description", this);
		descriptionHeader->setStyleSheet(
			"QLabel { font-size: 14pt; font-weight: bold; text-decoration: underline;}");
		formGrid->addWidget(nameHeader, 0, 0);
		formGrid->addWidget(descriptionHeader, 0, 1);
		continueButton->setDisabled(true);
		int i = 1;
		for (auto const &[key, val] : devices) {
			auto label = new QLabel(val.c_str(), this);
			auto field = new QLineEdit(this);
			formGrid->addWidget(label, i, 0);
			formGrid->addWidget(field, i, 1);
			_labels[val] = "";
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
			i += 1;
		}
		layout->addLayout(formGrid);
	} else {
		continueButton->setDisabled(false);
		auto topSpace = new QWidget(this);
		topSpace->setSizePolicy(QSizePolicy::Preferred,
					QSizePolicy::Expanding);
		layout->addWidget(topSpace);
		auto noVCDevices =
			new QLabel("No video capture devices found.", this);
		noVCDevices->setAlignment(Qt::AlignCenter);
		noVCDevices->setStyleSheet("QLabel{font-size: 18pt;}");
		layout->addWidget(noVCDevices);
	}

	auto spacer = new QWidget(this);
	spacer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

	layout->addWidget(spacer);

	auto buttons = new QHBoxLayout(this);
	auto backButton = new QPushButton(this);
	backButton->setText("Back");
	connect(backButton, &QPushButton::released, this,
		[this]() { emit backPressed(); });

	continueButton->setText("Continue");
	connect(continueButton, &QPushButton::released, this,
		[this]() { emit continuePressed(); });
	buttons->addWidget(backButton);
	buttons->addWidget(continueButton);
	layout->addLayout(buttons);
}

RequiredPlugins::RequiredPlugins(QWidget *parent,
				 std::vector<obs_module_t *> installedPlugins)
	: QWidget(parent)
{
	QVBoxLayout *layout = new QVBoxLayout(this);

	std::string titleText = "Required Plug-ins";
	auto title = new QLabel(this);
	title->setText(titleText.c_str());
	title->setStyleSheet("QLabel{ font-size: 18pt; font-weight: bold;}");
	layout->addWidget(title);

	auto subTitle = new QLabel(this);
	subTitle->setText(
		"Please select all of the plugins that are required to use this bundle.");
	subTitle->setStyleSheet(
		"QLabel{ font-size: 11pt; font-style: italic; }");
	subTitle->setWordWrap(true);
	layout->addWidget(subTitle);

	if (installedPlugins.size() > 0) {
		auto pluginList = new QListWidget(this);
		pluginList->setSizePolicy(QSizePolicy::Preferred,
					  QSizePolicy::Expanding);
		for (auto module : installedPlugins) {
			std::string filename = obs_get_module_file_name(module);
			_pluginStatus[filename] = false;
			pluginList->addItem(filename.c_str());
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
				_pluginStatus[filename] = item->checkState() ==
							  Qt::Checked;
			});

		layout->addWidget(pluginList);
	} else {
		auto topSpace = new QWidget(this);
		topSpace->setSizePolicy(QSizePolicy::Preferred,
					QSizePolicy::Expanding);
		layout->addWidget(topSpace);
		auto noPlugins =
			new QLabel("No installed plugins found.", this);
		noPlugins->setAlignment(Qt::AlignCenter);
		noPlugins->setStyleSheet("QLabel{font-size: 18pt;}");
		layout->addWidget(noPlugins);
	}

	auto spacer = new QWidget(this);
	spacer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

	layout->addWidget(spacer);

	auto buttons = new QHBoxLayout(this);
	auto backButton = new QPushButton(this);
	backButton->setText("Back");
	connect(backButton, &QPushButton::released, this,
		[this]() { emit backPressed(); });

	auto continueButton = new QPushButton(this);
	continueButton->setText("Save Bundle As...");
	connect(continueButton, &QPushButton::released, this,
		[this]() { emit continuePressed(); });
	buttons->addWidget(backButton);
	buttons->addWidget(continueButton);
	layout->addLayout(buttons);
}

ExportComplete::ExportComplete(QWidget *parent) : QWidget(parent)
{
	QVBoxLayout *layout = new QVBoxLayout(this);

	std::string titleText = "Scene Bundle Exported";
	auto title = new QLabel(this);
	title->setText(titleText.c_str());
	title->setStyleSheet("QLabel{ font-size: 18pt; font-weight: bold;}");
	layout->addWidget(title);

	std::string subText =
		"Bundle successfully exported. Click 'Close' below to close this window.";
	auto sub = new QLabel(this);
	sub->setText(subText.c_str());
	sub->setStyleSheet("QLabel{ font-size: 11pt; font-style: italic; }");
	sub->setWordWrap(true);
	layout->addWidget(sub);

	auto spacer = new QWidget(this);
	spacer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

	layout->addWidget(spacer);

	auto buttons = new QHBoxLayout(this);

	auto closeButton = new QPushButton(this);
	closeButton->setText("Close");
	connect(closeButton, &QPushButton::released, this,
		[this]() { emit closePressed(); });

	buttons->addWidget(closeButton);
	layout->addLayout(buttons);
}

std::vector<std::string> RequiredPlugins::RequiredPluginList()
{
	std::vector<std::string> reqList;
	for (auto const &[key, val] : _pluginStatus) {
		if (val) {
			reqList.push_back(key);
		}
	}
	return reqList;
}

StreamPackageExportWizard::StreamPackageExportWizard(QWidget *parent)
	: QDialog(parent),
	  _steps(nullptr)
{
	obs_enum_modules(StreamPackageExportWizard::AddModule, this);

	char *currentCollection = obs_frontend_get_current_scene_collection();
	std::string collectionName = currentCollection;
	bfree(currentCollection);

	if (!_bundle.FromCollection(collectionName)) {
		return;
	}
	std::vector<std::string> fileList = _bundle.FileList();
	std::map<std::string, std::string> vidDevs =
		_bundle.VideoCaptureDevices();

	QVBoxLayout *layout = new QVBoxLayout(this);
	_steps = new QStackedWidget(this);

	// Step 1- check the media files to be bundled (step index: 0)
	auto fileCheck = new FileCollectionCheck(this, fileList);
	connect(fileCheck, &FileCollectionCheck::continuePressed, this,
		[this]() { _steps->setCurrentIndex(1); });
	connect(fileCheck, &FileCollectionCheck::cancelPressed, this,
		[this]() { close(); });
	_steps->addWidget(fileCheck);

	// Step 2- Add labels for each video capture device source (step index: 1)
	auto videoLabels = new VideoSourceLabels(this, vidDevs);
	connect(videoLabels, &VideoSourceLabels::continuePressed, this,
		[this]() { _steps->setCurrentIndex(2); });
	connect(videoLabels, &VideoSourceLabels::backPressed, this,
		[this]() { _steps->setCurrentIndex(0); });
	_steps->addWidget(videoLabels);

	// Step 2- Specify required plugins (step index: 2)
	auto reqPlugins = new RequiredPlugins(this, _modules);
	connect(reqPlugins, &RequiredPlugins::continuePressed, this,
		[this, videoLabels, reqPlugins]() {
			auto vidDevLabels = videoLabels->Labels();
			auto plugins = reqPlugins->RequiredPluginList();
			QWidget *window =
				(QWidget *)obs_frontend_get_main_window();
			QString filename = QFileDialog::getSaveFileName(
				window, "Save As...", QString(),
				"*.elgatoscene");
			if (filename == "") {
				_steps->setCurrentIndex(2);
				return;
			}
			if (!filename.endsWith(".elgatoscene")) {
				filename += ".elgatoscene";
			}
			std::string filename_utf8 =
				filename.toUtf8().constData();
			_bundle.ToElgatoCloudFile(filename_utf8, plugins,
						  vidDevLabels);
			_steps->setCurrentIndex(3);
		});
	connect(reqPlugins, &RequiredPlugins::backPressed, this,
		[this]() { _steps->setCurrentIndex(1); });
	_steps->addWidget(reqPlugins);

	// Successful Export
	auto success = new ExportComplete(this);
	connect(success, &ExportComplete::closePressed, this,
		[this]() { close(); });
	_steps->addWidget(success);
	layout->addWidget(_steps);
}

StreamPackageExportWizard::~StreamPackageExportWizard() {}

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
