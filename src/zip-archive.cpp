#include "zip-archive.hpp"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QBuffer>
#include <QCoreApplication>
#include <QMetaObject>
#include <cstring>

ZipArchive::ZipArchive(QObject *parent)
    : QObject(parent)
{
}

ZipArchive::~ZipArchive()
{
    cancelOperation();
    if (m_zipRead.get()) m_zipRead.close();
}

void ZipArchive::cancelOperation()
{
    m_cancelRequested.store(true, std::memory_order_relaxed);
}

void ZipArchive::addFile(const QString &zipInternalName, const QString &sourcePath)
{
    PendingEntry e;
    e.internalName = zipInternalName;
    e.sourcePath = sourcePath;
    m_pending.append(std::move(e));
}

void ZipArchive::addData(const QString &zipInternalName, const QByteArray &data)
{
    PendingEntry e;
    e.internalName = zipInternalName;
    e.data = data;
    m_pending.append(std::move(e));
}

void ZipArchive::addString(const QString &zipInternalName,
			   const QString &stringData)
{
	QByteArray data = stringData.toUtf8();
	addData(zipInternalName, data);
}

bool ZipArchive::writeArchive(const QString &targetZipPath)
{
	m_cancelRequested.store(false, std::memory_order_relaxed);

	qint64 totalBytes = 0;

	for (const auto &p : m_pending) {
		totalBytes += p.size();
	}

    if (totalBytes <= 0) totalBytes = 0; // still support zero-byte archives

    int err = 0;
    ZipHandle z(zip_open(targetZipPath.toUtf8().constData(), ZIP_CREATE | ZIP_TRUNCATE, &err));
    if (!z) return false;

    bool ok = writePendingToZip(z.get(), totalBytes);

    if (!ok) {
        zip_discard(z.get());
	    if (m_cancelRequested.load(std::memory_order_relaxed)) {
		    QFile::remove(targetZipPath);
	    }
        return false;
    }

    return true;
}

bool ZipArchive::writePendingToZip(zip_t *zip, qint64 totalBytes)
{
	qint64 overallWritten = 0;
	const qint64 totalEntries = m_pending.size();
	//int writeSteps = 0;

	for (qint64 i = 0; i < totalEntries; ++i) {
		if (m_cancelRequested.load(std::memory_order_relaxed))
			return false;

		const PendingEntry &entry = m_pending[size_t(i)];
		const QString &name = entry.internalName;

		zip_source_t *src = nullptr;

		if (entry.isFile()) {
			src = createProgressingSourceFromFile(entry, this,
							      entry.size(),
							      overallWritten,
							      totalBytes);
		} else {
			src = createProgressingSourceFromMemory(
				entry, this, overallWritten, totalBytes);
		}

		if (!src)
			return false;

		if (m_cancelRequested.load(std::memory_order_relaxed)) {
			zip_source_free(src);
			return false;
		}

		zip_int64_t idx =
			zip_file_add(zip, name.toUtf8().constData(), src,
				     ZIP_FL_OVERWRITE | ZIP_FL_ENC_UTF_8);
		if (idx < 0) {
			zip_source_free(src);
			return false;
		}

		overallWritten += entry.size();

		if (m_cancelRequested.load(std::memory_order_relaxed))
			return false;
	}

	m_pending.clear();
	return true;
}

