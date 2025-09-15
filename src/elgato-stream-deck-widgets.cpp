#include "elgato-stream-deck-widgets.hpp"
#include "elgato-styles.hpp"
#include <QVBoxLayout>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QLabel>
#include <QPainter>
#include <QPaintDevice>
#include <QPainterPath>
#include <QList>
#include <QDesktopServices>
#include <QUrl>
#include <QApplication>
#include <QDrag>
#include <QButtonGroup>
#include <QImage>
#include <QSvgRenderer>

#include "plugin-support.h"
#include "obs-utils.hpp"

const std::map<std::string, std::string> modelMap{
	{"20GAA9901", "Stream Deck"},
	{"20GAA9902", "Stream Deck"},
	{"20GBA9901", "Stream Deck"},
	{"20GBL9901", "Stream Deck"},
	{"20GAI9901", "Stream Deck Mini"},
	{"20GAI9902", "Stream Deck Mini"},
	{"20GAT9901", "Stream Deck XL"},
	{"20GAT9902", "Stream Deck XL"},
	{"20GBF9901", "Stream Deck Pedal"},
	{"20GBD9901", "Stream Deck +"},
	{"20GBX9901", "Stream Deck + XL"},
	{"20GBJ9901", "Stream Deck Neo"},
	{"20GBO9901", "Stream Deck Studio"},
	{"VSD/WiFi", "Stream Deck Mobile"},
	{"VSD2/WiFi", "Stream Deck Mobile"},
	{"Corsair G4 Keyboard", "Corsair G-Keys"},
	{"Corsair G6 Keyboard", "Corsair G-Keys"},
	{"Corsair Bishop", "Corsair Voyager"},
	{"Corsair ScimitarV2 Mice", "Corsair Scimitar Elite"},
	{"SCUF G5 Gamepad", "Scuf Envision"}};


QPixmap createIconPixmap(SdaState const &state, int size = 64, int cornerRadius = 4)
{
	constexpr int padding = 0;               // padding on all sides
	constexpr double lineHeightScale = 0.85; // shrink line height to 85%

	QString title = state.hasTitle ? state.title : "";
	SdaIconVerticalAlign align = state.titleAlign;

	QByteArray imageData = state.hasImage ? state.imageBytes : QByteArray();

	// Device pixel ratio for HiDPI
	qreal dpr = QGuiApplication::primaryScreen()->devicePixelRatio();
	int pixSize = static_cast<int>(size * dpr);

	QPixmap pix(pixSize, pixSize);
	pix.setDevicePixelRatio(dpr);
	pix.fill(Qt::transparent);

	QPainter painter(&pix);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::TextAntialiasing);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);

	// Rounded clipping
	QPainterPath path;
	path.addRoundedRect(0, 0, size, size, cornerRadius, cornerRadius);
	painter.setClipPath(path);

	// Draw the image if available
	if (!imageData.isEmpty()) {
		QPixmap img;
		img.loadFromData(imageData);
		if (!img.isNull()) {
			img = img.scaled(size, size,
					 Qt::KeepAspectRatioByExpanding,
					 Qt::SmoothTransformation);
			painter.drawPixmap(0, 0, size, size, img);
		}
	}

	// Draw title text
	if (!title.isEmpty()) {
		QFont font = painter.font();
		font.setBold(true);
		painter.setFont(font);

		QStringList lines = title.split('\n');

		// Fit font width
		QFontMetrics fm(font);
		int maxLineWidth = 0;
		int availableWidth = size - 2 * padding;
		for (const QString &line : lines) {
			maxLineWidth = std::max(maxLineWidth,
						fm.horizontalAdvance(line));
		}
		while (maxLineWidth > availableWidth && font.pointSize() > 1) {
			font.setPointSize(font.pointSize() - 1);
			painter.setFont(font);
			fm = QFontMetrics(font);
			maxLineWidth = 0;
			for (const QString &line : lines) {
				maxLineWidth =
					std::max(maxLineWidth,
						 fm.horizontalAdvance(line));
			}
		}

		painter.setFont(font);
		int lineHeight =
			static_cast<int>(fm.height() * lineHeightScale);
		int totalHeight = lineHeight * lines.size();
		int availableHeight = size - 2 * padding;

		int yOffset = 0;
		switch (align) {
		case SdaIconVerticalAlign::Top:
			yOffset = padding;
			break;
		case SdaIconVerticalAlign::Middle:
			yOffset = padding + (availableHeight - totalHeight) / 2;
			break;
		case SdaIconVerticalAlign::Bottom:
			yOffset = size - padding - totalHeight;
			break;
		}

		for (int i = 0; i < lines.size(); ++i) {
			QRect lineRect(padding, yOffset + i * lineHeight,
				       size - 2 * padding, lineHeight);

			// Outline
			painter.setPen(Qt::black);
			for (int dx = -1; dx <= 1; ++dx) {
				for (int dy = -1; dy <= 1; ++dy) {
					if (dx == 0 && dy == 0)
						continue;
					QRect offsetRect =
						lineRect.translated(dx, dy);
					painter.drawText(
						offsetRect,
						Qt::AlignHCenter |
							Qt::AlignVCenter,
						lines[i]);
				}
			}

			// Main text
			painter.setPen(Qt::white);
			painter.drawText(lineRect,
					 Qt::AlignHCenter | Qt::AlignVCenter,
					 lines[i]);
		}
	}

	return pix;
}

