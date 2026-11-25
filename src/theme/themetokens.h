// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QFont>
#include <QHash>
#include <QObject>
#include <QtQmlIntegration/qqmlintegration.h>

// Token groups exposed to QML as Theme.color / font / size / space / radius / motion.
// Each group lists its tokens once in an X-macro, which generates storage and getters.
// The Q_PROPERTY lines must be spelled out for moc; a missing getter fails to compile and the
// theme tests verify that every JSON file defines exactly these tokens.

namespace owl::theme {

class ThemeManager;

// clang-format off
#define OWL_THEME_COLOR_TOKENS(X)                                                                  \
    X(bgCanvas) X(bgSurface) X(bgRaised) X(bgSunken) X(bgHover) X(bgSelected)                      \
    X(bgSelectedInactive)                                                                          \
    X(borderSubtle) X(borderDefault) X(borderStrong)                                               \
    X(textPrimary) X(textSecondary) X(textMuted) X(textDisabled)                                   \
    X(accentFill) X(accentFillHover) X(accentFillPressed) X(accentOnFill) X(accentLine)            \
    X(accentText) X(accentSubtle)                                                                  \
    X(danger) X(dangerSubtle) X(warning) X(warningSubtle) X(success) X(successSubtle)              \
    X(info) X(infoSubtle)                                                                          \
    X(tagRed) X(tagOrange) X(tagAmber) X(tagGreen) X(tagTeal) X(tagBlue) X(tagViolet) X(tagPink)   \
    X(gridNumber) X(gridDate) X(gridBoolean) X(gridJson) X(gridBinary)                             \
    X(shadow)

#define OWL_THEME_FONT_TOKENS(X) X(ui) X(uiSmall) X(uiTiny) X(title) X(heading) X(code) X(grid)

#define OWL_THEME_SIZE_TOKENS(X)                                                                   \
    X(row) X(control) X(controlSmall) X(tabBar) X(toolbar) X(statusBar) X(icon) X(iconSmall)       \
    X(treeIndent) X(splitterHit)

#define OWL_THEME_SPACE_TOKENS(X) X(xxs) X(xs) X(s) X(m) X(l) X(xl) X(xxl) X(xxxl)

#define OWL_THEME_RADIUS_TOKENS(X) X(control) X(popup) X(pill)
// clang-format on

#define OWL_TOKEN_GETTER(type, name) \
    type name() const                \
    {                                \
        return m_##name;             \
    }
#define OWL_TOKEN_SLOT(type, name) {QStringLiteral(#name), &m_##name},

#define OWL_COLOR_GETTER(name) OWL_TOKEN_GETTER(QColor, name)
#define OWL_COLOR_MEMBER(name) QColor m_##name;
#define OWL_COLOR_SLOT(name) OWL_TOKEN_SLOT(QColor, name)
#define OWL_FONT_GETTER(name) OWL_TOKEN_GETTER(QFont, name)
#define OWL_FONT_MEMBER(name) QFont m_##name;
#define OWL_FONT_SLOT(name) OWL_TOKEN_SLOT(QFont, name)
#define OWL_INT_GETTER(name) OWL_TOKEN_GETTER(int, name)
#define OWL_INT_MEMBER(name) int m_##name = 0;
#define OWL_INT_SLOT(name) OWL_TOKEN_SLOT(int, name)

class ThemeColors : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(QColor bgCanvas READ bgCanvas NOTIFY changed FINAL)
    Q_PROPERTY(QColor bgSurface READ bgSurface NOTIFY changed FINAL)
    Q_PROPERTY(QColor bgRaised READ bgRaised NOTIFY changed FINAL)
    Q_PROPERTY(QColor bgSunken READ bgSunken NOTIFY changed FINAL)
    Q_PROPERTY(QColor bgHover READ bgHover NOTIFY changed FINAL)
    Q_PROPERTY(QColor bgSelected READ bgSelected NOTIFY changed FINAL)
    Q_PROPERTY(QColor bgSelectedInactive READ bgSelectedInactive NOTIFY changed FINAL)
    Q_PROPERTY(QColor borderSubtle READ borderSubtle NOTIFY changed FINAL)
    Q_PROPERTY(QColor borderDefault READ borderDefault NOTIFY changed FINAL)
    Q_PROPERTY(QColor borderStrong READ borderStrong NOTIFY changed FINAL)
    Q_PROPERTY(QColor textPrimary READ textPrimary NOTIFY changed FINAL)
    Q_PROPERTY(QColor textSecondary READ textSecondary NOTIFY changed FINAL)
    Q_PROPERTY(QColor textMuted READ textMuted NOTIFY changed FINAL)
    Q_PROPERTY(QColor textDisabled READ textDisabled NOTIFY changed FINAL)
    Q_PROPERTY(QColor accentFill READ accentFill NOTIFY changed FINAL)
    Q_PROPERTY(QColor accentFillHover READ accentFillHover NOTIFY changed FINAL)
    Q_PROPERTY(QColor accentFillPressed READ accentFillPressed NOTIFY changed FINAL)
    Q_PROPERTY(QColor accentOnFill READ accentOnFill NOTIFY changed FINAL)
    Q_PROPERTY(QColor accentLine READ accentLine NOTIFY changed FINAL)
    Q_PROPERTY(QColor accentText READ accentText NOTIFY changed FINAL)
    Q_PROPERTY(QColor accentSubtle READ accentSubtle NOTIFY changed FINAL)
    Q_PROPERTY(QColor danger READ danger NOTIFY changed FINAL)
    Q_PROPERTY(QColor dangerSubtle READ dangerSubtle NOTIFY changed FINAL)
    Q_PROPERTY(QColor warning READ warning NOTIFY changed FINAL)
    Q_PROPERTY(QColor warningSubtle READ warningSubtle NOTIFY changed FINAL)
    Q_PROPERTY(QColor success READ success NOTIFY changed FINAL)
    Q_PROPERTY(QColor successSubtle READ successSubtle NOTIFY changed FINAL)
    Q_PROPERTY(QColor info READ info NOTIFY changed FINAL)
    Q_PROPERTY(QColor infoSubtle READ infoSubtle NOTIFY changed FINAL)
    Q_PROPERTY(QColor tagRed READ tagRed NOTIFY changed FINAL)
    Q_PROPERTY(QColor tagOrange READ tagOrange NOTIFY changed FINAL)
    Q_PROPERTY(QColor tagAmber READ tagAmber NOTIFY changed FINAL)
    Q_PROPERTY(QColor tagGreen READ tagGreen NOTIFY changed FINAL)
    Q_PROPERTY(QColor tagTeal READ tagTeal NOTIFY changed FINAL)
    Q_PROPERTY(QColor tagBlue READ tagBlue NOTIFY changed FINAL)
    Q_PROPERTY(QColor tagViolet READ tagViolet NOTIFY changed FINAL)
    Q_PROPERTY(QColor tagPink READ tagPink NOTIFY changed FINAL)
    Q_PROPERTY(QColor gridNumber READ gridNumber NOTIFY changed FINAL)
    Q_PROPERTY(QColor gridDate READ gridDate NOTIFY changed FINAL)
    Q_PROPERTY(QColor gridBoolean READ gridBoolean NOTIFY changed FINAL)
    Q_PROPERTY(QColor gridJson READ gridJson NOTIFY changed FINAL)
    Q_PROPERTY(QColor gridBinary READ gridBinary NOTIFY changed FINAL)
    Q_PROPERTY(QColor shadow READ shadow NOTIFY changed FINAL)

public:
    using QObject::QObject;
    OWL_THEME_COLOR_TOKENS(OWL_COLOR_GETTER)

signals:
    void changed();

private:
    friend class ThemeManager;
    QHash<QString, QColor *> tokenSlots() { return {OWL_THEME_COLOR_TOKENS(OWL_COLOR_SLOT)}; }
    OWL_THEME_COLOR_TOKENS(OWL_COLOR_MEMBER)
};

