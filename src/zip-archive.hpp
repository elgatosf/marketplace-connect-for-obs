#pragma once

#include "zip-handle.hpp"

#include <QObject>
#include <QString>
#include <QVector>
#include <QByteArray>
#include <atomic>

class ZipArchive : public QObject
{
    Q_OBJECT
public:
    explicit ZipArchive(QObject *parent = nullptr);
    ~ZipArchive() override;

    // Queue APIs (lazy)
    void addFile(const QString &zipInternalName, const QString &sourcePath);
    void addData(const QString &zipInternalName, const QByteArray &data);
    void addString(const QString &zipInternalName, const QString &stringData);

    // Write queued entries to disk (streams large files)
    // Returns true on success. Emits progress signals during the operation.
    bool writeArchive(const QString &targetZipPath);

    // Read existing archive
    bool openExisting(const QString &zipPath);
    QVector<QString> listEntries() const;
    bool contains(const QString &zipInternalName) const;
    QByteArray extractFileToMemory(const QString &zipInternalName, bool *ok = nullptr);
    QString extractFileToString(const QString &zipInternalName, bool *ok = nullptr);
    bool extractAllToFolder(const QString &destinationDir);

    // Cancel the currently running write/extract (thread-safe)
    void cancelOperation();

signals:
    // fileName, fileProgress (0..1)
    void fileProgress(const QString &fileName, double progress);

    // overallProgress across whole archive (0..1)
    void overallProgress(double progress);

    // extraction signals (0..1)
    void extractFileProgress(const QString &fileName, double progress);
    void extractOverallProgress(double progress);

private:
    struct PendingEntry {
        QString internalName;
        QString sourcePath;   // if non-empty, use file streaming
        QByteArray data;      // if not empty, write from memory
        bool isFile() const { return !sourcePath.isEmpty(); }
        qint64 size() const { return isFile() ? QFileInfo(sourcePath).size() : data.size(); }
    };

    QVector<PendingEntry> m_pending;
    ZipHandle m_zipRead;
    QString m_openedPath;

    std::atomic<bool> m_cancelRequested{false};

    bool writePendingToZip(zip_t *zip, qint64 totalBytes);

    static zip_source_t *createProgressingSourceFromFile(
	    const PendingEntry &entry, ZipArchive *owner,
	    qint64 totalBytesForEntry, qint64 alreadyWrittenForOverall,
	    qint64 overallTotalBytes);

    static zip_source_t *createProgressingSourceFromMemory(
	    const PendingEntry &entry, ZipArchive *owner,
	    qint64 alreadyWrittenForOverall, qint64 overallTotalBytes);

    Q_INVOKABLE void emitFileProgress(const QString &name, double progress);
    Q_INVOKABLE void emitOverallProgress(double p);
    Q_INVOKABLE void emitExtractFileProgress(const QString &name, double progress);
    Q_INVOKABLE void emitExtractOverallProgress(double p);
};