//QPixmap createIconPixmap(SdaState const &state, int size = 64)
//{
//	constexpr int padding = 4;               // padding on all sides
//	constexpr double lineHeightScale = 0.85; // shrink line height to 85%
//	constexpr int cornerRadius = 8;          // rounded corners
//
//	QString title = state.hasTitle ? state.title : "";
//	SdaIconVerticalAlign align = state.titleAlign;
//
//	QByteArray imageData = state.hasImage ? state.imageBytes : QByteArray();
//
//	// Get device pixel ratio
//	qreal dpr = QGuiApplication::primaryScreen()->devicePixelRatio();
//
//	int scaledSize = static_cast<int>(size * dpr);
//	int scaledPadding = static_cast<int>(padding * dpr);
//	int scaledCorner = static_cast<int>(cornerRadius * dpr);
//
//	QPixmap pix(scaledSize, scaledSize);
//	pix.setDevicePixelRatio(dpr); // Important for HiDPI
//	pix.fill(Qt::transparent);
//
//	QPainter painter(&pix);
//	painter.setRenderHint(QPainter::Antialiasing);
//	painter.setRenderHint(QPainter::TextAntialiasing);
//
//	// Apply rounded clipping
//	QPainterPath path;
//	path.addRoundedRect(0, 0, scaledSize, scaledSize, scaledCorner,
//			    scaledCorner);
//	painter.setClipPath(path);
//
//	// Draw image if available
//	if (!imageData.isEmpty()) {
//		QPixmap img;
//		img.loadFromData(imageData);
//		if (!img.isNull()) {
//			img = img.scaled(scaledSize, scaledSize,
//					 Qt::KeepAspectRatioByExpanding,
//					 Qt::SmoothTransformation);
//
//			QRect target(0, 0, scaledSize, scaledSize);
//			painter.drawPixmap(target, img);
//		}
//	}
//
//	// Draw title over image
//	if (!title.isEmpty()) {
//		QFont font = painter.font();
//		font.setBold(true);
//
//		QStringList lines = title.split('\n');
//
//		// Shrink font to fit width
//		QFontMetrics fm(font);
//		int maxLineWidth = 0;
//		int availableWidth = scaledSize - 2 * scaledPadding;
//		for (const QString &line : lines)
//			maxLineWidth = std::max(maxLineWidth,
//						fm.horizontalAdvance(line));
//
//		while (maxLineWidth > availableWidth && font.pointSize() > 1) {
//			font.setPointSize(font.pointSize() - 1);
//			painter.setFont(font);
//			fm = QFontMetrics(font);
//			maxLineWidth = 0;
//			for (const QString &line : lines)
//				maxLineWidth =
//					std::max(maxLineWidth,
//						 fm.horizontalAdvance(line));
//		}
//
//		painter.setFont(font);
//
//		int lineHeight =
//			static_cast<int>(fm.height() * lineHeightScale);
//		int totalHeight = lineHeight * lines.size();
//		int availableHeight = scaledSize - 2 * scaledPadding;
//
//		int yOffset = 0;
//		switch (align) {
//		case SdaIconVerticalAlign::Top:
//			yOffset = scaledPadding;
//			break;
//		case SdaIconVerticalAlign::Middle:
//			yOffset = scaledPadding +
//				  (availableHeight - totalHeight) / 2;
//			break;
//		case SdaIconVerticalAlign::Bottom:
//			yOffset = scaledSize - scaledPadding - totalHeight;
//			break;
//		}
//
//		// Draw each line with outline
//		for (int i = 0; i < lines.size(); ++i) {
//			QRect lineRect(scaledPadding, yOffset + i * lineHeight,
//				       scaledSize - 2 * scaledPadding,
//				       lineHeight);
//
//			// Outline
//			painter.setPen(Qt::black);
//			for (int dx = -1; dx <= 1; ++dx) {
//				for (int dy = -1; dy <= 1; ++dy) {
//					if (dx == 0 && dy == 0)
//						continue;
//					QRect offsetRect =
//						lineRect.translated(dx, dy);
//					painter.drawText(
//						offsetRect,
//						Qt::AlignHCenter |
//							Qt::AlignVCenter,
//						lines[i]);
//				}
//			}
//
//			// Main text
//			painter.setPen(Qt::white);
//			painter.drawText(lineRect,
//					 Qt::AlignHCenter | Qt::AlignVCenter,
//					 lines[i]);
//		}
//	}
//
//	painter.end();
//	return pix;
//}

SdaListItemWidget::SdaListItemWidget(SdaState const &state, QWidget *parent)
	: QWidget(parent), state_(state)
{
	auto *layout = new QHBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(6);

	QString title = state.hasTitle ? state.title : "";

	// Icon
	QLabel *iconLabel = new QLabel(this);
	QPixmap pixmap = createIconPixmap(state, 64);
	iconLabel->setPixmap(pixmap);
	iconLabel->setFixedSize(64, 64);
	iconLabel->setScaledContents(false);
	iconLabel->setSizePolicy(QSizePolicy::Fixed,
				 QSizePolicy::Fixed); // lock to 64x64
	layout->addWidget(iconLabel);

	// Editable title
	QLineEdit *edit = new QLineEdit(this);
	edit->setText(title);
	edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	layout->addWidget(edit, 1);

	// Delete button
	QPushButton *delButton = new QPushButton("Delete", this);
	delButton->setFixedSize(24, 24);
	layout->addWidget(delButton);

	setLayout(layout);

	m_edit = edit;
	m_deleteButton = delButton;
}

