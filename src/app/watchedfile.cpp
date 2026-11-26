// SPDX-License-Identifier: GPL-3.0-or-later
#include "watchedfile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

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
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size()
        || !file.commit()) {
        *error = QStringLiteral("Cannot write %1: %2").arg(m_path, file.errorString());
        return false;
    }
    m_lastKnown = contents;
    rewatch();
    return true;
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
