#pragma once
#include <vector>
#include <string>

#include "scene-bundle.hpp"
#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QStackedWidget>


struct SDFileDetails {
	std::string path;
	std::string label;
};

class SdaListItemWidget : public QWidget {
	Q_OBJECT
public:
	explicit SdaListItemWidget(
				   SdaState const& state,
				   QWidget *parent = nullptr);

	QString description() const { return m_edit->text(); }
	void setDescription(const QString &text) { m_edit->setText(text); }
	QPushButton *deleteButton() const { return m_deleteButton; }

	QSize sizeHint() const override;
	QString sdaFilePath() const;

private:
	QLineEdit *m_edit;
	QPushButton *m_deleteButton;
	SdaState state_;
};

class SdaListWidget : public QWidget {
	Q_OBJECT
public:
	explicit SdaListWidget(QWidget *parent = nullptr);
	~SdaListWidget();

	// Programmatic add
	void addSdaFile(const QString &filePath);

	// Retrieve descriptions (user-edited text)
	QStringList descriptions() const;

protected:
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dropEvent(QDropEvent *event) override;

private:
	QListWidget *listWidget;
};

struct SdaFileInfo {
	QString path;
	QString label;
};

class SdaDropListItem : public QWidget
{
	Q_OBJECT
public:
	explicit SdaDropListItem(SdaState const &state,
					   QWidget *parent = nullptr);

	QString sdaFilePath() const;
	QString sdaLabel() const;

signals:
	void requestDelete(SdaDropListItem *self);

private:
	QLineEdit *edit_;
	QPushButton *deleteButton_;
	QLabel *iconLabel_;
	SdaState state_;
};

class SdaDropListContainer : public QScrollArea
{
	Q_OBJECT
public:
	explicit SdaDropListContainer(QWidget *parent = nullptr);

	void addSdaFile(const QString &filePath);
	void addItem(const SdaState &state);
	void removeItem(SdaDropListItem *row);

	std::vector<SdaFileInfo> sdaFiles() const;

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	QStackedWidget *stackedWidget_;
	QWidget *emptyWidget_;
	QWidget *itemsWidget_;

	QVBoxLayout *layout_;
};

class SdProfileDropListItem : public QWidget {
	Q_OBJECT
public:
	explicit SdProfileDropListItem(const SdProfileState &state,
				       QWidget *parent = nullptr);

	QString filePath() const;
	QString label() const;

signals:
	void requestDelete(SdProfileDropListItem *self);

private:
	SdProfileState state_;
	QString label_;
	QPushButton *deleteButton_;
	QLabel *iconLabel_;
};

class SdProfileDropListContainer : public QScrollArea {
	Q_OBJECT
public:
	explicit SdProfileDropListContainer(QWidget *parent = nullptr);

	void addSdProfileFile(const QString &filePath);
	void addItem(const SdProfileState &filePath);
	void removeItem(SdProfileDropListItem *row);

	std::vector<SdaFileInfo> sdProfileFiles() const;

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	QStackedWidget *stackedWidget_;
	QWidget *emptyWidget_;
	QWidget *itemsWidget_;

	QVBoxLayout *layout_;
};

class StreamDeckFilesDropContainer : public QStackedWidget
{
	Q_OBJECT
public:
	explicit StreamDeckFilesDropContainer(QWidget *parent = nullptr);
	inline std::vector<SdaFileInfo> sdaFiles() const
	{
		return sdaFiles_->sdaFiles();
	};
	inline std::vector<SdaFileInfo> sdProfileFiles() const
	{
		return profileFiles_->sdProfileFiles();
	};
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dropEvent(QDropEvent *event) override;

private:
	SdaDropListContainer *sdaFiles_;
	SdProfileDropListContainer *profileFiles_;
	QWidget *filesContainers_;
	QWidget *dropZone_;
	QVBoxLayout *containersLayout_;
	QVBoxLayout *dropZoneLayout_;
};

class StreamDeckProfilesInstallListItem : public QWidget
{
	Q_OBJECT
public:
	explicit StreamDeckProfilesInstallListItem(
		const SdProfileState &state,
		std::string label,
		bool disabled,
		QWidget *parent = nullptr);

signals:
	void requestInstall(std::string path);

private:
	SdProfileState state_;
	QLabel *label_;
	QPushButton *installButton_;
	QLabel *iconLabel_;
};

class StreamDeckProfilesInstallListContainer : public QScrollArea
{
	Q_OBJECT
public:
	explicit StreamDeckProfilesInstallListContainer(
		std::vector<SDFileDetails> const &profileFiles,
		bool disabled,
		QWidget *parent = nullptr);

private:
	QWidget *innerWidget_;
	QVBoxLayout *layout_;
};

struct LabeledSdaState {
	SdaState state;
	std::string label;
};

class SdaGridWidget : public QWidget {
	Q_OBJECT
public:
	explicit SdaGridWidget(bool disabled, QWidget *parent = nullptr);
	void setStates(std::vector<SDFileDetails> const &sdaFiles);
	int heightForWidth(int width) const;

	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

protected:
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *ev) override;
	void leaveEvent(QEvent *event) override;
	bool event(QEvent *e) override;

private:
	std::vector<LabeledSdaState> states_;
	int iconSize_ = 64;
	int minIconSize_ = 56;
	int maxIconSize_ = 80;
	int iconCornerRadius_ = 12;
	int padding_ = 12;
	bool disabled_;
	QString lastToolTip_;

	QPoint dragStartPos_;
	bool dragStarted_ = false;

	int indexAtPos(const QPoint &pos) const;
	int columnCount() const;
};

class SdaGridScrollArea : public QScrollArea {
	Q_OBJECT
public:
	explicit SdaGridScrollArea(
		std::vector<SDFileDetails> const &sdaFiles, 
		bool disabled,
		QWidget *parent = nullptr);

	void setStates(std::vector<SDFileDetails> const &sdaFiles);

	void resizeEvent(QResizeEvent *ev) override;

private:
	SdaGridWidget *gridWidget_;
};

class StreamDeckSetupWidget : public QWidget
{
	Q_OBJECT
public:
	explicit StreamDeckSetupWidget(
		std::vector<SDFileDetails> const &sdaFiles,
		std::vector<SDFileDetails> const &profileFiles,
		bool disabled,
		QWidget *parent = nullptr);
};