QSize SdaListItemWidget::sizeHint() const
{
	// row height = icon height + padding
	return QSize(20 + 120 + 30, 20 + 8);
}

QString SdaListItemWidget::sdaFilePath() const
{
	return state_.path;
}

SdaListWidget::SdaListWidget(QWidget *parent)
	: QWidget(parent),
	  listWidget(new QListWidget(this))
{
	setAcceptDrops(true);
	listWidget->setViewMode(QListView::ListMode);
	listWidget->setSpacing(4);
	listWidget->setResizeMode(QListView::Adjust);
	listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
	listWidget->setDragEnabled(true);
	listWidget->setAcceptDrops(false); // only parent handles drops
	listWidget->setUniformItemSizes(false); // let custom widgets size properly

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(listWidget);
	setLayout(layout);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	listWidget->setSizePolicy(QSizePolicy::Expanding,
				  QSizePolicy::Expanding);
}

SdaListWidget::~SdaListWidget()
{
	// Cleanup temp files
	for (int i = 0; i < listWidget->count(); ++i) {
		QListWidgetItem *item = listWidget->item(i);
		QString tempPath = item->data(Qt::UserRole).toString();
		if (!tempPath.isEmpty()) {
			QFile::remove(tempPath);
		}
	}
}

void SdaListWidget::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasUrls()) {
		for (const QUrl &url : event->mimeData()->urls()) {
			if (url.isLocalFile() &&
			    url.toLocalFile().endsWith(".streamDeckAction",
						       Qt::CaseInsensitive)) {
				event->acceptProposedAction();
				return;
			}
		}
	}
}

void SdaListWidget::dropEvent(QDropEvent *event)
{
	for (const QUrl &url : event->mimeData()->urls()) {
		addSdaFile(url.toLocalFile());
	}
}

void SdaListWidget::addSdaFile(const QString &filePath)
{
	if (!filePath.endsWith(".streamDeckAction", Qt::CaseInsensitive))
		return;

	// Copy to temp location
	QString tempPath =
		QDir::temp().filePath(QFileInfo(filePath).fileName());

	for (int row = 0; row < listWidget->count(); ++row) {
		QListWidgetItem *item = listWidget->item(row);
		auto widget = dynamic_cast<SdaListItemWidget *>(listWidget->itemWidget(item));
		if (widget->sdaFilePath() == tempPath) {
			qDebug() << "Skipping duplicate:" << tempPath;
			return;
		}
	}

	QFile::copy(filePath, tempPath);

	// Parse SDA
	SdaFile sda(tempPath);
	if (!sda.isValid()) {
		qWarning() << "Invalid SDA file:" << filePath;
		QFile::remove(tempPath);
		return;
	}

	// Check if a valid state was found
	auto state = sda.firstState();
	if (!state) {
		qWarning() << "No valid state found in SDA file:" << filePath;
		return;
	}

	auto *item = new QListWidgetItem(listWidget);
	auto *widget = new SdaListItemWidget(state.value());
	auto size = widget->size();
	auto sizeHint = widget->sizeHint();
	item->setSizeHint(widget->sizeHint());

	listWidget->setItemWidget(item, widget);
	item->setData(Qt::UserRole, tempPath);

	connect(widget->deleteButton(), &QPushButton::clicked, this, [=]() {
		QString tempPath = item->data(Qt::UserRole).toString();
		if (!tempPath.isEmpty()) {
			QFile::remove(tempPath); // delete temp file
		}
		delete listWidget->takeItem(listWidget->row(item));
	});
}

QStringList SdaListWidget::descriptions() const
{
	QStringList result;
	for (int i = 0; i < listWidget->count(); ++i) {
		QListWidgetItem *item = listWidget->item(i);
		QWidget *w = listWidget->itemWidget(item);
		auto *sdaWidget = qobject_cast<SdaListItemWidget *>(w);
		if (sdaWidget) {
			result << sdaWidget->description();
		}
	}
	return result;
}

