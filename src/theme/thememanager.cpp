// SPDX-License-Identifier: GPL-3.0-or-later
#include "thememanager.h"

#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QQmlEngine>
#include <QSet>
#include <QStyleHints>

Q_STATIC_LOGGING_CATEGORY(lcTheme, "owlsql.theme")

namespace owl::theme {

namespace {

ThemeManager *s_instance = nullptr;

QJsonObject readJson(const QString &path, const QString &expectedFormat, QString *errorOut)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *errorOut = QStringLiteral("%1: cannot open (%2)").arg(path, file.errorString());
        return {};
    }
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        *errorOut = QStringLiteral("%1: invalid JSON (%2)").arg(path, parseError.errorString());
        return {};
    }
    QJsonObject root = doc.object();
    if (root.value(QLatin1String("format")).toString() != expectedFormat
        || root.value(QLatin1String("version")).toInt() != 1) {
        *errorOut
            = QStringLiteral("%1: expected format \"%2\" version 1").arg(path, expectedFormat);
        return {};
    }
    return root;
}

// Applies every key of `values` to the matching slot; reports missing and unknown tokens.
template<typename T, typename Convert>
QStringList applyGroup(const QString &group, const QJsonObject &values,
                       const QHash<QString, T *> &targets, Convert convert)
{
    QStringList errors;
    QSet<QString> seen;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        const QString name = ThemeManager::tokenKeyToName(it.key());
        T *slot = targets.value(name);
        if (!slot) {
            errors << QStringLiteral("%1: unknown token \"%2\"").arg(group, it.key());
            continue;
        }
        if (!convert(it.value(), slot))
            errors << QStringLiteral("%1: invalid value for \"%2\"").arg(group, it.key());
        seen.insert(name);
    }
    for (auto it = targets.constBegin(); it != targets.constEnd(); ++it) {
        if (!seen.contains(it.key()))
            errors << QStringLiteral("%1: missing token \"%2\"").arg(group, it.key());
    }
    return errors;
}

bool toInt(const QJsonValue &value, int *out)
{
    if (!value.isDouble())
        return false;
    *out = value.toInt();
    return true;
}

bool toColor(const QJsonValue &value, QColor *out)
{
    const QColor color = QColor::fromString(value.toString());
    if (!color.isValid())
        return false;
    *out = color;
    return true;
}

bool toFont(const QJsonValue &value, QFont *out)
{
    const QJsonObject spec = value.toObject();
    const QString family = spec.value(QLatin1String("family")).toString();
    const int pixelSize = spec.value(QLatin1String("pixelSize")).toInt();
    if (family.isEmpty() || pixelSize <= 0)
        return false;

    QFont font(family);
    font.setPixelSize(pixelSize);
    font.setWeight(static_cast<QFont::Weight>(spec.value(QLatin1String("weight")).toInt(400)));
    font.setHintingPreference(QFont::PreferNoHinting);
    if (spec.value(QLatin1String("tabular")).toBool())
        font.setFeature(QFont::Tag("tnum"), 1);
    *out = font;
    return true;
}

} // namespace

ThemeManager::ThemeManager(Paths paths, QObject *parent)
    : QObject(parent)
    , m_paths(std::move(paths))
    , m_colors(new ThemeColors(this))
    , m_fonts(new ThemeFonts(this))
    , m_sizes(new ThemeSizes(this))
    , m_space(new ThemeSpace(this))
    , m_radius(new ThemeRadius(this))
    , m_motion(new ThemeMotion(this))
{
    registerFonts();
    loadMetrics(m_paths.metrics);

    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        if (m_mode == Mode::System)
            updateEffectiveTheme();
    });
    updateEffectiveTheme();
}

ThemeManager::~ThemeManager()
{
    if (s_instance == this)
        s_instance = nullptr;
}

ThemeManager *ThemeManager::instance()
{
    if (!s_instance)
        s_instance = new ThemeManager(Paths{}, qApp);
    return s_instance;
}

ThemeManager *ThemeManager::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    ThemeManager *theme = instance();
    QJSEngine::setObjectOwnership(theme, QJSEngine::CppOwnership);
    return theme;
}

