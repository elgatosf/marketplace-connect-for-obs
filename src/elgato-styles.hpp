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
		"font-size: 16px;"
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

	inline const QString EIconOnlyButtonStyle =
		"QPushButton {"
		"background: transparent;"
		"background-repeat: no-repeat;"
		"border-radius: 8px;"
		"border: none;"
		"width: 32px;"
		"height: 32px;"
		"margin: 0px;"
		"border-image: url('${img}');"
		"}"
		"QPushButton:hover {"
		"background: rgba(255, 255, 255, 0.2);"
		"}"
		"QPushButton:pressed {"
		"background: rgba(255, 255, 255, 0.3);"
		"}";

	inline const QString EBlankSlateButtonStyle =
		"QPushButton {"
		"background: #204CFE;"
		"border-radius: 8px;"
		"border: none;"
		"color: #FFFFFF;"
		"font-size: 14px;"
		"height: 32px;"
		"padding-left: 8px;"
		"padding-right: 8px;"
		"}"
		"QPushButton:hover {"
		"background: #193ED4;"
		"}"
		"QPushButton:pressed {"
		"background: #1231AC;"
		"}";

	inline const QString EBlankSlateQuietButtonStyle =
		"QPushButton {"
		"background: rgba(255, 255, 255, 0.1);"
		"border-radius: 8px;"
		"border: none;"
		"color: #FFFFFF;"
		"font-size: 14px;"
		"height: 32px;"
		"padding-left: 8px;"
		"padding-right: 8px;"
		"}"
		"QPushButton:hover {"
		"background: rgba(255, 255, 255, 0.2);"
		"}"
		"QPushButton:pressed {"
		"background: rgba(255, 255, 255, 0.3);"
		"}";

	inline const QString EStopDownloadButtonStyle =
		"QPushButton {"
		"background: #2f2f2f;"
		"border-radius: 8px;"
		"border: none;"
		"color: #FFFFFF;"
		"font-size: 14px;"
		"height: 32px;"
		"padding-left: 8px;"
		"padding-right: 8px;"
		"}"
		"QPushButton:hover {"
		"background: #494949;"
		"}"
		"QPushButton:pressed {"
		"background: #636363;"
		"}";

	inline const QString EBlankSlateTitleStyle =
		"QLabel {"
		"color: #FFFFFF;"
		"font-weight: bold;"
		"font-size: 16px;"
		"}";

	inline const QString EBlankSlateSubTitleStyle =
		"QLabel {"
		"color: rgba(255, 255, 255, 0.67);"
		"font-size: 14px;"
		"}";

	inline const QString ESlateContainerStyle =
		"QWidget{"
		"background-color: #151515;"
		"border-radius: 16px;"
		"border: none;"
		"}";

	inline const QString ELeftNavListStyle =
		"QListWidget {"
		"border: none;"
		"font-size: 14px;"
		"outline: none;"
		"padding-top: 8px;"
		"}"
		"QListWidget::item {"
		"line-height: 20px;"
		"font-size: 14px;"
		"padding: 4px 8px;"
		"border-radius: 8px;"
		"background-color: #151515"
		"}"
		"QListWidget::item:selected {"
		"background-color: #1f1f1f;"
		"color: white;"
		"}"
		"QListWidget::item:hover {"
		"background-color: #1f1f1f;"
		"}";

	inline const QString EStepperLabelPrior =
		"QLabel {"
		"color: #FFFFFF;"
		"font-size: 14px;"
		"font-weight: 400;"
		"};";

	inline const QString EStepperLabelCurrent =
		"QLabel {"
		"color: #FFFFFF;"
		"font-size: 14px;"
		"font-weight: 500;"
		"};";

	inline const QString EStepperLabelFuture =
		"QLabel {"
		"color: rgba(255, 255, 255, 0.67);"
		"font-size: 14px;"
		"font-weight: 500;"
		"};";

	inline const QString EWizardWindow =
		"{"
		"background-color: #151515;"
		"}";

	inline const QString EWizardStepTitle =
		"QLabel {"
		"color: #FFFFFF;"
		"font-size: 16px;"
		"font-weight: bold;"
		"}";

	inline const QString EWizardStepSubTitle =
		"QLabel {"
		"color: rgba(255, 255, 255, 0.67);"
		"font-size: 14px;"
		"font-weight: 400;"
		"}";

	inline const QString EWizardFieldLabel =
		"QLabel {"
		"color: #FFFFFF;"
		"font-size: 14px;"
		"font-weight: 400;"
		"}";

	inline const QString EWizardFieldLabelQuiet =
		"QLabel {"
		"color: rgba(255, 255, 255, 0.67);"
		"font-size: 14px;"
		"font-weight: 400;"
		"}";

	inline const QString EWizardTextField =
		"QLineEdit {"
		"border: 1px solid rgba(255, 255, 255, 0.12);"
		"border-radius: 8px;"
		"}";

	inline const QString EWizardComboBoxStyle = "QComboBox {"
		"background-color: #232323;"
		"border: none;"
		"padding: 6px 12px 6px 12px;"
		"font-size: 14px;"
		"border-radius: 8px;"
		"}";

	inline const QString EWizardButtonStyle =
		"QPushButton {"
		"background: #204CFE;"
		"border-radius: 8px;"
		"border: none;"
		"color: #FFFFFF;"
		"font-size: 14px;"
		"height: 32px;"
		"padding-left: 8px;"
		"padding-right: 8px;"
		"}"
		"QPushButton:hover {"
		"background: #193ED4;"
		"}"
		"QPushButton:pressed {"
		"background: #1231AC;"
		"}";

	inline const QString EWizardQuietButtonStyle =
		"QPushButton {"
		"background: rgba(255, 255, 255, 0.1);"
		"border-radius: 8px;"
		"border: none;"
		"color: #FFFFFF;"
		"font-size: 14px;"
		"height: 32px;"
		"padding-left: 8px;"
		"padding-right: 8px;"
		"}"
		"QPushButton:hover {"
		"background: rgba(255, 255, 255, 0.2);"
		"}"
		"QPushButton:pressed {"
		"background: rgba(255, 255, 255, 0.3);"
		"}";

	inline const QString EWizardSmallLabel =
		"QLabel {"
		"color: rgba(255, 255, 255, 0.67);"
		"font-size: 12px;"
		"font-weight: 400;"
		"}";

	inline const QString EWizardCheckBoxStyle =
		"QCheckBox {"
		"font-size: 14px;"
		"}"
		"QCheckBox::indicator {"
		"width: 16px;"
		"height: 16px;"
		"}"
		"QCheckBox::indicator:checked {"
		"image: url('${checked-img}');"
		"}"
		"QCheckBox::indicator:unchecked {"
		"image: url('${unchecked-img}');"
		"}";

	inline const QString EWizardChecklistStyle =
		"QListWidget {"
		"border: none;"
		"background: #151515;"
		"border-radius: 8px;"
		"font-size: 14px;"
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
		"width: 16px;"
		"height: 16px;"
		"}"
		"QListWidget::indicator:checked {"
		"background-image: url('${checked-img}');"
		"}"
		"QListWidget::indicator:unchecked {"
		"background-image: url('${unchecked-img}');"
		"}";

	inline const QString EWizardCheckBoxTipStyle =
		"QLabel {"
		"font-size: 14px;"
		"color: rgba(255, 255, 255, 0.67);"
		"font-weight: 400;"
		"margin-left: 20px;"
		"}";

	inline const QString EWizardIconOnlyButtonStyle =
		"QPushButton {"
		"background: transparent;"
		"background-repeat: no-repeat;"
		"border-radius: 8px;"
		"border: none;"
		"width: 32px;"
		"height: 32px;"
		"margin: 0px;"
		"border-image: url('${img}');"
		"background: #232323;"
		"}"
		"QPushButton:hover {"
		"background: rgba(255, 255, 255, 0.2);"
		"}"
		"QPushButton:pressed {"
		"background: rgba(255, 255, 255, 0.3);"
		"}"
		"QPushButton:disabled {"
		"background: none;"
		"border-image: url('${img-disabled}');"
		"}";
} // namespace elgatocloud