SdaDropListItem::SdaDropListItem(const SdaState& state, QWidget* parent)
	: QWidget(parent), state_(state)
{
	std::string imageBaseDir = GetDataPath();
	imageBaseDir += "/images/";

	auto *layout = new QHBoxLayout(this);
	layout->setContentsMargins(4, 2, 4, 2);
	layout->setSpacing(6);
	setAttribute(Qt::WA_StyledBackground, true);
	setAutoFillBackground(true);
	setObjectName("SdaDropListItem");
	setStyleSheet("#SdaDropListItem {\
		background-color: rgba(255, 255, 255, 15);	\
		border-radius: 8px; \
	}");

	auto stylesheet = styleSheet();

	iconLabel_ = new QLabel(this);
	QPixmap pixmap = createIconPixmap(state, 28);
	iconLabel_->setPixmap(pixmap);
	iconLabel_->setFixedSize(28, 28);
	iconLabel_->setScaledContents(false);
	layout->addWidget(iconLabel_);

	edit_ = new QLineEdit(this);
	edit_->setText(state.hasTitle ? state.title : "");
	layout->addWidget(edit_, 1);

    // --- Delete button ---
	deleteButton_ = new QPushButton(this);

	std::string trashImgPath = imageBaseDir + "IconTrash.svg";
	QSvgRenderer renderer(QString::fromStdString(trashImgPath));

	// Desired icon height
	int targetHeight = 22;

	// Compute width to maintain aspect ratio
	QSize originalSize = renderer.defaultSize();
	int targetWidth =
		originalSize.width() * targetHeight / originalSize.height();

	// Render SVG to QPixmap
	QPixmap trashPixmap(targetWidth, targetHeight);
	trashPixmap.fill(Qt::transparent);
	QPainter painter(&trashPixmap);
	renderer.render(&painter);

	// Apply pixmap to button
	deleteButton_->setIcon(trashPixmap);
	deleteButton_->setIconSize(trashPixmap.size());

	// Make button just the icon
	deleteButton_->setFlat(true);
	deleteButton_->setStyleSheet(
		"QPushButton { border: none; background: transparent; padding: 0; }"
		"QPushButton:pressed { background: transparent; }");
	deleteButton_->setFocusPolicy(Qt::NoFocus);
	deleteButton_->setFixedSize(trashPixmap.size());

	connect(deleteButton_, &QPushButton::clicked, this,
		[this]() { emit requestDelete(this); });

	layout->addWidget(deleteButton_);
}

QString SdaDropListItem::sdaFilePath() const
{
	return state_.path;
}

QString SdaDropListItem::sdaLabel() const
{
	return edit_->text();
}

SdaDropListContainer::SdaDropListContainer(QWidget* parent)
	: QScrollArea(parent)
{
	std::string imageBaseDir = GetDataPath();
	imageBaseDir += "/images/";

	setWidgetResizable(true);
	setFrameShape(QFrame::NoFrame);
	setAttribute(Qt::WA_StyledBackground, true);

	// Make viewport transparent so border is visible
	viewport()->setAttribute(Qt::WA_StyledBackground, true);
	viewport()->setStyleSheet("background: transparent;");

	// --- Stacked widget ---
	stackedWidget_ = new QStackedWidget(viewport());

	// Empty state widget
	emptyWidget_ = new QWidget;
	QVBoxLayout *emptyLayout = new QVBoxLayout(emptyWidget_);
	emptyLayout->setContentsMargins(0, 0, 0, 0);
	emptyLayout->setAlignment(Qt::AlignCenter);

	QLabel *placeholder = new QLabel(emptyWidget_);
	std::string dropImgPath = imageBaseDir + "IconUpload.svg";
	QPixmap dropPixmap = QPixmap(dropImgPath.c_str());
	placeholder->setPixmap(dropPixmap);
	placeholder->setAlignment(Qt::AlignCenter);
	emptyLayout->addWidget(placeholder);

	stackedWidget_->addWidget(emptyWidget_);

	// Items container
	itemsWidget_ = new QWidget;
	itemsWidget_->setAttribute(Qt::WA_StyledBackground, true);
	itemsWidget_->setStyleSheet("background: transparent;");

	layout_ = new QVBoxLayout(itemsWidget_);
	layout_->setContentsMargins(12, 12, 12, 12); // leave space for rounded border
	layout_->setSpacing(4);
	layout_->addStretch(); // bottom stretch
	stackedWidget_->addWidget(itemsWidget_);

	// Show empty state initially
	stackedWidget_->setCurrentWidget(emptyWidget_);

	// Set the stacked widget as the scroll area widget
	setWidget(stackedWidget_);
}

std::vector<SdaFileInfo> SdaDropListContainer::sdaFiles() const
{
	std::vector<SdaFileInfo> sdaFileList;

	//QList<SdaDropListItem *> rows =
	//	innerWidget_->findChildren<SdaDropListItem *>();

	QList<SdaDropListItem *> rows =
		itemsWidget_->findChildren<SdaDropListItem *>();

	for (SdaDropListItem *row : rows) {
		sdaFileList.push_back({ 
			row->sdaFilePath(),
			row->sdaLabel()
		});
	}
	return sdaFileList;
}

void SdaDropListContainer::addItem(const SdaState &state)
{
	auto *row = new SdaDropListItem(state, itemsWidget_);

	// Connect delete signal
	connect(row, &SdaDropListItem::requestDelete, this,
		[this, row]() { removeItem(row); });

	// Insert before the stretch
	layout_->insertWidget(layout_->count() - 1, row);

	// Switch to items view
	stackedWidget_->setCurrentWidget(itemsWidget_);
}


void SdaDropListContainer::removeItem(SdaDropListItem* row)
{
	layout_->removeWidget(row);
	row->deleteLater();

	// If no items left, show empty view
	if (itemsWidget_->findChildren<SdaDropListItem *>().empty()) {
		stackedWidget_->setCurrentWidget(emptyWidget_);
	}
}