void ThemeManager::registerFonts()
{
    static const bool registered = [] {
        const QStringList fonts = {
            QStringLiteral(":/owlsql/fonts/inter/Inter-Regular.ttf"),
            QStringLiteral(":/owlsql/fonts/inter/Inter-Medium.ttf"),
            QStringLiteral(":/owlsql/fonts/inter/Inter-SemiBold.ttf"),
            QStringLiteral(":/owlsql/fonts/inter/Inter-Italic.ttf"),
            QStringLiteral(":/owlsql/fonts/jetbrains-mono/JetBrainsMono-Regular.ttf"),
            QStringLiteral(":/owlsql/fonts/jetbrains-mono/JetBrainsMono-Bold.ttf"),
            QStringLiteral(":/owlsql/fonts/jetbrains-mono/JetBrainsMono-Italic.ttf"),
        };
        for (const QString &font : fonts) {
            if (QFontDatabase::addApplicationFont(font) < 0)
                qCWarning(lcTheme) << "Failed to register font" << font;
        }
        return true;
    }();
    Q_UNUSED(registered)
}

void ThemeManager::setMode(Mode mode)
{
    if (m_mode == mode)
        return;
    m_mode = mode;

    // Tell the platform too, so native title bars and dialogs match.
    QStyleHints *hints = QGuiApplication::styleHints();
    switch (mode) {
    case Mode::System:
        hints->unsetColorScheme();
        break;
    case Mode::Light:
        hints->setColorScheme(Qt::ColorScheme::Light);
        break;
    case Mode::Dark:
        hints->setColorScheme(Qt::ColorScheme::Dark);
        break;
    }

    emit modeChanged();
    updateEffectiveTheme();
}

void ThemeManager::setReduceMotion(bool reduce)
{
    if (m_motion->m_reduced == reduce)
        return;
    m_motion->m_reduced = reduce;
    emit m_motion->changed();
    emit reduceMotionChanged();
}

QStringList ThemeManager::iconNames() const
{
    QStringList names = QDir(QStringLiteral(":/owlsql/icons/tabler"))
                            .entryList({QStringLiteral("*.svg")}, QDir::Files, QDir::Name);
    for (QString &name : names)
        name.chop(4); // ".svg"
    return names;
}

QString ThemeManager::tokenKeyToName(const QString &key)
{
    const QStringList parts = key.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    QString name;
    for (const QString &part : parts) {
        if (name.isEmpty())
            name = part;
        else
            name += part.left(1).toUpper() + part.mid(1);
    }
    return name;
}

void ThemeManager::loadMetrics(const QString &path)
{
    QString readError;
    const QJsonObject root = readJson(path, QStringLiteral("owlsql-metrics"), &readError);
    if (root.isEmpty()) {
        error(readError);
        return;
    }

    const auto group = [&root](const char *name) {
        return root.value(QLatin1String(name)).toObject();
    };
    m_errors << applyGroup(QStringLiteral("font"), group("font"), m_fonts->tokenSlots(), toFont)
             << applyGroup(QStringLiteral("size"), group("size"), m_sizes->tokenSlots(), toInt)
             << applyGroup(QStringLiteral("space"), group("space"), m_space->tokenSlots(), toInt)
             << applyGroup(QStringLiteral("radius"), group("radius"), m_radius->tokenSlots(), toInt)
             << applyGroup(QStringLiteral("motion"), group("motion"), m_motion->tokenSlots(),
                           toInt);
    for (const QString &message : std::as_const(m_errors))
        qCWarning(lcTheme).noquote() << message;
}

void ThemeManager::applyColors(bool dark)
{
    const QString path = dark ? m_paths.dark : m_paths.light;
    QString readError;
    const QJsonObject root = readJson(path, QStringLiteral("owlsql-theme"), &readError);
    if (root.isEmpty()) {
        error(readError);
        return;
    }

    const QStringList errors = applyGroup(QStringLiteral("color (%1)").arg(path),
                                          root.value(QLatin1String("color")).toObject(),
                                          m_colors->tokenSlots(), toColor);
    for (const QString &message : errors)
        error(message);

    m_name = root.value(QLatin1String("name")).toString();
    m_editorTheme = root.value(QLatin1String("editorTheme")).toString();
}

void ThemeManager::updateEffectiveTheme()
{
    bool dark = true;
    switch (m_mode) {
    case Mode::System:
        dark = QGuiApplication::styleHints()->colorScheme() != Qt::ColorScheme::Light;
        break;
    case Mode::Light:
        dark = false;
        break;
    case Mode::Dark:
        dark = true;
        break;
    }

    // First call always loads; later calls only when the effective theme flips.
    if (dark == m_dark && !m_name.isEmpty())
        return;
    m_dark = dark;
    applyColors(dark);
    emit m_colors->changed();
    emit themeChanged();
}

void ThemeManager::error(const QString &message)
{
    if (!m_errors.contains(message))
        m_errors << message;
    qCWarning(lcTheme).noquote() << message;
}

} // namespace owl::theme
