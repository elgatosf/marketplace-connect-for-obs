#include <algorithm>

#include "export-wizard.hpp"
#include "elgato-styles.hpp"
#include "plugins.hpp"
#include <QMainWindow>
#include <QVBoxLayout>
#include <QPushButton>
#include <QListWidget>
#include <QLineEdit>
#include <QDir>

namespace elgatocloud {

SceneBundle* bundle = new SceneBundle();

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
	title->setAlignment(Qt::AlignCenter);
	title->setStyleSheet("QLabel{ font-size: 18pt; font-weight: bold;}");
	layout->addWidget(title);
	layout->setSpacing(20);
	std::string titleText =
		std::to_string(_files.size()) +
		" media files were found to bundle.\nIf this is correct, click 'Next' below.";
	auto subTitle = new QLabel(this);
	subTitle->setText(titleText.c_str());
	subTitle->setStyleSheet("QLabel {font-size: 12pt;}");
	subTitle->setAlignment(Qt::AlignCenter);
	subTitle->setWordWrap(true);
	layout->addWidget(subTitle);

	auto fileList = new QListWidget(this);
	for (auto fileName : _files) {
		fileList->addItem(fileName.c_str());
	}

	fileList->setStyleSheet(EListStyle);
	fileList->setSpacing(4);
	fileList->setSelectionMode(QAbstractItemView::NoSelection);
	fileList->setFocusPolicy(Qt::FocusPolicy::NoFocus);

	layout->addWidget(fileList);

	QHBoxLayout *buttons = new QHBoxLayout(this);

	QPushButton *continueButton = new QPushButton(this);
	continueButton->setText("Next");
	continueButton->setStyleSheet(EPushButtonStyle);
	connect(continueButton, &QPushButton::released, this,
		[this]() { emit continuePressed(); });

	auto spacer = new QWidget(this);
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	buttons->addWidget(spacer);
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
	title->setAlignment(Qt::AlignCenter);
	title->setStyleSheet(ETitleStyle);
	title->setText(titleText.c_str());
	layout->setSpacing(20);
	layout->addWidget(title);

	auto description = new QLabel(this);
	description->setText("Now go ahead and label your video sources.");
	description->setStyleSheet(
		"QLabel {font-size: 12pt; margin-bottom: 10px;}");
	description->setAlignment(Qt::AlignCenter);
	layout->addWidget(description);