// Create a zip_source that streams from file and reports progress to the owner.
// We pass a dynamically allocated context that the callback will free in ZIP_SOURCE_FREE.
zip_source_t* ZipArchive::createProgressingSourceFromFile(const PendingEntry &entry,
                                                          ZipArchive *owner,
                                                          qint64 totalBytesForEntry,
                                                          qint64 alreadyWrittenForOverall,
                                                          qint64 overallTotalBytes)
{
    struct Ctx {
        QFile *file;
        QString name;
        ZipArchive *owner;
        qint64 totalForEntry;
        qint64 overallOffset; // bytes already written before this entry
        qint64 overallTotal;
        qint64 reportedForFile = 0;
		int reportedPctForFile = -1;
    };

    Ctx *ctx = new Ctx();
    ctx->file = new QFile(entry.sourcePath);
    if (!ctx->file->open(QIODevice::ReadOnly)) {
        delete ctx->file;
        delete ctx;
        return nullptr;
    }
    ctx->name = entry.internalName;
    ctx->owner = owner;
    ctx->totalForEntry = totalBytesForEntry;
    ctx->overallOffset = alreadyWrittenForOverall;
    ctx->overallTotal = overallTotalBytes;
    ctx->reportedForFile = 0;
    ctx->reportedPctForFile = -1;

    auto callback = [](void *userdata, void *data, zip_uint64_t len, zip_source_cmd_t cmd) -> zip_int64_t {
        Ctx *c = reinterpret_cast<Ctx*>(userdata);

	    // Global cancel check helper
	    auto isCanceled = [&]() {
		    return c->owner->m_cancelRequested.load(
			    std::memory_order_relaxed);
	    };

        switch (cmd) {
        case ZIP_SOURCE_SUPPORTS:
            return ZIP_SOURCE_SUPPORTS_READABLE;
        case ZIP_SOURCE_OPEN:
			if (isCanceled())
				return -1; // abort before reading starts
			if (!c->file->seek(0))
				return -1;
			return 0;
        case ZIP_SOURCE_READ: {
			if (isCanceled())
				return -1;

            qint64 r = c->file->read(reinterpret_cast<char*>(data), (qint64)len);
			
			if (r > 0) {
				if (isCanceled())
					return -1;

                // emit progress for this file and overall
                c->reportedForFile += r;
				double fileP = (c->totalForEntry > 0) ? (double(c->reportedForFile) / double(c->totalForEntry)) : 1.0;
				int pct = static_cast<int>(floor(fileP * 1000.0));
				if (pct != c->reportedPctForFile) {
					c->reportedPctForFile = pct;
					// emit file progress (via queued/auto connection to be safe)
					QMetaObject::invokeMethod(c->owner, "emitFileProgress", Qt::AutoConnection,
											  Q_ARG(QString, c->name),
											  Q_ARG(double, fileP));
					if (c->overallTotal > 0) {
						double overallP = double(c->overallOffset + c->reportedForFile) / double(c->overallTotal);
						QMetaObject::invokeMethod(c->owner, "emitOverallProgress", Qt::AutoConnection,
												  Q_ARG(double, overallP));
					}
				}

            }
            return r >= 0 ? r : -1;
        }
        case ZIP_SOURCE_CLOSE:
            return 0;
        case ZIP_SOURCE_STAT:
			if (isCanceled())
				return -1;
			{
				zip_stat_t *st = reinterpret_cast<zip_stat_t*>(data);
				zip_stat_init(st);
				st->size = c->file->size();
				st->mtime = QFileInfo(*c->file).lastModified().toSecsSinceEpoch();
				st->valid = ZIP_STAT_SIZE | ZIP_STAT_MTIME;
				return 0;
			}
        case ZIP_SOURCE_ERROR:
            return 0;
        case ZIP_SOURCE_FREE:
            c->file->close();
            delete c->file;
            delete c;
            return 0;
        default:
            return -1;
        }
    };

    zip_source_t *src = zip_source_function_create(callback, ctx, nullptr);
    return src;
}

