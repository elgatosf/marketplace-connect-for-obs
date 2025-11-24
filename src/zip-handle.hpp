#pragma once

#include <zip.h>
#include <QFile>
#include <QFileInfo>
#include <QMetaType>
#include <memory>

// RAII wrapper for zip_t*
class ZipHandle {
public:
    explicit ZipHandle(zip_t *z = nullptr) : z_(z) {}
    ~ZipHandle() { close(); }

    ZipHandle(const ZipHandle&) = delete;
    ZipHandle& operator=(const ZipHandle&) = delete;

    ZipHandle(ZipHandle&& o) noexcept : z_(o.z_) { o.z_ = nullptr; }
    ZipHandle& operator=(ZipHandle&& o) noexcept {
        if (this != &o) { close(); z_ = o.z_; o.z_ = nullptr; }
        return *this;
    }

    operator bool() const { return z_ != nullptr; }
    zip_t* get() const { return z_; }

    void close() { if (z_) { zip_close(z_); z_ = nullptr; } }

private:
    zip_t *z_ = nullptr;
};

// RAII wrapper for zip_file_t*
class ZipFileHandle {
public:
    explicit ZipFileHandle(zip_file_t *f = nullptr) : f_(f) {}
    ~ZipFileHandle() { close(); }

    ZipFileHandle(const ZipFileHandle&) = delete;
    ZipFileHandle& operator=(const ZipFileHandle&) = delete;

    ZipFileHandle(ZipFileHandle&& o) noexcept : f_(o.f_) { o.f_ = nullptr; }
    ZipFileHandle& operator=(ZipFileHandle&& o) noexcept {
        if (this != &o) { close(); f_ = o.f_; o.f_ = nullptr; }
        return *this;
    }

    operator bool() const { return f_ != nullptr; }
    zip_file_t* get() const { return f_; }
    void close() { if (f_) { zip_fclose(f_); f_ = nullptr; } }

private:
    zip_file_t *f_ = nullptr;
};

// RAII wrapper for zip_source_t*
class ZipSourceHandle {
public:
    explicit ZipSourceHandle(zip_source_t *s = nullptr) : s_(s) {}
    ~ZipSourceHandle() { free(); }

    ZipSourceHandle(const ZipSourceHandle&) = delete;
    ZipSourceHandle& operator=(const ZipSourceHandle&) = delete;

    ZipSourceHandle(ZipSourceHandle&& o) noexcept : s_(o.s_) { o.s_ = nullptr; }
    ZipSourceHandle& operator=(ZipSourceHandle&& o) noexcept {
        if (this != &o) { free(); s_ = o.s_; o.s_ = nullptr; }
        return *this;
    }

    operator bool() const { return s_ != nullptr; }
    zip_source_t* get() const { return s_; }
    void release() { s_ = nullptr; } // relinquish ownership (libzip will own it)
    void free() { if (s_) { zip_source_free(s_); s_ = nullptr; } }

private:
    zip_source_t *s_ = nullptr;
};

// Helper: create a zip_source that reads from a QFile (streaming).
// Returns ZipSourceHandle. The ownership of the QFile is transferred to the source
// and will be cleaned up in ZIP_SOURCE_FREE.
static inline ZipSourceHandle makeZipSourceFromQFile(const QString &path)
{
    QFile *file = new QFile(path);
    if (!file->open(QIODevice::ReadOnly)) { delete file; return ZipSourceHandle(nullptr); }

    auto callback = [](void *userdata, void *data, zip_uint64_t len, zip_source_cmd_t cmd) -> zip_int64_t {
        QFile *f = reinterpret_cast<QFile*>(userdata);
        switch (cmd) {
        case ZIP_SOURCE_SUPPORTS:
            return ZIP_SOURCE_SUPPORTS_READABLE;
        case ZIP_SOURCE_OPEN:
            if (!f->seek(0)) return -1;
            return 0;
        case ZIP_SOURCE_READ: {
            qint64 r = f->read(reinterpret_cast<char*>(data), (qint64)len);
            return (r >= 0) ? r : -1;
        }
        case ZIP_SOURCE_CLOSE:
            return 0;
        case ZIP_SOURCE_STAT: {
            zip_stat_t *st = reinterpret_cast<zip_stat_t*>(data);
            zip_stat_init(st);
            st->size = f->size();
            st->mtime = QFileInfo(*f).lastModified().toSecsSinceEpoch();
            st->valid = ZIP_STAT_SIZE | ZIP_STAT_MTIME;
            return 0;
        }
        case ZIP_SOURCE_ERROR:
            return 0;
        case ZIP_SOURCE_FREE:
            // ensure deletion happens on Qt's object system safe context
            f->deleteLater();
            return 0;
        default:
            return -1;
        }
    };

    zip_source_t *src = zip_source_function_create(callback, file, nullptr);
    return ZipSourceHandle(src);
}
