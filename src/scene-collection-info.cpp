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

#include "scene-collection-info.hpp"
#include "elgato-styles.hpp"
#include "elgato-stream-deck-widgets.hpp"
#include "elgato-cloud-data.hpp"
#include "util.h"
#include <obs-module.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QListWidget>
#include <QDesktopServices>
#include <QLabel>

namespace elgatocloud {

ThirdPartyItem::ThirdPartyItem(std::string label,
	std::string url, QWidget* parent)
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
	downloadButton->setText(obs_module_text("SceneCollectionInfo.ThirdParty.GetButton"));
	downloadButton->setStyleSheet(EWizardButtonStyle);

	connect(downloadButton, &QPushButton::released, this,
		[url]() { QDesktopServices::openUrl(QUrl(url.c_str())); });
	layout->addWidget(downloadButton);
}

ThirdPartyWidget::ThirdPartyWidget(nlohmann::json tpData, QWidget *parent)
	: QWidget(parent)
{
	QVBoxLayout *layout = new QVBoxLayout(this);
	QLabel* title = new QLabel(this);
	title->setText(obs_module_text("SceneCollectionInfo.ThirdParty.Title"));
	title->setStyleSheet(EWizardStepTitle);
	layout->addWidget(title);

	QLabel* subTitle = new QLabel(this);
	subTitle->setText(obs_module_text("SceneCollectionInfo.ThirdParty.Text"));
	subTitle->setWordWrap(true);
	subTitle->setStyleSheet(EWizardStepSubTitle);
	layout->addWidget(subTitle);

	auto thirdPartyList = new QListWidget(this);
	for (auto& row : tpData) {
		auto item = new QListWidgetItem();
		std::string name = row["name"];
		std::string url = row["url"];
		item->setSizeHint(QSize(0, 60));
		auto itemWidget = new ThirdPartyItem(
			name.c_str(), url.c_str(), this);
		thirdPartyList->addItem(item);
		thirdPartyList->setItemWidget(item, itemWidget);
	}

	thirdPartyList->setStyleSheet(EMissingPluginsStyle);
	thirdPartyList->setSpacing(4);
	thirdPartyList->setSelectionMode(QAbstractItemView::NoSelection);
	thirdPartyList->setFocusPolicy(Qt::FocusPolicy::NoFocus);

	layout->addWidget(thirdPartyList);
	layout->addStretch();
}

StreamDeckInstallWidget::StreamDeckInstallWidget(nlohmann::json scData,
						 bool disabled, QWidget *parent)
	: QWidget(parent)
{
	QVBoxLayout *layout = new QVBoxLayout(this);
	QLabel *title = new QLabel(this);
	title->setText(obs_module_text("SceneCollectionInfo.StreamDeck.Title"));
	title->setStyleSheet(EWizardStepTitle);
	layout->addWidget(title);

	QLabel *subTitle = new QLabel(this);
	std::vector<SDFileDetails> sdaFiles;
	std::vector<SDFileDetails> sdProfileFiles;
	try {
		std::string basePath =
			scData["pack_path"].get<std::string>() + "/";
		if (scData.contains("stream_deck_actions")) {
			std::string sdaBasePath =
				basePath +
				"Assets/stream-deck/stream-deck-actions/";
			for (auto &sdaFile : scData["stream_deck_actions"]) {
				std::string filename =
					sdaBasePath +
					sdaFile["filename"].get<std::string>();
				sdaFiles.push_back(
					{filename, sdaFile["label"]});
			}
		}
		if (scData.contains("stream_deck_profiles")) {
			std::string sdaBasePath =
				basePath +
				"Assets/stream-deck/stream-deck-profiles/";
			for (auto &sdaFile : scData["stream_deck_profiles"]) {
				std::string filename =
					sdaBasePath +
					sdaFile["filename"].get<std::string>();
				sdProfileFiles.push_back(
					{filename, sdaFile["label"]});
			}
		}
	} catch (...) {
	}

	bool legacy = true;
	auto ec = GetElgatoCloud();
	auto sdi = ec->GetStreamDeckInfo();
	bool requiresLegacy = compareVersions(sdi.version, "7.1") < 0;

	for (auto& sda : sdaFiles) {
		SdaFile sdaf(sda.path.c_str());
		if (sdaf.fileVersion() == SdFileVersion::Current) {
			legacy = false;
		}
	}

	for (auto &sdp : sdProfileFiles) {
		SdProfileFile sdpf(sdp.path.c_str());
		if (sdpf.fileVersion() == SdFileVersion::Current) {
			legacy = false;
		}
	}

	if (disabled) {
		subTitle->setText(obs_module_text(
			"SceneCollectionInfo.StreamDeck.StreamDeckRequired"));
	} else if (!legacy && requiresLegacy) {
		disabled = true;
		subTitle->setText(obs_module_text(
			"SceneCollectionInfo.StreamDeck.StreamDeckUpdateRequired"));
	} else {
		subTitle->setText(
			obs_module_text("SceneCollectionInfo.StreamDeck.Text"));
	}

	subTitle->setWordWrap(true);
	subTitle->setStyleSheet(EWizardStepSubTitle);
	layout->addWidget(subTitle);


	StreamDeckSetupWidget *widget = new StreamDeckSetupWidget(sdaFiles, sdProfileFiles, disabled, this);
	widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	widget->setFixedHeight(400);
	layout->addWidget(widget);

}

