// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "sessionimport.h"
#include "sessioninfo.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QUrl>

#include <memory>
#include <optional>

namespace owl {
class ConnectionWorker;
class DriverRegistry;
class IDriver;
} // namespace owl

namespace owl::app {

class SecretStore;
class Session;
class SessionListModel;
class SessionStore;

// The session manager behind the Welcome tab (decisions 065, 166–170), as the QML singleton
// `Sessions`. Editing works on drafts — plain maps the form binds to:
//   { id, name, driverId, folderId, level, tag, notes, startupScript, readOnly, savePassword,
//     hasStoredPassword, values: { <form key>: value } }
// Open sessions are Session objects keyed by session id; the "active" one is shown in the
// status bar and decides the prod frame.
class SessionManager : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Sessions)
    QML_SINGLETON
    Q_PROPERTY(owl::app::SessionListModel *model READ model CONSTANT FINAL)
    Q_PROPERTY(bool available READ isAvailable CONSTANT FINAL)
    Q_PROPERTY(QVariantList drivers READ drivers CONSTANT FINAL)
    Q_PROPERTY(QVariantList folders READ folders NOTIFY foldersChanged FINAL)
    Q_PROPERTY(bool keychainAvailable READ keychainAvailable CONSTANT FINAL)
    Q_PROPERTY(QStringList openSessions READ openSessions NOTIFY openSessionsChanged FINAL)
    Q_PROPERTY(QString activeSession READ activeSession WRITE setActiveSession NOTIFY
                   activeSessionChanged FINAL)
    Q_PROPERTY(QVariantMap activeInfo READ activeInfo NOTIFY activeSessionChanged FINAL)
    Q_PROPERTY(bool testing READ isTesting NOTIFY testingChanged FINAL)

public:
    // `store` may be null (no owlsql.db): the manager is then empty and refuses edits.
    SessionManager(SessionStore *store, DriverRegistry *drivers, SecretStore *secrets,
                   QObject *parent = nullptr);
    ~SessionManager() override;

    static SessionManager *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    SessionListModel *model() const { return m_model.get(); }
    bool isAvailable() const { return m_store != nullptr; }
    QVariantList drivers() const;
    QVariantList folders() const; // [{ id, name, depth }] in tree order, for pickers
    bool keychainAvailable() const;
    QStringList openSessions() const { return m_openOrder; }
    QString activeSession() const { return m_active; }
    void setActiveSession(const QString &id);
    QVariantMap activeInfo() const { return info(m_active); }
    bool isTesting() const { return m_testWorker != nullptr; }

    // --- Editing ---------------------------------------------------------------------------------
    // The driver's connection form: [{ key, type, label, group, default, choices:[{value, label}],
    // required, minimum, maximum, placeholder, help, visibleWhenKey, visibleWhenValue }].
    Q_INVOKABLE QVariantList form(const QString &driverId) const;
    Q_INVOKABLE QVariantMap newDraft(const QString &driverId, const QString &folderId = {}) const;
    Q_INVOKABLE QVariantMap draft(const QString &sessionId) const;
    // A draft from a connection URL; { error } when no driver understands it.
    Q_INVOKABLE QVariantMap draftFromUrl(const QString &url, const QString &folderId = {}) const;
    // Field key → message; empty when the draft can be saved. "name" for the session name.
    Q_INVOKABLE QVariantMap validate(const QVariantMap &draft) const;
    // { ok, id, error }. A typed password goes to the keychain (savePassword) or is asked for
    // on every connect.
    Q_INVOKABLE QVariantMap save(const QVariantMap &draft);
    Q_INVOKABLE bool remove(const QString &sessionId);
    Q_INVOKABLE QString duplicate(const QString &sessionId);
    Q_INVOKABLE QString addFolder(const QString &name, const QString &parentId = {});
    Q_INVOKABLE bool renameFolder(const QString &folderId, const QString &name);
    Q_INVOKABLE bool removeFolder(const QString &folderId);
    Q_INVOKABLE void setFolderExpanded(const QString &folderId, bool expanded);
    Q_INVOKABLE bool moveSession(const QString &sessionId, const QString &folderId, int position);
    // Connects with the draft's settings on a throwaway connection; ends with testFinished().
    Q_INVOKABLE void test(const QVariantMap &draft);
    Q_INVOKABLE void cancelTest();
    // A file dialog's URL as a native path (for path fields).
    Q_INVOKABLE static QString localPath(const QUrl &url);

    // --- Import / export -------------------------------------------------------------------------
    // Reads a sessions file (.owlsql-sessions.json, .pgpass, .my.cnf — by name, then content) and
    // keeps it for applyImport(): { ok, error, source, sessions, conflicts, warnings: [...] }.
    Q_INVOKABLE QVariantMap inspectImport(const QUrl &file);
    // policy: "keepBoth" | "replace" | "skip". { ok, error, imported }.
    Q_INVOKABLE QVariantMap applyImport(const QString &policy);
    // Writes the given sessions (all when empty) and their folders. { ok, error, exported }.
    Q_INVOKABLE QVariantMap exportTo(const QUrl &file, const QStringList &sessionIds = {});

    // --- Open sessions ---------------------------------------------------------------------------
    Q_INVOKABLE void open(const QString &sessionId);
    Q_INVOKABLE void providePassword(const QString &sessionId, const QString &password,
                                     bool remember) const;
    Q_INVOKABLE void close(const QString &sessionId);
    // "" (closed) | "connecting" | "needsPassword" | "connected" | "failed"
    Q_INVOKABLE QString state(const QString &sessionId) const;
    // { id, name, level, tag, state, error, product, version, driverName } — empty when unknown.
    Q_INVOKABLE QVariantMap info(const QString &sessionId) const;
    Session *session(const QString &sessionId) const;

signals:
    void foldersChanged();
    void openSessionsChanged();
    void activeSessionChanged();
    void sessionStateChanged(const QString &sessionId);
    void passwordRequested(const QString &sessionId, const QString &name);
    void sessionFailed(const QString &sessionId, const QString &message);
    void testingChanged();
    void testFinished(bool ok, const QString &message);

private:
    Result<SessionInfo> infoFromDraft(const QVariantMap &draft, QString *password) const;
    QVariantMap toDraft(const SessionInfo &info, const QString &password = {}) const;
    IDriver *driverFor(const QString &driverId) const;
    void finishTest(bool ok, const QString &message);

    SessionStore *m_store;
    DriverRegistry *m_drivers;
    QPointer<SecretStore> m_secrets;
    std::unique_ptr<SessionListModel> m_model;
    QHash<QString, Session *> m_open; // owned (children)
    QStringList m_openOrder;
    QString m_active;
    std::unique_ptr<ConnectionWorker> m_testWorker;
    std::optional<ImportBundle> m_pendingImport;
};

} // namespace owl::app
