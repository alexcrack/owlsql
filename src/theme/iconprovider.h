// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QQuickImageProvider>

class QQmlEngine;

namespace owl::theme {

// Serves tinted icons: image://owlicon/<name>?color=<#rrggbb|#aarrggbb>[&stroke=<width>]
// <name> is a Tabler outline icon bundled under :/owlsql/icons/tabler/.
class IconProvider : public QQuickImageProvider
{
public:
    static constexpr auto providerId = "owlicon";

    IconProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

    // Adds the provider to the engine (QML engines need this once, before loading QML).
    static void install(QQmlEngine &engine);
};

} // namespace owl::theme
