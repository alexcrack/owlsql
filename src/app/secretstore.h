// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QObject>
#include <QString>

#include <functional>

namespace owl::app {

// Session secrets (passwords, passphrases) — only ever in the OS keychain, never in owlsql.db,
// settings or exports (app-store.md §2, decision 013). Asynchronous: keychains may prompt.
// Key: service "OwlSql", account "session/<session-id>/<secret>".
class SecretStore : public QObject
{
    Q_OBJECT

public:
    struct Outcome
    {
        bool ok = false;
        bool notFound = false; // read: no such secret
        QString value; // read
        QString error;
    };
    using Done = std::function<void(const Outcome &)>;

    using QObject::QObject;

    virtual bool isAvailable() const = 0;
    // `done` runs later on the main thread (never inside the call).
    virtual void read(const QString &sessionId, const QString &secret, Done done) = 0;
    virtual void write(const QString &sessionId, const QString &secret, const QString &value,
                       Done done = {})
        = 0;
    virtual void remove(const QString &sessionId, const QString &secret, Done done = {}) = 0;
    // Every secret a session can have (password, SSH and SSL passphrases).
    void removeAll(const QString &sessionId);

    static QString account(const QString &sessionId, const QString &secret);
};

// QtKeychain: Secret Service / KWallet (Linux), Keychain (macOS), Credential Manager (Windows).
class KeychainSecretStore : public SecretStore
{
    Q_OBJECT

public:
    using SecretStore::SecretStore;
    bool isAvailable() const override;
    void read(const QString &sessionId, const QString &secret, Done done) override;
    void write(const QString &sessionId, const QString &secret, const QString &value,
               Done done = {}) override;
    void remove(const QString &sessionId, const QString &secret, Done done = {}) override;
};

// In memory, for tests and for systems without a keychain (secrets last for this run only).
class MemorySecretStore : public SecretStore
{
    Q_OBJECT

public:
    using SecretStore::SecretStore;
    bool isAvailable() const override { return m_available; }
    void setAvailable(bool available) { m_available = available; }
    void read(const QString &sessionId, const QString &secret, Done done) override;
    void write(const QString &sessionId, const QString &secret, const QString &value,
               Done done = {}) override;
    void remove(const QString &sessionId, const QString &secret, Done done = {}) override;
    qsizetype size() const { return m_values.size(); }

private:
    QHash<QString, QString> m_values;
    bool m_available = true;
};

} // namespace owl::app
