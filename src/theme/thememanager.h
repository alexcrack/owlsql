// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "themetokens.h"

#include <QObject>
#include <QStringList>
#include <QtQmlIntegration/qqmlintegration.h>

class QJSEngine;
class QQmlEngine;

namespace owl::theme {

// QML singleton "Theme": loads token files, follows Light/Dark/System and exposes typed tokens.
// See docs/specs/design-system.md §2.
class ThemeManager : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Theme)
    QML_SINGLETON

    Q_PROPERTY(Mode mode READ mode WRITE setMode NOTIFY modeChanged FINAL)
    Q_PROPERTY(bool dark READ isDark NOTIFY themeChanged FINAL)
    Q_PROPERTY(QString name READ name NOTIFY themeChanged FINAL)
    Q_PROPERTY(QString editorTheme READ editorTheme NOTIFY themeChanged FINAL)
    Q_PROPERTY(
        bool reduceMotion READ reduceMotion WRITE setReduceMotion NOTIFY reduceMotionChanged FINAL)
    Q_PROPERTY(owl::theme::ThemeColors *color READ color CONSTANT FINAL)
    Q_PROPERTY(owl::theme::ThemeFonts *font READ font CONSTANT FINAL)
    Q_PROPERTY(owl::theme::ThemeSizes *size READ size CONSTANT FINAL)
    Q_PROPERTY(owl::theme::ThemeSpace *space READ space CONSTANT FINAL)
    Q_PROPERTY(owl::theme::ThemeRadius *radius READ radius CONSTANT FINAL)
    Q_PROPERTY(owl::theme::ThemeMotion *motion READ motion CONSTANT FINAL)

public:
    enum class Mode : quint8 { System, Light, Dark };
    Q_ENUM(Mode)

    struct Paths
    {
        QString metrics = QStringLiteral(":/owlsql/themes/metrics.json");
        QString light = QStringLiteral(":/owlsql/themes/owl-light.json");
        QString dark = QStringLiteral(":/owlsql/themes/owl-dark.json");
    };

    // Deliberately not default-constructible: QML_SINGLETON prefers a default constructor over
    // create(), which would give every QML engine its own private instance.
    ThemeManager(Paths paths, QObject *parent);
    ~ThemeManager() override;

    // The application-wide instance (created on first use). QML gets the same object.
    static ThemeManager *instance();
    static ThemeManager *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    // Registers the bundled Inter and JetBrains Mono fonts. Safe to call repeatedly.
    static void registerFonts();

    Mode mode() const { return m_mode; }
    void setMode(Mode mode);
    bool isDark() const { return m_dark; }
    QString name() const { return m_name; }
    QString editorTheme() const { return m_editorTheme; }
    bool reduceMotion() const { return m_motion->m_reduced; }
    void setReduceMotion(bool reduce);

    ThemeColors *color() const { return m_colors; }
    ThemeFonts *font() const { return m_fonts; }
    ThemeSizes *size() const { return m_sizes; }
    ThemeSpace *space() const { return m_space; }
    ThemeRadius *radius() const { return m_radius; }
    ThemeMotion *motion() const { return m_motion; }

    // Problems found while loading token files (missing/unknown/invalid tokens). Empty when OK.
    QStringList loadErrors() const { return m_errors; }

    // "bg.selectedInactive" -> "bgSelectedInactive"
    static QString tokenKeyToName(const QString &key);

signals:
    void modeChanged();
    void themeChanged();
    void reduceMotionChanged();

private:
    void loadMetrics(const QString &path);
    void applyColors(bool dark);
    void updateEffectiveTheme();
    void error(const QString &message);

    Paths m_paths;
    Mode m_mode = Mode::System;
    bool m_dark = true;
    QString m_name;
    QString m_editorTheme;
    QStringList m_errors;

    ThemeColors *m_colors;
    ThemeFonts *m_fonts;
    ThemeSizes *m_sizes;
    ThemeSpace *m_space;
    ThemeRadius *m_radius;
    ThemeMotion *m_motion;
};

} // namespace owl::theme
