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
	downloadButton->setText(obs_module_text("SetupWizard.MissingPlugins.DownloadButton"));
	downloadButton->setStyleSheet(EWizardButtonStyle);

	connect(downloadButton, &QPushButton::released, this,
		[url]() { QDesktopServices::openUrl(QUrl(url.c_str())); });
	layout->addWidget(downloadButton);
}

SceneCollectionInfo::SceneCollectionInfo(std::vector<SceneCollectionLineItem> const& rows, QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle(obs_module_text("SceneCollectionInfo.WindowTitle"));
	setStyleSheet("background-color: #151515;");
	setModal(true);
	setFixedSize(800, 448);
	QVBoxLayout* layout = new QVBoxLayout(this);

	QLabel* title = new QLabel(this);
	title->setText(obs_module_text("SceneCollectionInfo.Title"));
	title->setStyleSheet(EWizardStepTitle);
	layout->addWidget(title);

	QLabel* subTitle = new QLabel(this);
	subTitle->setText(obs_module_text("SceneCollectionInfo.Text"));
	subTitle->setWordWrap(true);
	subTitle->setStyleSheet(EWizardStepSubTitle);
	layout->addWidget(subTitle);

	auto thirdPartyList = new QListWidget(this);
	for (auto& row : rows) {
		auto item = new QListWidgetItem();
		item->setSizeHint(QSize(0, 60));
		auto itemWidget = new ThirdPartyItem(
			row.name.c_str(), row.url.c_str(), this);
		thirdPartyList->addItem(item);
		thirdPartyList->setItemWidget(item, itemWidget);
	}

	thirdPartyList->setStyleSheet(EMissingPluginsStyle);
	thirdPartyList->setSpacing(4);
	thirdPartyList->setSelectionMode(QAbstractItemView::NoSelection);
	thirdPartyList->setFocusPolicy(Qt::FocusPolicy::NoFocus);

	layout->addWidget(thirdPartyList);
	layout->addStretch();

	QHBoxLayout* buttons = new QHBoxLayout();
	QPushButton* closeButton = new QPushButton(this);
	closeButton->setText(obs_module_text("SceneCollectionInfo.CloseButton"));
	closeButton->setStyleSheet(EWizardButtonStyle);

	connect(closeButton, &QPushButton::released, this,
		[this]() { close(); });
	buttons->addStretch();
	buttons->addWidget(closeButton);
	layout->addLayout(buttons);
}

}