	// Declare continue button so we can enable/disable it
	// on keypress in label list.
	auto continueButton = new QPushButton(this);
	auto formLayout = new QVBoxLayout();
	formLayout->setSpacing(4);
	if (devices.size() > 0) {
		continueButton->setDisabled(true);
		int i = 1;
		for (auto const &[key, val] : devices) {
			auto label = new QLabel(val.c_str(), this);
			label->setStyleSheet(
				"QLabel {font-size: 12pt; padding: 12px 0px 8px 0px; }");

			auto field = new QLineEdit(this);
			field->setPlaceholderText("Description text goes here");
			field->setStyleSheet(ELineEditStyle);
			formLayout->addWidget(label);
			formLayout->addWidget(field);
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
		}
		layout->addLayout(formLayout);
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

	auto buttonSpacer = new QWidget(this);
	buttonSpacer->setSizePolicy(QSizePolicy::Expanding,
				    QSizePolicy::Preferred);
	auto buttons = new QHBoxLayout(this);
	auto backButton = new QPushButton(this);
	backButton->setText("Back");
	backButton->setStyleSheet(EPushButtonStyle);
	connect(backButton, &QPushButton::released, this,
		[this]() { emit backPressed(); });

	continueButton->setText("Next");
	continueButton->setStyleSheet(EPushButtonStyle);

	connect(continueButton, &QPushButton::released, this,
		[this]() { emit continuePressed(); });
	buttons->addWidget(buttonSpacer);
	buttons->addWidget(backButton);
	buttons->addWidget(continueButton);
	layout->addLayout(buttons);
}

RequiredPlugins::RequiredPlugins(QWidget *parent,
				 std::vector<obs_module_t *> installedPlugins)
	: QWidget(parent)
{
	PluginInfo pi;

	auto installed = pi.installed();
	// Set up the paths for checked/unchecked images.
	// TODO: Refactor this, there must be a better way.
	std::string imagesPath = obs_get_module_data_path(obs_current_module());
	imagesPath += "/images/";
	std::string checkedImage = imagesPath + "checkbox_checked.png";
	std::string uncheckedImage = imagesPath + "checkbox_unchecked.png";
	QString checklistStyle = EChecklistStyleTemplate;
	checklistStyle.replace("${checked-img}", checkedImage.c_str());
	checklistStyle.replace("${unchecked-img}", uncheckedImage.c_str());

	QVBoxLayout *layout = new QVBoxLayout(this);
	std::string titleText = "Required Plug-ins";
	auto title = new QLabel(this);
	title->setText(titleText.c_str());
	title->setAlignment(Qt::AlignCenter);
	title->setStyleSheet(ETitleStyle);
	layout->addWidget(title);
	layout->setSpacing(20);

	auto subTitle = new QLabel(this);
	subTitle->setText(
		"Please select all plugins that are required to use this scene collection.");
	subTitle->setStyleSheet("QLabel {font-size: 12pt;}");
	subTitle->setAlignment(Qt::AlignCenter);
	subTitle->setWordWrap(true);
	layout->addWidget(subTitle);

	if (installed.size() > 0) {
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

	//auto spacer = new QWidget(this);
	//spacer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

	//layout->addWidget(spacer);

	auto buttons = new QHBoxLayout(this);
	auto buttonSpacer = new QWidget(this);
	buttonSpacer->setSizePolicy(QSizePolicy::Expanding,
				    QSizePolicy::Preferred);
	auto backButton = new QPushButton(this);
	backButton->setText("Back");
	backButton->setStyleSheet(EPushButtonStyle);
	connect(backButton, &QPushButton::released, this,
		[this]() { emit backPressed(); });

	auto continueButton = new QPushButton(this);
	continueButton->setText("Next");
	continueButton->setStyleSheet(EPushButtonStyle);

	connect(continueButton, &QPushButton::released, this,
		[this]() { emit continuePressed(); });
	buttons->addWidget(buttonSpacer);
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
	title->setStyleSheet(ETitleStyle);
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
	closeButton->setStyleSheet(EPushButtonStyle);
	connect(closeButton, &QPushButton::released, this,
		[this]() { emit closePressed(); });

	buttons->addWidget(closeButton);
	layout->addLayout(buttons);
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

Exporting::Exporting(QWidget *parent) : QWidget(parent)
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
	title->setText("Exporting Bundle...");
	title->setStyleSheet(ETitleStyle);
	title->setAlignment(Qt::AlignCenter);
	layout->addWidget(title);

	auto subTitle = new QLabel(this);
	subTitle->setText(
		"Note: this can take a few minutes for scene collections with large files.");
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
	obs_enum_modules(StreamPackageExportWizard::AddModule, this);
	setWindowTitle("Elgato Deep Linking Export");
	setStyleSheet("background-color: #232323");
	std::string homeDir = QDir::homePath().toStdString();

	char *currentCollection = obs_frontend_get_current_scene_collection();
	std::string collectionName = currentCollection;
	bfree(currentCollection);

	if (!bundle->FromCollection(collectionName)) {
		return;
	}
	std::vector<std::string> fileList = bundle->FileList();
	std::map<std::string, std::string> vidDevs =
		bundle->VideoCaptureDevices();

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
	connect(reqPlugins, &RequiredPlugins::continuePressed, this, [this, videoLabels, reqPlugins]() {
		auto vidDevLabels = videoLabels->Labels();
		auto plugins = reqPlugins->RequiredPluginList();
		QWidget *window = (QWidget *)obs_frontend_get_main_window();
		QString filename = QFileDialog::getSaveFileName(
			window, "Save As...", QString(), "*.elgatoscene");
		if (filename == "") {
			_steps->setCurrentIndex(2);
			return;
		}
		if (!filename.endsWith(".elgatoscene")) {
			filename += ".elgatoscene";
		}
		_steps->setCurrentIndex(3);
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
								_steps->setCurrentIndex(
									4);
							} else if (
								status ==
								SceneBundleStatus::
									Cancelled) {
								_steps->setCurrentIndex(
									2);
							} else if (
								status ==
								SceneBundleStatus::
									CallerDestroyed) {
								blog(LOG_INFO,
								     "Wizard Closed");
							}
						});
				})
				.onCanceled([]() {

				});
		//createBundle(filename_utf8, plugins, vidDevLabels);
	});
	connect(reqPlugins, &RequiredPlugins::backPressed, this,
		[this]() { _steps->setCurrentIndex(1); });
	_steps->addWidget(reqPlugins);

	auto exporting = new Exporting(this);
	_steps->addWidget(exporting);

	// Successful Export
	auto success = new ExportComplete(this);
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
