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
#include <QString>

namespace elgatocloud {
// Constant strings for different element styles
// Cant use QT style sheets in a plugin reliably,
// so instead we encode the styles as constant
// strings.

inline const QString EListStyle = "QListWidget {"
				  "border: none;"
				  "background: #151515;"
				  "border-radius: 8px;"
				  "}"
				  "QListWidget::item {"
				  "border: none;"
				  "padding: 8px;"
				  "background-color: #232323;"
				  "border-radius: 8px;"
				  "}"
				  "QListWidget::item:selected {"
				  "border: none;"
				  "}";

inline const QString EMissingPluginsStyle = "QListWidget {"
					    "border: none;"
					    "background: #151515;"
					    "border-radius: 8px;"
					    "}"
					    "QListWidget::item {"
					    "border: none;"
					    "padding: 0px;"
					    "background-color: #232323;"
					    "border-radius: 8px;"
					    "}"
					    "QListWidget::item:selected {"
					    "border: none;"
					    "}";

inline const QString EPushButtonStyle = "QPushButton {"
					"font-size: 12pt;"
					"padding: 8px 36px 8px 36px;"
					"background-color: #204cfe;"
					"border-radius: 8px;"
					"border: none;"
					"}"
					"QPushButton:hover {"
					"background-color: #193ed4;"
					"}"
					"QPushButton:disabled {"
					"background-color: #1c1c1c;"
					"border: none;"
					"}";

inline const QString EPushButtonDarkStyle = "QPushButton {"
					    "font-size: 12pt;"
					    "padding: 8px 36px 8px 36px;"
					    "background-color: #3b3b3b;"
					    "border-radius: 8px;"
					    "border: none;"
					    "}"
					    "QPushButton:hover {"
					    "background-color: #193ed4;"
					    "}"
					    "QPushButton:disabled {"
					    "background-color: #1c1c1c;"
					    "border: none;"
					    "}";

inline const QString EPushButtonCancelStyle = "QPushButton {"
					      "font-size: 12pt;"
					      "padding: 8px 36px 8px 36px;"
					      "background-color: #414141;"
					      "border-radius: 8px;"
					      "border: none;"
					      "}"
					      "QPushButton:hover {"
					      "background-color: #193ed4;"
					      "}"
					      "QPushButton:disabled {"
					      "background-color: #1c1c1c;"
					      "border: none;"
					      "}";

inline const QString ELineEditStyle = "QLineEdit {"
				      "background-color: #151515;"
				      "border: none;"
				      "padding: 12px;"
				      "font-size: 11pt;"
				      "border-radius: 8px;"
				      "}";

inline const QString ETitleStyle =
	"QLabel{ font-size: 18pt; font-weight: bold; }";

inline const QString EChecklistStyleTemplate =
	"QListWidget {"
	"border: none;"
	"background: #151515;"
	"border-radius: 8px;"
	"}"
	"QListWidget::item {"
	"border: none;"
	"padding: 8px;"
	"background-color: #232323;"
	"border-radius: 8px;"
	"}"
	"QListWidget::item:selected {"
	"border: none;"
	"}"
	"QListWidget::indicator {"
	"width: 20px;"
	"height: 20px;"
	"background: none;"
	"}"
	"QListWidget::indicator:checked {"
	"background-image: url('${checked-img}')"
	"}"
	"QListWidget::indicator:unchecked {"
	"background-image: url('${unchecked-img}')"
	"}";

inline const QString ECheckBoxStyle =
	"QCheckBox {"
	"margin-left: 16px;"
	"margin-top: 8px;"
	"font-size: 12pt;"
	"}"
	"QCheckBox::indicator {"
	"width: 20px;"
	"height: 20px;"
	"}"
	"QCheckBox::indicator:checked {"
	"image: url('${checked-img}')"
	"}"
	"QCheckBox::indicator:unchecked {"
	"image: url('${unchecked-img}')"
	"}";

inline const QString EComboBoxStyle = "QComboBox {"
				      "background-color: #151515;"
				      "border: none;"
				      "padding: 12px;"
				      "font-size: 11pt;"
				      "border-radius: 8px;"
				      "}";

inline const QString EComboBoxStyleLight = "QComboBox {"
					   "background-color: #FFFFFF;"
					   "color: #444444;"
					   "border: none;"
					   "padding: 12px;"
					   "font-size: 11pt;"
					   "border-radius: 8px;"
					   "}";

inline const QString EIconButtonStyle = "QPushButton {"
					"background: transparent;"
					"border: none;"
					"padding-left: 4px;"
					"padding-right: 4px;"
					"}";

inline const QString EIconHoverButtonStyle =
	"QPushButton {"
	"background: transparent;"
	"background-repeat: no-repeat;"
	"border: none;"
	"width: 24px;"
	"height: 24px;"
	"padding: 0px;"
	"margin: 0px;"
	"border-image: url('${img}');"
	"}"

	"QPushButton:hover {"
	"border-image: url('${hover-img}');"
	"}";

inline const QString EInlineIconHoverButtonStyle =
	"QPushButton {"
	"background: transparent;"
	"background-repeat: no-repeat;"
	"border: none;"
	//"width: 24px;"
	//"height: 24px;"
	"padding: 0px;"
	"margin: 0px 16px 0px 0px;"
	"border-image: url('${img}');"
	"}"

	"QPushButton:hover {"
	"border-image: url('${hover-img}');"
	"}";

inline const QString EIconHoverDisabledButtonStyle =
	"QPushButton {"
	"background: transparent;"
	"background-repeat: no-repeat;"
	"border: none;"
	"width: 24px;"
	"height: 24px;"
	"padding: 0px;"
	"margin: 0px;"
	"border-image: url('${img}');"
	"}"

	"QPushButton:hover {"
	"border-image: url('${hover-img}');"
	"}"

	"QPushButton:disabled {"
	"border-image: url('${disabled-img}');"
	"}";
} // namespace elgatocloud