void SdaDropListContainer::addSdaFile(const QString &filePath)
{
	QList<SdaDropListItem *> rows =
		itemsWidget_->findChildren<SdaDropListItem *>();

	for (SdaDropListItem *row : rows) {
		if (row->sdaFilePath() == filePath) {
			qDebug() << "Skipping duplicate:" << filePath;
			return;
		}
	}
	// Parse SDA
	SdaFile sda(filePath);
	if (!sda.isValid()) {
		qWarning() << "Invalid SDA file:" << filePath;
		QFile::remove(filePath);
		return;
	}

	// Get first state
	auto state = sda.firstState();
	if (!state) {
		qWarning() << "No valid state found in SDA file:" << filePath;
		QFile::remove(filePath);
		return;
	}

	addItem(state.value());
}

void SdaDropListContainer::paintEvent(QPaintEvent *event)
{
	QScrollArea::paintEvent(event);

	QPainter p(this->viewport());
	p.setRenderHint(QPainter::Antialiasing);

	QPen pen(QColor(255, 255, 255, 67)); // semi-transparent gray
	pen.setWidthF(1.0);                // thin but visible
	pen.setStyle(Qt::DashLine);
	p.setPen(pen);

	QRectF r = rect();
	r.adjust(1, 1, -2, -2); // padding inside edges
	p.drawRoundedRect(r, 16, 16);
}


SdProfileDropListItem::SdProfileDropListItem(const SdProfileState &state,
					    QWidget *parent)
	: QWidget(parent),
	  state_(state)
{
	std::string imageBaseDir = GetDataPath();
	imageBaseDir += "/images/";

	auto *layout = new QHBoxLayout(this);
	layout->setContentsMargins(4, 2, 4, 2);
	layout->setSpacing(6);
	setAttribute(Qt::WA_StyledBackground, true);
	setAutoFillBackground(true);
	setObjectName("SdProfileDropListItem");
	setStyleSheet("#SdProfileDropListItem {\
		background-color: rgba(255, 255, 255, 15);	\
		border-radius: 8px; \
	}");

	QString profileImgPath = QString(imageBaseDir.c_str()) + "IconProfile.svg";
	int profileTargetHeight = 24; // desired height

	// Load the SVG
	QSvgRenderer profileRenderer(profileImgPath);

	// Compute width to maintain aspect ratio
	QSize profileOriginalSize = profileRenderer.defaultSize();
	int profileTargetWidth =
		profileOriginalSize.width() * profileTargetHeight / profileOriginalSize.height();

	// Create a pixmap at the target size
	QPixmap pixmap(profileTargetWidth, profileTargetHeight);
	pixmap.fill(Qt::transparent); // transparent background

	// Render SVG onto pixmap
	QPainter profilePainter(&pixmap);
	profileRenderer.render(&profilePainter);

	iconLabel_ = new QLabel(this);
	iconLabel_->setPixmap(pixmap);
	iconLabel_->setFixedSize(pixmap.size());
	iconLabel_->setScaledContents(false);
	layout->addWidget(iconLabel_);

	std::string modelStr = state.model.toStdString();
	std::string model = modelMap.count(modelStr) > 0 ? modelMap.at(modelStr)
							 : modelStr;
	std::string name = state.name.toStdString() + " (" + model + ")";
	label_ = name.c_str();
	QLabel *label = new QLabel(label_, this);
	layout->addWidget(label);

    // --- Delete button ---
	deleteButton_ = new QPushButton(this);

	std::string trashImgPath = imageBaseDir + "IconTrash.svg";
	QSvgRenderer renderer(QString::fromStdString(trashImgPath));

	// Desired icon height
	int targetHeight = 22;

	// Compute width to maintain aspect ratio
	QSize originalSize = renderer.defaultSize();
	int targetWidth =
		originalSize.width() * targetHeight / originalSize.height();

	// Render SVG to QPixmap
	QPixmap trashPixmap(targetWidth, targetHeight);
	trashPixmap.fill(Qt::transparent);
	QPainter painter(&trashPixmap);
	renderer.render(&painter);

	// Apply pixmap to button
	deleteButton_->setIcon(trashPixmap);
	deleteButton_->setIconSize(trashPixmap.size());

	// Make button just the icon
	deleteButton_->setFlat(true);
	deleteButton_->setStyleSheet(
		"QPushButton { border: none; background: transparent; padding: 0; }"
		"QPushButton:pressed { background: transparent; }");
	deleteButton_->setFocusPolicy(Qt::NoFocus);
	deleteButton_->setFixedSize(trashPixmap.size());

	connect(deleteButton_, &QPushButton::clicked, this,
		[this]() { emit requestDelete(this); });

	layout->addWidget(deleteButton_);
}

QString SdProfileDropListItem::filePath() const
{
	return state_.path;
}

QString SdProfileDropListItem::label() const
{
	return label_;
}

