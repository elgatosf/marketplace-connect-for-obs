/*
Elgato Deep - Linking OBS Plug - In
Copyright(C) 2024 Corsair Memory Inc.oss.elgato@corsair.com

This program is free software; you can redistribute it and /or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program.If not, see < https://www.gnu.org/licenses/>
*/

#pragma once

#include <map>
#include <string>
#include <future>
#include <atomic>

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <obs.hpp>
#include <obs.h>

#include <QDialog>
#include <QWidget>
#include <QLabel>
#include <QComboBox>
#include <QStackedWidget>
#include <QMovie>
#include <QStyledItemDelegate>
#include <QFontMetrics>

#include "scene-bundle.hpp"
#include "elgato-widgets.hpp"
#include "elgato-stream-deck-widgets.hpp"


namespace elgatocloud {

class ExportStepsSideBar : public QWidget {
	Q_OBJECT
public:
	ExportStepsSideBar(std::string name, QWidget* parent);
	void setStep(int step);
	void incrementStep();
	void decrementStep();

private:
	Stepper* _stepper;
};

class StartExport : public QWidget {
	Q_OBJECT
public:
	StartExport(std::string name, QWidget* parent);
signals:
	void continuePressed();
};


class SceneCollectionFilesDelegate : public QStyledItemDelegate {
	Q_OBJECT
public:
	explicit SceneCollectionFilesDelegate(QObject* parent = nullptr);

	QString elideMiddlePath(const QString& fullPath, const QFontMetrics& metrics, int maxWidth) const;

	void paint(QPainter* painter, const QStyleOptionViewItem& option,
		const QModelIndex& index) const override;

	QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};


class FileCollectionCheck : public QWidget {
	Q_OBJECT

public:
	FileCollectionCheck(std::string name, std::vector<std::string> files, QWidget* parent);

signals:
	void continuePressed();
	void cancelPressed();

private:
	std::vector<std::string> _files;
	bool _SubFiles(std::vector<std::string>& files, std::string curDir);
};

class SelectOutputScenes : public QWidget {
	Q_OBJECT

public:
	SelectOutputScenes(std::string name, QWidget* parent = nullptr);
	std::vector<SceneInfo> OutputScenes() const;
	static bool AddScene(void* data, obs_source_t* scene);

signals:
	void continuePressed();
	void backPressed();

private:
	std::vector<SceneInfo> _scenes;
};

class VideoSourceLabels : public QWidget {
	Q_OBJECT
public:
	VideoSourceLabels(std::string name,
			  std::map<std::string, std::string> devices, QWidget* parent);
	inline std::map<std::string, std::string> Labels() { return _labels; }
signals:
	void continuePressed();
	void backPressed();

private:
	std::map<std::string, std::string> _labels;
};

class RequiredPlugins : public QWidget {
	Q_OBJECT
public:
	RequiredPlugins(std::string name,
			std::vector<obs_module_t *> installedPlugins, QWidget* parent);
	std::vector<std::string> RequiredPluginList();
signals:
	void continuePressed();
	void backPressed();

private:
	std::map<std::string, std::pair<bool, std::string>> _pluginStatus;
};

// --- Row-level focus watcher ---
class RowFocusWatcher : public QObject {
	Q_OBJECT
public:
	std::function<void()> onRowFocusLost;
	explicit RowFocusWatcher(QWidget *rowWidget, QObject *parent = nullptr);

protected:
	bool eventFilter(QObject *obj, QEvent *event) override;

private:
	bool anyChildHasFocus() const;

	QWidget *m_row;
};

class ThirdPartyRequirements : public QWidget {
	Q_OBJECT
public:
	ThirdPartyRequirements(std::string name, QWidget* parent);
	std::vector<std::pair<std::string, std::string>> getRequirements() const;

signals:
	void continuePressed();
	void backPressed();

private slots:
	void onTextChanged();
	void onDeleteClicked();

private:
	struct InputRow {
		QWidget* rowWidget;
		QLineEdit* titleEdit;
		QLineEdit* urlEdit;
		QPushButton* deleteButton;
		QLabel *errorLabel;
	};

	QVBoxLayout* _formLayout;
	QVector<InputRow> _inputRows;
	QPushButton *_continueButton = nullptr;
	QLabel *_errorLabel = nullptr;

	void addInputRow();
	void removeInputRow(QWidget* rowWidget);
	bool isLastRow(const InputRow& row) const;
	bool isRowFilled(const InputRow& row) const;
	void validateRowForUserFeedback(const InputRow &row);
	void updateDeleteButtonStates();

	void validate();
};

class Version : public QWidget {
	Q_OBJECT

public:
	explicit Version(std::string name, QWidget *parent = nullptr);

signals:
	void backPressed();
	void nextPressed(const QString &version);

private slots:
	void validateInput();
	void handleBackPressed();
	void handleNextPressed();

private:
	QLineEdit *_lineEdit;
	QPushButton *_backButton;
	QPushButton *_nextButton;
};

class StreamDeckButtons : public QWidget {
	Q_OBJECT

public:
	explicit StreamDeckButtons(std::string name, QWidget *parent = nullptr);
	inline std::vector<SdaFileInfo> sdaFiles() const {
		return _sdaList->sdaFiles();
	};
	inline std::vector<SdaFileInfo> sdProfileFiles() const {
		return _sdaList->sdProfileFiles();
	};

signals:
	void backPressed();
	void nextPressed();

private slots:
	void handleBackPressed();
	void handleNextPressed();

private:
	//SdaListWidget *_sdaList;
	StreamDeckFilesDropContainer *_sdaList;
	QPushButton *_backButton;
	QPushButton *_nextButton;
};


static bool isValidHttpUrl(const QString& urlStr);

class Exporting : public QWidget {
	Q_OBJECT
public:
	Exporting(std::string name, QWidget *parent);
	~Exporting();

	void updateProgress(double progress);
	void updateFileProgress(const QString &fileName, double progress);

private:
	QMovie *_indicator;
	ProgressSpinner *spinner_;
	QLabel *subTitle_;
};

class ExportComplete : public QWidget {
	Q_OBJECT

public:
	ExportComplete(std::string name, QWidget *parent);

signals:
	void closePressed();
};

class StreamPackageExportWizard : public QDialog {
	Q_OBJECT

public:
	StreamPackageExportWizard(QWidget *parent);
	~StreamPackageExportWizard();

	static void AddModule(void *data, obs_module_t *module);

	void emitOverallProgress(double progress);
	void emitFileProgress(const QString &filename, double progress);
	void emitCancelOperation();

public slots:
	void SetupUI();

signals:
	void overallProgress(double progress);
	void fileProgress(const QString &filename, double progress);
	void cancelOperation();

protected:
	void closeEvent(QCloseEvent *event) override;

private:
	QStackedWidget *_steps;
	SceneBundle _bundle;
	std::vector<obs_module_t *> _modules;
	//QFuture<void> _future;
	std::future<void> _future;
	std::atomic<bool> _canceled{false};
	bool _waiting;
};

void OpenExportWizard();

} // namespace elgatocloud
