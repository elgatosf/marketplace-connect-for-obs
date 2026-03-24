#include "elgato-update-modal.hpp"
#include "elgato-cloud-data.hpp"
#include "elgato-styles.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>
#include <QMainWindow>
#include <QApplication>

#include <curl/curl.h>


namespace elgatocloud {

ElgatoUpdateModal* updateModal = nullptr;

ElgatoUpdateModal::ElgatoUpdateModal(QWidget* parent, std::string version, std::string downloadUrl)
	: QDialog(parent)
{
	std::string imageBaseDir =
		obs_get_module_data_path(obs_current_module());
	imageBaseDir += "/images/";

	setWindowTitle(QString("Elgato Marketplace Connect Update Available"));
	setFixedSize(QSize(680, 532));
	setAttribute(Qt::WA_DeleteOnClose);

	auto layout = new QVBoxLayout();

	auto icon = new QLabel(this);
	std::string iconImgPath = imageBaseDir + "IconSync.svg";
	QPixmap iconPixmap = QPixmap(iconImgPath.c_str());
	icon->setPixmap(iconPixmap);
	icon->setAlignment(Qt::AlignCenter);

	auto title = new QLabel(this);
	title->setText(
		obs_module_text("UpdateModal.Title"));
	title->setAlignment(Qt::AlignCenter);
	title->setStyleSheet(EWizardStepTitle);
	title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	layout->addStretch();
	layout->addWidget(icon);
	layout->addWidget(title);
	
	auto hLayout = new QHBoxLayout();
	auto description = new QLabel(this);
	std::string descriptionText = obs_module_text("UpdateModal.Description");
	std::string linkText = obs_module_text("UpdateModal.ChangesLinkText");
	std::string link = "<a href='https://help.elgato.com/hc/en-us/sections/32970231491345-Marketplace-Connect-for-OBS-Release-Notes'>" + linkText + "</a>";
	replace_all(descriptionText, "{CHANGES LINK}", link);
	description->setText(descriptionText.c_str());
	description->setAlignment(Qt::AlignCenter);
	description->setWordWrap(true);
	description->setStyleSheet(EWizardStepSubTitle);
	description->setFixedWidth(480);
	description->setTextInteractionFlags(Qt::TextBrowserInteraction);
	description->setTextFormat(Qt::RichText);
	description->setOpenExternalLinks(true);
	hLayout->addStretch();
	hLayout->addWidget(description);
	hLayout->addStretch();
	
	layout->addLayout(hLayout);

	auto buttons = new QHBoxLayout();

	QPushButton* skipButton = new QPushButton(this);
	skipButton->setText(
		obs_module_text("UpdateModal.SkipVersionButton"));
	skipButton->setStyleSheet(EWizardQuietButtonStyle);

	QPushButton* laterButton = new QPushButton(this);
	laterButton->setText(
		obs_module_text("UpdateModal.LaterButton"));
	laterButton->setStyleSheet(EWizardQuietButtonStyle);

	QPushButton* downloadButton = new QPushButton(this);
	downloadButton->setText(
		obs_module_text("UpdateModal.DownloadUpdateButton"));
	downloadButton->setStyleSheet(EWizardButtonStyle);
	
	buttons->addWidget(skipButton);
	buttons->addStretch();
	buttons->addWidget(laterButton);
	buttons->addWidget(downloadButton);
	connect(skipButton, &QPushButton::released, this, [this, version]() {
		elgatoCloud->SetSkipVersion(version);
		close();
	});

	connect(laterButton, &QPushButton::released, this, [this]() {
		close();
	});

	connect(downloadButton, &QPushButton::released, this, [this, downloadUrl]() {
	#ifdef WIN32
		ShellExecuteA(NULL, NULL, downloadUrl.c_str(), NULL, NULL, SW_SHOW);
	#elif __APPLE__
		// TODO: MacOS Download Button
	#endif
		close();
	});

	layout->addStretch();
	layout->addLayout(buttons);
	setStyleSheet("background-color: #151515");
	layout->setSpacing(16);
	setLayout(layout);

	updateModal = this;
}

ElgatoUpdateModal::~ElgatoUpdateModal()
{
	updateModal = nullptr;
}

void openUpdateModal(std::string version, std::string downloadUrl)
{
	const auto mainWindow = static_cast<QMainWindow*>(
		obs_frontend_get_main_window());
	ElgatoUpdateModal* window = nullptr;
	if (updateModal == nullptr) {
		window = new ElgatoUpdateModal(mainWindow, version, downloadUrl);
		window->exec();
	}
}

}