SceneCollectionInfo::SceneCollectionInfo(nlohmann::json scData, QWidget *parent)
	: QDialog(parent),
	  step_(1)
{
	bool hasSdActions = scData.contains("stream_deck_actions") &&
			     scData["stream_deck_actions"].size() > 0;
	bool hasSdProfiles = scData.contains("stream_deck_profiles") &&
			     scData["stream_deck_profiles"].size() > 0;
	bool hasThirdParty = scData.contains("third_party") &&
			     scData["third_party"].size() > 0;

	bool sdInstalled = isProtocolHandlerRegistered(L"streamdeck");

	bool hasSd = (hasSdActions || hasSdProfiles);
	steps_ = 0;
	if (hasSd) {
		steps_++;
	}
	if (hasThirdParty) {
		steps_++;
	}

	setWindowFlags(
		Qt::Dialog |
		Qt::MSWindowsFixedSizeDialogHint | // prevents frame recalculation
		Qt::CustomizeWindowHint | Qt::WindowTitleHint |
		Qt::WindowCloseButtonHint);

	setFixedSize(800, 448);
	setMinimumSize(800, 448);
	setMaximumSize(800, 448);

	setWindowTitle(obs_module_text("SceneCollectionInfo.WindowTitle"));
	setStyleSheet("background-color: #151515;");
	setModal(true);
	QVBoxLayout* layout = new QVBoxLayout(this);
	content_ = new QStackedWidget(this);
	content_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	if (hasThirdParty) {
		auto thirdParty =
			new ThirdPartyWidget(scData["third_party"]);
		content_->addWidget(thirdParty);
	}

	if (hasSd) {
		auto sd = new StreamDeckInstallWidget(scData, !sdInstalled);
		content_->addWidget(sd);
	}

	QHBoxLayout* buttons = new QHBoxLayout();
	backButton_ = new QPushButton(this);
	backButton_->setText(
		obs_module_text("SceneCollectionInfo.BackButton"));
	backButton_->setStyleSheet(EWizardButtonStyle);
	connect(backButton_, &QPushButton::released, this,
		[this]() { 
			step_--;
			updateButtons_();
		}
	);

	nextButton_ = new QPushButton(this);
	nextButton_->setText(obs_module_text("SceneCollectionInfo.NextButton"));
	nextButton_->setStyleSheet(EWizardButtonStyle);
	connect(nextButton_, &QPushButton::released, this, [this]() {
		step_++;
		updateButtons_();
	});

	doneButton_ = new QPushButton(this);
	doneButton_->setText(obs_module_text("SceneCollectionInfo.DoneButton"));
	doneButton_->setStyleSheet(EWizardButtonStyle);
	connect(doneButton_, &QPushButton::released, this, [this]() {
		close();
	});

	buttons->addStretch();
	buttons->addWidget(backButton_);
	buttons->addWidget(nextButton_);
	buttons->addWidget(doneButton_);
	layout->addWidget(content_);
	layout->addLayout(buttons);

	updateButtons_();
}

