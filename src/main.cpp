// SPDX-License-Identifier: GPL-3.0-or-later
#include "commandregistry.h"
#include "iconprovider.h"
#include "owl/core/version.h"
#include "thememanager.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(OwlSql_ThemePlugin)
Q_IMPORT_QML_PLUGIN(OwlSql_StylePlugin)
Q_IMPORT_QML_PLUGIN(OwlSql_AppPlugin)
Q_IMPORT_QML_PLUGIN(OwlSql_UiPlugin)

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("OwlSql"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("owlsql.org"));
    QGuiApplication::setApplicationName(QStringLiteral("OwlSql"));
    QGuiApplication::setApplicationVersion(owl::core::versionString());
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/owlsql/logo/owlsql.svg")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("OwlSql database client"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption galleryOption(
        QStringLiteral("gallery"), QStringLiteral("Show the component gallery (development)."));
    parser.addOption(galleryOption);
    parser.process(app);

    // Create the shared singletons before any QML: fonts, tokens and commands.
    owl::app::CommandRegistry::instance();
    QGuiApplication::setFont(owl::theme::ThemeManager::instance()->font()->ui());

    // Our QML imports OwlSql.Style directly (compile-time selection). Setting it as the runtime
    // style too makes attached ToolTips and Qt's own QML dialogs use it.
    QQuickStyle::setStyle(QStringLiteral("OwlSql.Style"));

    QQmlApplicationEngine engine;
    owl::theme::IconProvider::install(engine);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.loadFromModule("OwlSql.Ui", parser.isSet(galleryOption) ? "Gallery" : "Main");

    return QGuiApplication::exec();
}
