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
	setWindowTitle(QString("Elgato Marketplace Connect Update Available"));
	setFixedSize(QSize(680, 300));
	setAttribute(Qt::WA_DeleteOnClose);

	auto layout = new QVBoxLayout();

	auto title = new QLabel(this);
	title->setText(
		obs_module_text("UpdateModal.Title"));
	title->setAlignment(Qt::AlignCenter);
	title->setStyleSheet("QLabel { font-size: 16pt; }");
	title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	layout->addWidget(title);
	layout->addStretch();
	auto description = new QLabel(this);
	description->setText(obs_module_text("UpdateModal.Description"));
	description->setAlignment(Qt::AlignCenter);
	description->setWordWrap(true);
	description->setStyleSheet("QLabel { font-size: 13pt; }");
	description->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	
	layout->addWidget(description);

	auto buttons = new QHBoxLayout();

	QPushButton* skipButton = new QPushButton(this);
	skipButton->setText(
		obs_module_text("UpdateModal.SkipVersionButton"));
	skipButton->setStyleSheet(EPushButtonCancelStyle);

	QPushButton* laterButton = new QPushButton(this);
	laterButton->setText(
		obs_module_text("UpdateModal.LaterButton"));
	laterButton->setStyleSheet(EPushButtonCancelStyle);

	QPushButton* downloadButton = new QPushButton(this);
	downloadButton->setText(
		obs_module_text("UpdateModal.DownloadUpdateButton"));
	downloadButton->setStyleSheet(EPushButtonStyle);

	
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
		ShellExecuteA(NULL, NULL, downloadUrl.c_str(), NULL, NULL, SW_SHOW);
		close();
	});

	layout->addStretch();
	layout->addLayout(buttons);
	setStyleSheet("background-color: #232323");

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