zip_source_t *ZipArchive::createProgressingSourceFromMemory(
	const PendingEntry &entry, ZipArchive *owner,
	qint64 alreadyWrittenForOverall, qint64 overallTotalBytes)
{
	struct Ctx {
		QBuffer *buffer;
		QString name;
		ZipArchive *owner;
		qint64 totalForEntry;
		qint64 overallOffset;
		qint64 overallTotal;
		qint64 reportedForFile = 0;
	};

	Ctx *ctx = new Ctx();
	ctx->buffer = new QBuffer();
	ctx->buffer->setData(entry.data);
	ctx->buffer->open(QIODevice::ReadOnly);
	ctx->name = entry.internalName;
	ctx->owner = owner;
	ctx->totalForEntry = entry.data.size();
	ctx->overallOffset = alreadyWrittenForOverall;
	ctx->overallTotal = overallTotalBytes;
	ctx->reportedForFile = 0;

	auto callback = [](void *userdata, void *data, zip_uint64_t len,
			   zip_source_cmd_t cmd) -> zip_int64_t {
		Ctx *c = reinterpret_cast<Ctx *>(userdata);

		auto isCanceled = [&]() {
			return c->owner->m_cancelRequested.load(std::memory_order_relaxed);
		};

		switch (cmd) {
		case ZIP_SOURCE_SUPPORTS:
			return ZIP_SOURCE_SUPPORTS_READABLE;
		case ZIP_SOURCE_OPEN:
			if (isCanceled())
				return -1;
			if (!c->buffer->seek(0))
				return -1;
			return 0;
		case ZIP_SOURCE_READ: {
			if (isCanceled())
				return -1;

			qint64 r = c->buffer->read(
				reinterpret_cast<char *>(data), (qint64)len);
			if (r > 0) {
				if (isCanceled())
					return -1;

				c->reportedForFile += r;
				double fileP =
					(c->totalForEntry > 0)
						? double(c->reportedForFile) /
							  double(c->totalForEntry)
						: 1.0;
				QMetaObject::invokeMethod(
					c->owner, "emitFileProgress",
					Qt::AutoConnection,
					Q_ARG(QString, c->name),
					Q_ARG(double, fileP));
			}
			return r >= 0 ? r : -1;
		}
		case ZIP_SOURCE_CLOSE:
			return 0;
		case ZIP_SOURCE_STAT: 
		    if (isCanceled())
				return -1;
			{
				zip_stat_t *st = reinterpret_cast<zip_stat_t *>(data);
				zip_stat_init(st);
				st->size = c->totalForEntry;
				st->mtime = QDateTime::currentSecsSinceEpoch();
				st->valid = ZIP_STAT_SIZE | ZIP_STAT_MTIME;
				return 0;
			}
		case ZIP_SOURCE_ERROR:
			return 0;
		case ZIP_SOURCE_FREE:
			c->buffer->close();
			delete c->buffer;
			delete c;
			return 0;
		default:
			return -1;
		}
	};

	return zip_source_function_create(callback, ctx, nullptr);
}

bool ZipArchive::openExisting(const QString &zipPath)
{
    if (m_zipRead.get()) { m_zipRead.close(); }
    int e = 0;
    ZipHandle z(zip_open(zipPath.toUtf8().constData(), ZIP_RDONLY, &e));
    if (!z) return false;
    // keep z in m_zipRead by moving
    m_zipRead = std::move(z);
    m_openedPath = zipPath;
    return true;
}

QVector<QString> ZipArchive::listEntries() const
{
    QVector<QString> out;
    if (!m_zipRead.get()) return out;
    zip_int64_t n = zip_get_num_entries(m_zipRead.get(), 0);
    for (zip_int64_t i = 0; i < n; ++i) {
        const char *name = zip_get_name(m_zipRead.get(), i, ZIP_FL_ENC_UTF_8);
        if (name) out.append(QString::fromUtf8(name));
    }
    return out;
}

bool ZipArchive::contains(const QString &zipInternalName) const
{
	if (!m_zipRead.get())
		return false;

	zip_int64_t idx = zip_name_locate(m_zipRead.get(),
					  zipInternalName.toUtf8().constData(),
					  ZIP_FL_ENC_UTF_8);
	return (idx >= 0);
}

QByteArray ZipArchive::extractFileToMemory(const QString &zipInternalName, bool *ok)
{
    if (ok) *ok = false;
    if (!m_zipRead.get()) return {};
    zip_file_t *zf = zip_fopen(m_zipRead.get(), zipInternalName.toUtf8().constData(), ZIP_FL_ENC_UTF_8);
    if (!zf) return {};

    ZipFileHandle fh(zf); // will auto-close

    zip_stat_t st;
    if (zip_stat(m_zipRead.get(), zipInternalName.toUtf8().constData(), ZIP_FL_ENC_UTF_8, &st) != 0) return {};

    QByteArray out;
    constexpr qint64 CHUNK = 1024 * 256;
    char buf[CHUNK];
    zip_int64_t totalRead = 0;

    while ((totalRead = zip_fread(fh.get(), buf, CHUNK)) > 0) {
        out.append(buf, (int)totalRead);
        double fileP = (st.size > 0) ? (double(out.size()) / double(st.size)) : 1.0;
        QMetaObject::invokeMethod(const_cast<ZipArchive*>(this), "emitExtractFileProgress", Qt::AutoConnection,
                                  Q_ARG(QString, zipInternalName), Q_ARG(double, fileP));
    }

    if (ok) *ok = true;
    return out;
}

