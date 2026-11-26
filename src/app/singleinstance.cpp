// SPDX-License-Identifier: GPL-3.0-or-later
#include "singleinstance.h"

#include <QCryptographicHash>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLoggingCategory>

Q_STATIC_LOGGING_CATEGORY(lcInstance, "owlsql.instance")

namespace owl::app {

namespace {
constexpr int connectTimeoutMs = 500;
}

SingleInstance::SingleInstance(QString serverName, QObject *parent)
    : QObject(parent)
    , m_serverName(std::move(serverName))
{ }

SingleInstance::~SingleInstance() = default;

QString SingleInstance::serverNameFor(const QString &dataDir)
{
    const QByteArray user = qgetenv("USER").isEmpty() ? qgetenv("USERNAME") : qgetenv("USER");
    const QByteArray key = user + '\0' + QDir::cleanPath(dataDir).toUtf8();
    return QStringLiteral("owlsql-")
        + QString::fromLatin1(
               QCryptographicHash::hash(key, QCryptographicHash::Sha256).toHex().left(16));
}

bool SingleInstance::start(const QStringList &arguments)
{
    // Is another instance listening? Hand over the arguments and step aside.
    {
        QLocalSocket socket;
        socket.connectToServer(m_serverName);
        if (socket.waitForConnected(connectTimeoutMs)) {
            socket.write(QJsonDocument(QJsonArray::fromStringList(arguments))
                             .toJson(QJsonDocument::Compact));
            socket.waitForBytesWritten(connectTimeoutMs);
            socket.disconnectFromServer();
            if (socket.state() != QLocalSocket::UnconnectedState)
                socket.waitForDisconnected(connectTimeoutMs);
            return false;
        }
    }

    // We are first. A stale socket file from a crashed instance would block listen().
    QLocalServer::removeServer(m_serverName);
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server->listen(m_serverName)) {
        qCWarning(lcInstance) << "Cannot listen on" << m_serverName << m_server->errorString()
                              << "- continuing without single-instance support";
        return true;
    }
    connect(m_server, &QLocalServer::newConnection, this, &SingleInstance::onNewConnection);
    return true;
}

void SingleInstance::onNewConnection()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        auto buffer = std::make_shared<QByteArray>();
        connect(socket, &QLocalSocket::readyRead, socket,
                [socket, buffer] { buffer->append(socket->readAll()); });
        connect(socket, &QLocalSocket::disconnected, this, [this, socket, buffer] {
            buffer->append(socket->readAll());
            const QJsonDocument doc = QJsonDocument::fromJson(*buffer);
            QStringList arguments;
            const QJsonArray values = doc.array();
            for (const auto &value : values)
                arguments << value.toString();
            socket->deleteLater();
            emit activationRequested(arguments);
        });
    }
}

} // namespace owl::app
