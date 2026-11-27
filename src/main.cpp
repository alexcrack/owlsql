// SPDX-License-Identifier: GPL-3.0-or-later
#include "appcontext.h"
#include "commandregistry.h"
#include "iconprovider.h"
#include "owl/core/version.h"
#include "singleinstance.h"
#include "thememanager.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTranslator>
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
    // App icon: exact-size images, so 16 and 24 px use the hand-tuned drawing (decision 175).
    QIcon appIcon;
    for (const int size : {16, 24, 32, 48, 64, 128, 256})
        appIcon.addFile(QStringLiteral(":/owlsql/appicon/png/owlsql-%1.png").arg(size),
                        QSize(size, size));
    QGuiApplication::setWindowIcon(appIcon);
    QGuiApplication::setDesktopFileName(QStringLiteral("org.owlsql.OwlSql"));

    // UI translation for the system language, when one is bundled (embedded under :/i18n).
    QTranslator translator;
    if (translator.load(QLocale(), QStringLiteral("owlsql"), QStringLiteral("_"),
                        QStringLiteral(":/i18n")))
        QGuiApplication::installTranslator(&translator);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("OwlSql database client"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption galleryOption(
        QStringLiteral("gallery"), QStringLiteral("Show the component gallery (development)."));
    const QCommandLineOption dataDirOption(
        QStringLiteral("data-dir"),
        QStringLiteral("Keep settings and data under <dir> instead of the standard locations."),
        QStringLiteral("dir"));
    const QCommandLineOption newInstanceOption(
        QStringLiteral("new-instance"),
        QStringLiteral("Start a separate instance even if OwlSql is already running."));
    parser.addOptions({galleryOption, dataDirOption, newInstanceOption});
    parser.process(app);

    const owl::app::AppPaths paths = parser.isSet(dataDirOption)
        ? owl::app::AppPaths::underRoot(parser.value(dataDirOption))
        : owl::app::AppPaths::standard();

    // Hand over to an already running OwlSql (same user and data directory).
    owl::app::SingleInstance instance(owl::app::SingleInstance::serverNameFor(paths.dataDir),
                                      nullptr);
    if (!parser.isSet(newInstanceOption) && !instance.start(parser.positionalArguments()))
        return EXIT_SUCCESS;

    // Shared services before any QML: theme (fonts, tokens), commands, settings, UI state.
    QGuiApplication::setFont(owl::theme::ThemeManager::instance()->font()->ui());
    owl::app::CommandRegistry::instance();
    owl::app::AppContext context(paths);
    QString error;
    if (!context.initialize(&error)) {
        qCritical().noquote() << "OwlSql cannot start:" << error;
        return EXIT_FAILURE;
    }

    // Our QML imports OwlSql.Style directly (compile-time selection). Setting it as the runtime
    // style too makes attached ToolTips and Qt's own QML dialogs use it.
    QQuickStyle::setStyle(QStringLiteral("OwlSql.Style"));

    QQmlApplicationEngine engine;
    owl::theme::IconProvider::install(engine);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.loadFromModule("OwlSql.Ui", parser.isSet(galleryOption) ? "Gallery" : "Main");

    QObject::connect(&instance, &owl::app::SingleInstance::activationRequested, &engine, [&engine] {
        if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0))) {
            if (window->visibility() == QWindow::Minimized)
                window->showNormal();
            window->raise();
            window->requestActivate();
        }
    });

    return QGuiApplication::exec();
}
