// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>

namespace owl::app {

// A user-editable file that is read, written atomically and watched for outside changes.
// Survives editors that save by replacing the file, and ignores the echo of our own writes.
class WatchedFile : public QObject
{
    Q_OBJECT

public:
    WatchedFile(QString path, QObject *parent);

    QString path() const { return m_path; }
    bool exists() const;
    QByteArray read() const;
    bool write(const QByteArray &contents, QString *error);

signals:
    // Contents changed on disk by someone else (debounced).
    void changedExternally();

private:
    void rewatch();
    void onChanged();

    QString m_path;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QByteArray m_lastKnown;
};

} // namespace owl::app
