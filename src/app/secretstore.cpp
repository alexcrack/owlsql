// SPDX-License-Identifier: GPL-3.0-or-later
#include "secretstore.h"

#include "sessioninfo.h"

#include <QTimer>
#include <qt6keychain/keychain.h>

namespace owl::app {

namespace {

const QString service = QStringLiteral("OwlSql");

void later(QObject *context, SecretStore::Done done, SecretStore::Outcome outcome)
{
    if (!done)
        return;
    QTimer::singleShot(0, context,
                       [done = std::move(done), outcome = std::move(outcome)] { done(outcome); });
}

template<typename Job>
void run(Job *job, SecretStore::Done done, bool isRead)
{
    QObject::connect(job, &QKeychain::Job::finished, job, [job, done = std::move(done), isRead] {
        if (!done)
            return;
        SecretStore::Outcome o;
        o.ok = job->error() == QKeychain::NoError;
        o.notFound = job->error() == QKeychain::EntryNotFound;
        if (!o.ok)
            o.error = job->errorString();
        if constexpr (std::is_same_v<Job, QKeychain::ReadPasswordJob>) {
            if (o.ok && isRead)
                o.value = job->textData();
        }
        done(o);
    });
    job->start();
}

} // namespace

QString SecretStore::account(const QString &sessionId, const QString &secret)
{
    return QStringLiteral("session/%1/%2").arg(sessionId, secret);
}

void SecretStore::removeAll(const QString &sessionId)
{
    for (const QLatin1StringView s : {secret::Password, secret::SshPassword,
                                      secret::SshKeyPassphrase, secret::SslKeyPassphrase})
        remove(sessionId, s);
}

bool KeychainSecretStore::isAvailable() const
{
    return QKeychain::isAvailable();
}

void KeychainSecretStore::read(const QString &sessionId, const QString &secret, Done done)
{
    auto *job = new QKeychain::ReadPasswordJob(service, this);
    job->setAutoDelete(true);
    job->setKey(account(sessionId, secret));
    run(job, std::move(done), true);
}

void KeychainSecretStore::write(const QString &sessionId, const QString &secret,
                                const QString &value, Done done)
{
    auto *job = new QKeychain::WritePasswordJob(service, this);
    job->setAutoDelete(true);
    job->setKey(account(sessionId, secret));
    job->setTextData(value);
    run(job, std::move(done), false);
}

void KeychainSecretStore::remove(const QString &sessionId, const QString &secret, Done done)
{
    auto *job = new QKeychain::DeletePasswordJob(service, this);
    job->setAutoDelete(true);
    job->setKey(account(sessionId, secret));
    run(job, std::move(done), false);
}

void MemorySecretStore::read(const QString &sessionId, const QString &secret, Done done)
{
    Outcome o;
    if (!m_available) {
        o.error = QStringLiteral("No keychain available");
    } else if (const auto it = m_values.constFind(account(sessionId, secret));
               it != m_values.cend()) {
        o.ok = true;
        o.value = *it;
    } else {
        o.notFound = true;
        o.error = QStringLiteral("Not found");
    }
    later(this, std::move(done), o);
}

void MemorySecretStore::write(const QString &sessionId, const QString &secret, const QString &value,
                              Done done)
{
    Outcome o;
    o.ok = m_available;
    if (m_available)
        m_values.insert(account(sessionId, secret), value);
    else
        o.error = QStringLiteral("No keychain available");
    later(this, std::move(done), o);
}

void MemorySecretStore::remove(const QString &sessionId, const QString &secret, Done done)
{
    Outcome o;
    o.ok = m_values.remove(account(sessionId, secret)) > 0;
    o.notFound = !o.ok;
    later(this, std::move(done), o);
}

} // namespace owl::app