class ThemeFonts : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(QFont ui READ ui NOTIFY changed FINAL)
    Q_PROPERTY(QFont uiSmall READ uiSmall NOTIFY changed FINAL)
    Q_PROPERTY(QFont uiTiny READ uiTiny NOTIFY changed FINAL)
    Q_PROPERTY(QFont title READ title NOTIFY changed FINAL)
    Q_PROPERTY(QFont heading READ heading NOTIFY changed FINAL)
    Q_PROPERTY(QFont code READ code NOTIFY changed FINAL)
    Q_PROPERTY(QFont grid READ grid NOTIFY changed FINAL)

public:
    using QObject::QObject;
    OWL_THEME_FONT_TOKENS(OWL_FONT_GETTER)

signals:
    void changed();

private:
    friend class ThemeManager;
    QHash<QString, QFont *> tokenSlots() { return {OWL_THEME_FONT_TOKENS(OWL_FONT_SLOT)}; }
    OWL_THEME_FONT_TOKENS(OWL_FONT_MEMBER)
};

class ThemeSizes : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(int row READ row NOTIFY changed FINAL)
    Q_PROPERTY(int control READ control NOTIFY changed FINAL)
    Q_PROPERTY(int controlSmall READ controlSmall NOTIFY changed FINAL)
    Q_PROPERTY(int tabBar READ tabBar NOTIFY changed FINAL)
    Q_PROPERTY(int toolbar READ toolbar NOTIFY changed FINAL)
    Q_PROPERTY(int statusBar READ statusBar NOTIFY changed FINAL)
    Q_PROPERTY(int icon READ icon NOTIFY changed FINAL)
    Q_PROPERTY(int iconSmall READ iconSmall NOTIFY changed FINAL)
    Q_PROPERTY(int treeIndent READ treeIndent NOTIFY changed FINAL)
    Q_PROPERTY(int splitterHit READ splitterHit NOTIFY changed FINAL)

