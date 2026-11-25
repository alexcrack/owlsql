// SPDX-License-Identifier: GPL-3.0-or-later
#include "iconprovider.h"

#include <QColor>
#include <QFile>
#include <QLoggingCategory>
#include <QPainter>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QSvgRenderer>
#include <QUrlQuery>

Q_STATIC_LOGGING_CATEGORY(lcIcons, "owlsql.icons")

namespace owl::theme {

namespace {
constexpr int defaultSize = 16;
}

IconProvider::IconProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{ }

void IconProvider::install(QQmlEngine &engine)
{
    const QString id = QLatin1String(providerId);
    if (!engine.imageProvider(id))
        engine.addImageProvider(id, new IconProvider); // engine takes ownership
}

QImage IconProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const qsizetype queryStart = id.indexOf(QLatin1Char('?'));
    const QString name = id.left(queryStart);
    const QUrlQuery query(queryStart < 0 ? QString() : id.mid(queryStart + 1));

    // Only plain icon names: no paths into other resources.
    static const QRegularExpression validName(QStringLiteral("^[a-z0-9]+(-[a-z0-9]+)*$"));
    if (!validName.match(name).hasMatch()) {
        qCWarning(lcIcons) << "Invalid icon name" << name;
        return {};
    }

    QFile file(QStringLiteral(":/owlsql/icons/tabler/%1.svg").arg(name));
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcIcons) << "Unknown icon" << name;
        return {};
    }
    QByteArray svg = file.readAll();

    QColor color
        = QColor::fromString(query.queryItemValue(QStringLiteral("color"), QUrl::FullyDecoded));
    if (!color.isValid())
        color = Qt::black;
    svg.replace("currentColor", color.name(QColor::HexRgb).toLatin1());

    bool strokeOk = false;
    const double stroke = query.queryItemValue(QStringLiteral("stroke")).toDouble(&strokeOk);
    if (strokeOk && stroke > 0 && stroke <= 4)
        svg.replace("stroke-width=\"2\"",
                    QByteArray(QByteArray("stroke-width=\"") + QByteArray::number(stroke) + '"'));

    const QSize target(requestedSize.width() > 0 ? requestedSize.width() : defaultSize,
                       requestedSize.height() > 0 ? requestedSize.height() : defaultSize);

    QImage image(target, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QSvgRenderer renderer(svg);
    if (!renderer.isValid()) {
        qCWarning(lcIcons) << "Cannot render icon" << name;
        return {};
    }
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setOpacity(color.alphaF());
    renderer.render(&painter);
    painter.end();

    if (size)
        *size = target;
    return image;
}

} // namespace owl::theme
