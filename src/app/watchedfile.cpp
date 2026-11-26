// SPDX-License-Identifier: GPL-3.0-or-later
#include "watchedfile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QThread>

namespace owl::app {

WatchedFile::WatchedFile(QString path, QObject *parent)
    : QObject(parent)
    , m_path(std::move(path))
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(100);
    connect(&m_debounce, &QTimer::timeout, this, &WatchedFile::onChanged);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_debounce, qOverload<>(&QTimer::start));
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_debounce,
            qOverload<>(&QTimer::start));
    m_lastKnown = read();
    rewatch();
}

bool WatchedFile::exists() const
{
    return QFileInfo::exists(m_path);
}

QByteArray WatchedFile::read() const
{
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

bool WatchedFile::write(const QByteArray &contents, QString *error)
{
    QDir().mkpath(QFileInfo(m_path).absolutePath());

    // The atomic save replaces the target file. On Windows that fails with "Access is denied"
    // while another process briefly holds the file open (antivirus, search indexer, cloud sync,
    // an editor), so retry a few times with a short backoff before giving up (~0.4 s at most).
    constexpr int attempts = 6;
    QString lastError;
    for (int attempt = 1; attempt <= attempts; ++attempt) {
        QSaveFile file(m_path);
        if (file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size()
            && file.commit()) {
            m_lastKnown = contents;
            rewatch();
            return true;
        }
        lastError = file.errorString();
        if (attempt < attempts)
            QThread::sleep(std::chrono::milliseconds(25 * attempt));
    }
    *error = QStringLiteral("Cannot write %1: %2").arg(m_path, lastError);
    return false;
}

void WatchedFile::rewatch()
{
    // Watch the directory too: atomic saves replace the file, and it may not exist yet.
    const QString dir = QFileInfo(m_path).absolutePath();
    if (!m_watcher.directories().contains(dir) && QFileInfo::exists(dir))
        m_watcher.addPath(dir);
    if (!m_watcher.files().contains(m_path) && exists())
        m_watcher.addPath(m_path);
}

void WatchedFile::onChanged()
{
    rewatch();
    const QByteArray contents = read();
    if (contents == m_lastKnown)
        return; // our own write, or an unrelated change in the directory
    m_lastKnown = contents;
    emit changedExternally();
}

} // namespace owl::app
