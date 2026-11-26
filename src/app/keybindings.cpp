// SPDX-License-Identifier: GPL-3.0-or-later
#include "keybindings.h"

#include "commandregistry.h"
#include "watchedfile.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QLoggingCategory>

Q_STATIC_LOGGING_CATEGORY(lcKeybindings, "owlsql.keybindings")

namespace owl::app {

namespace {
// QKeySequence does not reject garbage: unknown names parse to Qt::Key_unknown.
bool isValidSequence(const QKeySequence &sequence)
{
    if (sequence.isEmpty())
        return false;
    for (int i = 0; i < sequence.count(); ++i) {
        if (sequence[i].key() == Qt::Key_unknown)
            return false;
    }
    return !sequence.toString(QKeySequence::PortableText).isEmpty();
}
} // namespace

Keybindings::Keybindings(const QString &filePath, CommandRegistry *registry, QObject *parent)
    : QObject(parent)
    , m_registry(registry)
    , m_file(new WatchedFile(filePath, this))
{
    connect(m_file, &WatchedFile::changedExternally, this, &Keybindings::reload);
    reload();
}

void Keybindings::reload()
{
    QStringList problems;
    m_registry->clearShortcutOverrides();

    if (m_file->exists()) {
        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(m_file->read(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
            problems << tr("keybindings.json must be a JSON array (%1); ignored.")
                            .arg(parseError.errorString());
        } else {
            const QJsonArray entries = doc.array();
            for (const auto &entry : entries) {
                const QString command = entry.toObject().value(QLatin1String("command")).toString();
                const QJsonValue keyValue = entry.toObject().value(QLatin1String("key"));
                if (command.isEmpty() || !keyValue.isString()) {
                    problems << tr(R"(Entries need "command" and "key" strings; one was ignored.)");
                    continue;
                }
                if (!m_registry->contains(command)) {
                    problems << tr("%1: unknown command; ignored.").arg(command);
                    continue;
                }
                const QString keyText = keyValue.toString();
                const QKeySequence sequence(keyText, QKeySequence::PortableText);
                if (!keyText.isEmpty() && !isValidSequence(sequence)) {
                    problems << tr("%1: invalid key \"%2\"; ignored.").arg(command, keyText);
                    continue;
                }
                m_registry->setShortcutOverride(command, sequence);
            }
        }
    }

    if (problems != m_problems) {
        m_problems = problems;
        for (const QString &problem : std::as_const(m_problems))
            qCWarning(lcKeybindings).noquote() << problem;
        emit problemsChanged();
    }
}

} // namespace owl::app
