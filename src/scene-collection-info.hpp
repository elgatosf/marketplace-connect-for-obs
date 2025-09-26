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

#pragma once
#include <QDialog>
#include <QStackedWidget>
#include <QPushButton>
#include <nlohmann/json.hpp>

namespace elgatocloud {

struct SceneCollectionLineItem {
	std::string name;
	std::string url;
};

class ThirdPartyItem : public QWidget {
	Q_OBJECT
public:
	ThirdPartyItem(std::string label, std::string url, QWidget* parent);
};

class ThirdPartyWidget : public QWidget {
	Q_OBJECT

public:
	ThirdPartyWidget(nlohmann::json tpData, QWidget *parent = nullptr);
};

class StreamDeckInstallWidget : public QWidget {
	Q_OBJECT
public:
	StreamDeckInstallWidget(nlohmann::json scData, bool disabled,
				QWidget *parent = nullptr);
};

class SceneCollectionInfo : public QDialog {
	Q_OBJECT

public:
	SceneCollectionInfo(nlohmann::json scData, QWidget* parent = nullptr);

private:
	void updateButtons_();
	QStackedWidget *content_;
	QPushButton *backButton_;
	QPushButton *nextButton_;
	QPushButton *doneButton_;
	int step_;
	int steps_;
};

class SceneCollectionConfig : public QDialog {
	Q_OBJECT
public:
	SceneCollectionConfig(nlohmann::json scData, QWidget *parent = nullptr);
};

}