SdProfileDropListContainer::SdProfileDropListContainer(QWidget *parent)
	: QScrollArea(parent)
{
	std::string imageBaseDir = GetDataPath();
	imageBaseDir += "/images/";

	setWidgetResizable(true);
	setFrameShape(QFrame::NoFrame);
	setAttribute(Qt::WA_StyledBackground, true);

	// Make viewport transparent so border is visible
	viewport()->setAttribute(Qt::WA_StyledBackground, true);
	viewport()->setStyleSheet("background: transparent;");

	// --- Stacked widget ---
	stackedWidget_ = new QStackedWidget(viewport());

	// Empty state widget
	emptyWidget_ = new QWidget;
	QVBoxLayout *emptyLayout = new QVBoxLayout(emptyWidget_);
	emptyLayout->setContentsMargins(0, 0, 0, 0);
	emptyLayout->setAlignment(Qt::AlignCenter);

	QLabel *placeholder = new QLabel(emptyWidget_);
	std::string dropImgPath = imageBaseDir + "IconUpload.svg";
	QPixmap dropPixmap = QPixmap(dropImgPath.c_str());
	placeholder->setPixmap(dropPixmap);
	placeholder->setAlignment(Qt::AlignCenter);
	emptyLayout->addWidget(placeholder);

	stackedWidget_->addWidget(emptyWidget_);

	// Items container
	itemsWidget_ = new QWidget;
	itemsWidget_->setAttribute(Qt::WA_StyledBackground, true);
	itemsWidget_->setStyleSheet("background: transparent;");

	layout_ = new QVBoxLayout(itemsWidget_);
	layout_->setContentsMargins(12, 12, 12,
				    12); // leave space for rounded border
	layout_->setSpacing(4);
	layout_->addStretch(); // bottom stretch
	stackedWidget_->addWidget(itemsWidget_);

	// Show empty state initially
	stackedWidget_->setCurrentWidget(emptyWidget_);

	// Set the stacked widget as the scroll area widget
	setWidget(stackedWidget_);

}

void SdProfileDropListContainer::addSdProfileFile(const QString &filePath)
{
	QList<SdProfileDropListItem *> rows =
		itemsWidget_->findChildren<SdProfileDropListItem *>();

	for (SdProfileDropListItem *row : rows) {
		if (row->filePath() == filePath) {
			qDebug() << "Skipping duplicate:" << filePath;
			return;
		}
	}
	// Parse SDA
	SdProfileFile sdp(filePath);
	if (!sdp.isValid()) {
		qWarning() << "Invalid SD Profile file:" << filePath;
		QFile::remove(filePath);
		return;
	}

	auto state = sdp.state();

	addItem(state);
}

void SdProfileDropListContainer::addItem(const SdProfileState &state)
{
	auto *row = new SdProfileDropListItem(state, itemsWidget_);

	// Connect delete signal
	connect(row, &SdProfileDropListItem::requestDelete, this,
		[this, row]() { removeItem(row); });

	// Insert before the stretch
	layout_->insertWidget(layout_->count() - 1, row);

	// Switch to items view
	stackedWidget_->setCurrentWidget(itemsWidget_);
}

void SdProfileDropListContainer::removeItem(SdProfileDropListItem *row)
{
	layout_->removeWidget(row);
	row->deleteLater();

	// If no items left, show empty view
	if (itemsWidget_->findChildren<SdProfileDropListItem *>().empty()) {
		stackedWidget_->setCurrentWidget(emptyWidget_);
	}
}

std::vector<SdaFileInfo> SdProfileDropListContainer::sdProfileFiles() const
{
	std::vector<SdaFileInfo> sdProfileFileList;

	QList<SdProfileDropListItem *> rows =
		itemsWidget_->findChildren<SdProfileDropListItem *>();

	for (SdProfileDropListItem *row : rows) {
		sdProfileFileList.push_back({row->filePath(), row->label()});
	}
	return sdProfileFileList;
}

void SdProfileDropListContainer::paintEvent(QPaintEvent *event)
{
	QScrollArea::paintEvent(event);

	QPainter p(this->viewport());
	p.setRenderHint(QPainter::Antialiasing);

	QPen pen(QColor(255, 255, 255, 67)); // semi-transparent gray
	pen.setWidthF(1.0);                  // thin but visible
	pen.setStyle(Qt::DashLine);
	p.setPen(pen);

	QRectF r = rect();
	r.adjust(1, 1, -2, -2); // padding inside edges
	p.drawRoundedRect(r, 16, 16);
}

StreamDeckFilesDropContainer::StreamDeckFilesDropContainer(QWidget* parent)
	: QStackedWidget(parent)
{
	setAcceptDrops(true);

	// Set up "Drop file here" panel
	dropZone_ = new QWidget(this);
	auto dropZoneLabel = new QLabel("Drop Zone", dropZone_);
	dropZoneLayout_ = new QVBoxLayout(dropZone_);
	dropZoneLayout_->addStretch();
	dropZoneLayout_->addWidget(dropZoneLabel);
	dropZoneLayout_->addStretch();

	// Set up lists
	filesContainers_ = new QWidget(this);
	auto profilesLabel = new QLabel(obs_module_text("ExportWizard.StreamDeckButtons.ProfilesTitle"), filesContainers_);
	profilesLabel->setStyleSheet(
		"QLabel { font-size: 14px; margin-top: 16px; }");
	profileFiles_ = new SdProfileDropListContainer(filesContainers_);
	profileFiles_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	auto profilesAccepted = new QLabel(
		obs_module_text("ExportWizard.StreamDeckButtons.ProfilesAccepted"),
		filesContainers_);
	profilesAccepted->setStyleSheet(
		"QLabel {font-size: 12px; color: #A8A8A8; }");
	
	auto sdasLabel = new QLabel(
		obs_module_text("ExportWizard.StreamDeckButtons.ActionsTitle"),
		filesContainers_);
	sdasLabel->setStyleSheet(
		"QLabel { font-size: 14px; margin-top: 16px; }");
	sdaFiles_ = new SdaDropListContainer(filesContainers_);
	sdaFiles_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	auto actionsAccepted = new QLabel(
		obs_module_text(
			"ExportWizard.StreamDeckButtons.ActionsAccepted"),
		filesContainers_);
	actionsAccepted->setStyleSheet(
		"QLabel {font-size: 12px; color: #A8A8A8; }");

	containersLayout_ = new QVBoxLayout(filesContainers_);
	containersLayout_->addWidget(profilesLabel);
	containersLayout_->addWidget(profileFiles_);
	containersLayout_->addWidget(profilesAccepted);

	containersLayout_->addWidget(sdasLabel);
	containersLayout_->addWidget(sdaFiles_);
	containersLayout_->addWidget(actionsAccepted);

	containersLayout_->setContentsMargins(0, 0, 0, 0);

	addWidget(dropZone_);
	addWidget(filesContainers_);

	setCurrentIndex(1);
}