void SceneCollectionInfo::updateButtons_()
{
	if (step_ <= steps_) {
		content_->setCurrentIndex(step_ - 1);
	}
	backButton_->setVisible(step_ > 1);
	nextButton_->setVisible(step_ < steps_);
	doneButton_->setVisible(step_ == steps_);
}

SceneCollectionConfig::SceneCollectionConfig(nlohmann::json scData,
	QWidget* parent)
	: QDialog(parent)
{
	bool hasSdActions = scData.contains("stream_deck_actions") &&
			    scData["stream_deck_actions"].size() > 0;
	bool hasSdProfiles = scData.contains("stream_deck_profiles") &&
			     scData["stream_deck_profiles"].size() > 0;
	bool hasThirdParty = scData.contains("third_party") &&
			     scData["third_party"].size() > 0;

	bool hasSd = (hasSdActions || hasSdProfiles);

	setWindowFlags(
		Qt::Dialog |
		Qt::MSWindowsFixedSizeDialogHint | // prevents frame recalculation
		Qt::CustomizeWindowHint | Qt::WindowTitleHint |
		Qt::WindowCloseButtonHint);

	setFixedSize(800, 448);
	setMinimumSize(800, 448);
	setMaximumSize(800, 448);

	setWindowTitle(obs_module_text("SceneCollectionConfig.WindowTitle"));
	setStyleSheet("background-color: #151515;");
	setModal(true);

	auto sideMenu = new QListWidget(this);
	
	auto stack = new QStackedWidget(this);

	ThirdPartyWidget *thirdPartyWidget = nullptr;
	StreamDeckInstallWidget *streamDeckWidget = nullptr;

	if (hasThirdParty) {
		auto thirdParty = new QListWidgetItem(
			obs_module_text("SceneCollectionConfig.ThirdPartyDownloads"));
		sideMenu->addItem(thirdParty);

		thirdPartyWidget = new ThirdPartyWidget(scData["third_party"], this);
		stack->addWidget(thirdPartyWidget);
	}
	

	if (hasSd) {
		bool sdInstalled = isProtocolHandlerRegistered(L"streamdeck");
		bool disableSd = !sdInstalled;

		auto streamDeck = new QListWidgetItem(obs_module_text(
			"SceneCollectionConfig.StreamDeckIntegration"));
		sideMenu->addItem(streamDeck);
		streamDeckWidget = new StreamDeckInstallWidget(scData, disableSd, this);
		stack->addWidget(streamDeckWidget);
	}
	
	//sideMenu->setIconSize(QSize(20, 20));

	sideMenu->setSizePolicy(QSizePolicy::Preferred,
				 QSizePolicy::Expanding);
	sideMenu->setStyleSheet(ELeftNavListStyle);
	sideMenu->setCurrentRow(0);
	sideMenu->setFixedWidth(240);
	connect(sideMenu, &QListWidget::itemPressed, this,
		[this, stack, thirdPartyWidget, streamDeckWidget](QListWidgetItem *item) {
			QString val = item->text();
			if (val == obs_module_text("SceneCollectionConfig.ThirdPartyDownloads")) {
				stack->setCurrentWidget(thirdPartyWidget);
			} else if (val == obs_module_text("SceneCollectionConfig.StreamDeckIntegration")) {
				stack->setCurrentWidget(streamDeckWidget);
			}
		});




	auto layout = new QHBoxLayout(this);
	layout->addWidget(sideMenu);
	layout->addWidget(stack);
}

}