QString ZipArchive::extractFileToString(const QString &zipInternalName,
					bool *ok)
{
	QByteArray data = extractFileToMemory(zipInternalName, ok);

	if (ok && !*ok) {
		return QString();
	}

	QString out = QString::fromUtf8(data);

	return out;
}

bool ZipArchive::extractAllToFolder(const QString &destinationDir)
{
	if (!m_zipRead.get())
		return false;
	QDir().mkpath(destinationDir);

	zip_int64_t n = zip_get_num_entries(m_zipRead.get(), 0);

	// compute total bytes for overall extraction progress
	qint64 totalBytes = 0;
	QVector<zip_uint64_t> sizes;
	sizes.resize(n);
	for (zip_uint64_t i = 0; i < (zip_uint64_t)n; ++i) {
		zip_stat_t st;
		if (zip_stat_index(m_zipRead.get(), i, ZIP_FL_ENC_UTF_8, &st) ==
		    0) {
			sizes[i] = st.size;
			totalBytes += st.size;
		} else {
			sizes[i] = 0;
		}
	}

	qint64 overallWritten = 0;

	// Use a pointer to allocate the read/write buffer on the heap
	// instead of a a large stack allocation
	const qint64 CHUNK = 1024 * 256;
	std::unique_ptr<char[]> buf(new char[CHUNK]);

	for (zip_uint64_t i = 0; i < (zip_uint64_t)n; ++i) {
		if (m_cancelRequested.load(std::memory_order_relaxed))
			return false;
		const char *cname =
			zip_get_name(m_zipRead.get(), i, ZIP_FL_ENC_UTF_8);
		if (!cname)
			continue;
		QString name = QString::fromUtf8(cname);
		QString outPath = QDir(destinationDir).filePath(name);
		if (name.endsWith("/")) {
			QDir().mkpath(outPath);
			continue;
		}

		ZipFileHandle fh(zip_fopen_index(m_zipRead.get(), i, 0));
		if (!fh)
			return false;

		QDir().mkpath(QFileInfo(outPath).path());
		QFile out(outPath);
		if (!out.open(QIODevice::WriteOnly))
			return false;

		zip_int64_t r;
		qint64 writtenForThis = 0;
		zip_uint64_t expected = sizes[i];

		while ((r = zip_fread(fh.get(), buf.get(), CHUNK)) > 0) {
			out.write(buf.get(), (int)r);
			writtenForThis += r;
			overallWritten += r;
			double fileP = (expected > 0)
					       ? (double(writtenForThis) /
						  double(expected))
					       : 1.0;
			double overallP = (totalBytes > 0)
						  ? (double(overallWritten) /
						     double(totalBytes))
						  : 1.0;
			QMetaObject::invokeMethod(this,
						  "emitExtractFileProgress",
						  Qt::AutoConnection,
						  Q_ARG(QString, name),
						  Q_ARG(double, fileP));
			QMetaObject::invokeMethod(this,
						  "emitExtractOverallProgress",
						  Qt::AutoConnection,
						  Q_ARG(double, overallP));
		}

		out.close();
	}

	QMetaObject::invokeMethod(this, "emitExtractOverallProgress",
				  Qt::AutoConnection, Q_ARG(double, 1.0));
	return true;
}

void ZipArchive::emitFileProgress(const QString &name, double progress)
{
    emit fileProgress(name, progress);
}

void ZipArchive::emitOverallProgress(double p)
{
    emit overallProgress(p);
}

void ZipArchive::emitExtractFileProgress(const QString &name, double progress)
{
    emit extractFileProgress(name, progress);
}

void ZipArchive::emitExtractOverallProgress(double p)
{
    emit extractOverallProgress(p);
}