public:
    using QObject::QObject;
    OWL_THEME_SIZE_TOKENS(OWL_INT_GETTER)

signals:
    void changed();

private:
    friend class ThemeManager;
    QHash<QString, int *> tokenSlots() { return {OWL_THEME_SIZE_TOKENS(OWL_INT_SLOT)}; }
    OWL_THEME_SIZE_TOKENS(OWL_INT_MEMBER)
};

class ThemeSpace : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(int xxs READ xxs NOTIFY changed FINAL)
    Q_PROPERTY(int xs READ xs NOTIFY changed FINAL)
    Q_PROPERTY(int s READ s NOTIFY changed FINAL)
    Q_PROPERTY(int m READ m NOTIFY changed FINAL)
    Q_PROPERTY(int l READ l NOTIFY changed FINAL)
    Q_PROPERTY(int xl READ xl NOTIFY changed FINAL)
    Q_PROPERTY(int xxl READ xxl NOTIFY changed FINAL)
    Q_PROPERTY(int xxxl READ xxxl NOTIFY changed FINAL)

public:
    using QObject::QObject;
    OWL_THEME_SPACE_TOKENS(OWL_INT_GETTER)

signals:
    void changed();

private:
    friend class ThemeManager;
    QHash<QString, int *> tokenSlots() { return {OWL_THEME_SPACE_TOKENS(OWL_INT_SLOT)}; }
    OWL_THEME_SPACE_TOKENS(OWL_INT_MEMBER)
};

class ThemeRadius : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(int control READ control NOTIFY changed FINAL)
    Q_PROPERTY(int popup READ popup NOTIFY changed FINAL)
    Q_PROPERTY(int pill READ pill NOTIFY changed FINAL)

public:
    using QObject::QObject;
    OWL_THEME_RADIUS_TOKENS(OWL_INT_GETTER)

signals:
    void changed();

private:
    friend class ThemeManager;
    QHash<QString, int *> tokenSlots() { return {OWL_THEME_RADIUS_TOKENS(OWL_INT_SLOT)}; }
    OWL_THEME_RADIUS_TOKENS(OWL_INT_MEMBER)
};

// Durations in ms; both report 0 while reduced motion is on.
class ThemeMotion : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(int fast READ fast NOTIFY changed FINAL)
    Q_PROPERTY(int normal READ normal NOTIFY changed FINAL)

public:
    using QObject::QObject;
    int fast() const { return m_reduced ? 0 : m_fast; }
    int normal() const { return m_reduced ? 0 : m_normal; }

signals:
    void changed();

private:
    friend class ThemeManager;
    QHash<QString, int *> tokenSlots()
    {
        return {{QStringLiteral("fast"), &m_fast}, {QStringLiteral("normal"), &m_normal}};
    }
    int m_fast = 0;
    int m_normal = 0;
    bool m_reduced = false;
};

} // namespace owl::theme