void StreamDeckFilesDropContainer::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasUrls()) {
		for (const QUrl &url : event->mimeData()->urls()) {
			if (url.isLocalFile() &&
			    (url.toLocalFile().endsWith(".streamDeckAction", Qt::CaseInsensitive) ||
				url.toLocalFile().endsWith(".streamDeckProfile", Qt::CaseInsensitive))) {
				event->acceptProposedAction();
				return;
			}
		}
	}
}

void StreamDeckFilesDropContainer::dropEvent(QDropEvent *event)
{
	for (const QUrl &url : event->mimeData()->urls()) {
		QString filePath = url.toLocalFile();
		setCurrentIndex(1);
		if (filePath.endsWith(".streamDeckAction", Qt::CaseInsensitive)) {
			sdaFiles_->addSdaFile(filePath);
		} else if (filePath.endsWith(".streamDeckProfile", Qt::CaseInsensitive)) {
			profileFiles_->addSdProfileFile(filePath);
		}
	}
}

StreamDeckProfilesInstallListItem::StreamDeckProfilesInstallListItem(
	const SdProfileState &state, std::string label, QWidget* parent)
	: QWidget(parent), state_(state)
{
	std::string imageBaseDir = GetDataPath();
	imageBaseDir += "/images/";

	auto *layout = new QHBoxLayout(this);
	layout->setContentsMargins(4, 2, 4, 2);
	layout->setSpacing(6);
	setAttribute(Qt::WA_StyledBackground, true);
	setAutoFillBackground(true);
	setObjectName("StreamDeckProfilesInstallListItem");
	setStyleSheet("#StreamDeckProfilesInstallListItem {\
		background-color: rgba(255, 255, 255, 15);	\
		border-radius: 8px; \
	}");

	QString profileImgPath =
		QString(imageBaseDir.c_str()) + "IconProfile.svg";
	int profileTargetHeight = 24; // desired height

	// Load the SVG
	QSvgRenderer profileRenderer(profileImgPath);

	// Compute width to maintain aspect ratio
	QSize profileOriginalSize = profileRenderer.defaultSize();
	int profileTargetWidth = profileOriginalSize.width() *
				 profileTargetHeight /
				 profileOriginalSize.height();

	// Create a pixmap at the target size
	QPixmap pixmap(profileTargetWidth, profileTargetHeight);
	pixmap.fill(Qt::transparent); // transparent background

	// Render SVG onto pixmap
	QPainter profilePainter(&pixmap);
	profileRenderer.render(&profilePainter);

	iconLabel_ = new QLabel(this);
	iconLabel_->setPixmap(pixmap);
	iconLabel_->setFixedSize(pixmap.size());
	iconLabel_->setScaledContents(false);
	iconLabel_->setStyleSheet(
		"QLabel { background: none; }");
	layout->addWidget(iconLabel_);

	label_ = new QLabel(label.c_str(), this);
	label_->setStyleSheet("QLabel { background: none; }");
	layout->addWidget(label_, 1);

	installButton_ = new QPushButton(obs_module_text("SceneCollectionInfo.StreamDeck.AddToStreamDeckButton"), this);
	installButton_->setStyleSheet(elgatocloud::EWizardQuietButtonStyle);
	
	connect(installButton_, &QPushButton::clicked, this,
		[this, state]() { 
			std::string path = state.path.toStdString();
			emit requestInstall(path);
		});

	layout->addWidget(installButton_);
}

StreamDeckProfilesInstallListContainer::StreamDeckProfilesInstallListContainer(
	std::vector<SDFileDetails> const& profileFiles, QWidget* parent)
	: QScrollArea(parent)
{
	setWidgetResizable(true);

	innerWidget_ = new QWidget(this);
	layout_ = new QVBoxLayout(innerWidget_);
	layout_->setContentsMargins(4, 4, 4, 4);
	layout_->setSpacing(4);
	layout_->addStretch();

	innerWidget_->setLayout(layout_);
	setWidget(innerWidget_);

	setObjectName("StreamDeckProfilesInstallListContainer");
	setStyleSheet("#StreamDeckProfilesInstallListContainer { border: none; }");

	for (auto const &profile : profileFiles) {
		SdProfileFile sdp(profile.path.c_str());
		auto state = sdp.state();
		auto *row = new StreamDeckProfilesInstallListItem(state, profile.label, this);

		connect(row, &StreamDeckProfilesInstallListItem::requestInstall,
			this,
			[this](std::string path) {
				QDesktopServices::openUrl(
					QUrl("streamdeck://open/mainwindow"));
				QUrl fileUrl = QUrl::fromLocalFile(path.c_str());
				if (QDesktopServices::openUrl(fileUrl)) {
					
				} else {
					// Warning
				}
			});

		// insert before the stretch item
		layout_->insertWidget(layout_->count() - 1, row);
	}
}

