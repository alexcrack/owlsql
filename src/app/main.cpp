// SPDX-License-Identifier: GPL-3.0-or-later
#include "iconprovider.h"
#include "owl/core/version.h"
#include "thememanager.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(OwlSql_ThemePlugin)
Q_IMPORT_QML_PLUGIN(OwlSql_UiPlugin)

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("OwlSql"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("owlsql.org"));
    QGuiApplication::setApplicationName(QStringLiteral("OwlSql"));
    QGuiApplication::setApplicationVersion(owl::core::versionString());

    // Loads fonts and tokens before any QML is created.
    QGuiApplication::setFont(owl::theme::ThemeManager::instance()->font()->ui());

    // Until OwlSql.Style exists (later in M0) use the Basic style, which our style will build on.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QQmlApplicationEngine engine;
    owl::theme::IconProvider::install(engine);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.loadFromModule("OwlSql.Ui", "Main");

    return QGuiApplication::exec();
}