SdaGridWidget::SdaGridWidget(QWidget *parent) : QWidget(parent)
{
	setAcceptDrops(false); // only drag OUT
	setMouseTracking(true);
}

void SdaGridWidget::setStates(std::vector<SDFileDetails> const &sdaFiles)
{
	states_.clear();
	for (auto const &sdaDat : sdaFiles) {
		SdaFile sda(sdaDat.path.c_str());
		auto state = sda.firstState().value();
		states_.push_back({state, sdaDat.label});
	}

	update();
}

int SdaGridWidget::columnCount() const
{
	// Estimate columns based on maximum logical pixmap width plus padding
	int maxWidth =
		iconSize_; // or you could calculate the largest pixmap dynamically
	int cols = width() / (maxWidth + padding_);
	return std::max(1, cols);
}

void SdaGridWidget::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::TextAntialiasing);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);

	int cols = columnCount();
	int x0 = padding_ / 2;
	int y0 = padding_ / 2;

	for (int i = 0; i < static_cast<int>(states_.size()); ++i) {
		const auto &state = states_[i].state;

		int row = i / cols;
		int col = i % cols;

		int x = x0 + col * (iconSize_ + padding_);
		int y = y0 + row * (iconSize_ + padding_);

		QPixmap pixmap = createIconPixmap(state, iconSize_, iconCornerRadius_);

		// Logical size for layout (prevents clipping)
		QSize logicalSize = pixmap.size() / pixmap.devicePixelRatio();

		painter.drawPixmap(x, y, logicalSize.width(),
				   logicalSize.height(), pixmap);
	}
}

// Map mouse position to index, accounting for HiDPI
int SdaGridWidget::indexAtPos(const QPoint &pos) const
{
	int cols = columnCount();
	if (cols <= 0)
		return -1;

	int x0 = padding_ / 2;
	int y0 = padding_ / 2;

	for (int i = 0; i < static_cast<int>(states_.size()); ++i) {
		int row = i / cols;
		int col = i % cols;

		QRect cellRect(x0 + col * (iconSize_ + padding_),
			       y0 + row * (iconSize_ + padding_), iconSize_,
			       iconSize_);

		if (cellRect.contains(pos))
			return i;
	}

	return -1;
}

void SdaGridWidget::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		dragStartPos_ = event->pos();
	}
	QWidget::mousePressEvent(event);
}

void SdaGridWidget::mouseMoveEvent(QMouseEvent *event)
{
	if (!(event->buttons() & Qt::LeftButton))
		return;

	if ((event->pos() - dragStartPos_).manhattanLength() <
	    QApplication::startDragDistance())
		return;
	
	int idx = indexAtPos(dragStartPos_);
	if (idx < 0 || idx >= (int)states_.size())
		return;

	const auto &state = states_[idx].state;
	if (state.path.isEmpty())
		return;


	auto *drag = new QDrag(this);
	auto *mime = new QMimeData();

	mime->setUrls({QUrl::fromLocalFile(state.path)});
	drag->setMimeData(mime);

	QPixmap pixmap = createIconPixmap(state, iconSize_);
	drag->setPixmap(pixmap);

	drag->exec(Qt::CopyAction);
	QDesktopServices::openUrl(QUrl("streamdeck://open/mainwindow"));
}

SdaGridScrollArea::SdaGridScrollArea(std::vector<SDFileDetails> const &sdaFiles,
				     QWidget *parent)
	: QScrollArea(parent),
	  gridWidget_(new SdaGridWidget(this))
{
	setWidget(gridWidget_);
	setWidgetResizable(true);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setStates(sdaFiles);
	
	setObjectName("SdaGridScrollArea");
	setStyleSheet("#SdaGridScrollArea { border: none; }");
}

void SdaGridScrollArea::setStates(std::vector<SDFileDetails> const &sdaFiles)
{
	gridWidget_->setStates(sdaFiles);
}

StreamDeckSetupWidget::StreamDeckSetupWidget(
	std::vector<SDFileDetails> const& sdaFiles,
	std::vector<SDFileDetails> const& profileFiles, QWidget* parent)
	: QWidget(parent)
{
	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	auto profilesLabel = new QLabel(
		obs_module_text("SceneCollectionInfo.StreamDeck.ProfilesTitle"),
		this);
	profilesLabel->setStyleSheet(
		"QLabel { font-size: 14px; margin-top: 16px; }");
	auto *profiles = new StreamDeckProfilesInstallListContainer(profileFiles, this);
	profiles->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	auto sdasLabel = new QLabel(
		obs_module_text("SceneCollectionInfo.StreamDeck.ActionsTitle"),
		this);
	sdasLabel->setStyleSheet(
		"QLabel { font-size: 14px; margin-top: 16px; }");
	auto *sdas = new SdaGridScrollArea(sdaFiles, this);
	sdas->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	layout->addWidget(profilesLabel);
	layout->addWidget(profiles);
	layout->addWidget(sdasLabel);
	layout->addWidget(sdas);
}