#include "markuppainter.h"

#include <QFontMetricsF>
#include <QLineF>
#include <QPainterPath>
#include <QtMath>

MarkupPainter::Style MarkupPainter::Style::forSheet(const QSizeF &sheet) {
    const qreal edge = qMax(sheet.width(), sheet.height());
    return {qMax(1.0, edge / 1000), qMax(9.0, edge / 150), qMax(6.0, edge / 130)};
}

namespace {

// Text with a white halo, so it reads over linework.
void haloText(QPainter *painter, const QPointF &baseline, const QString &text, const QFont &font, qreal halo) {
    QPainterPath path;
    path.addText(baseline, font, text);
    painter->strokePath(path, QPen(Qt::white, halo, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->fillPath(path, MarkupPainter::color);
}

QFont markupFont(qreal size) {
    QFont font("sans-serif");
    font.setPixelSize(qMax(1, qRound(size)));
    font.setWeight(QFont::DemiBold);
    return font;
}

// The scalloped outline revision clouds are drawn with: arcs bulging out
// along each side of the rectangle.
QPainterPath cloudPath(const QRectF &rect, qreal scallop) {
    QPainterPath path;
    const QPointF corners[] = {rect.topLeft(), rect.topRight(), rect.bottomRight(), rect.bottomLeft()};
    path.moveTo(corners[0]);
    for (int side = 0; side < 4; ++side) {
        const QPointF from = corners[side], to = corners[(side + 1) % 4];
        const QLineF edge(from, to);
        const int arcs = qMax(1, qRound(edge.length() / scallop));
        // Outward is to the left of travel for a clockwise rectangle in
        // screen coordinates (y down).
        const QPointF outward = edge.length() > 0
            ? QPointF(edge.dy(), -edge.dx()) / edge.length() : QPointF();
        for (int i = 0; i < arcs; ++i) {
            const QPointF a = from + (to - from) * (qreal(i) / arcs);
            const QPointF b = from + (to - from) * (qreal(i + 1) / arcs);
            const QPointF bulge = outward * (edge.length() / arcs) * 0.55;
            path.cubicTo(a + (b - a) * 0.05 + bulge, b - (b - a) * 0.05 + bulge, b);
        }
    }
    return path;
}

QRectF bounds(const Markup &markup, const SheetScale &scale, const MarkupPainter::Style &style) {
    if (markup.type == "text") {
        const QFontMetricsF metrics(markupFont(style.text));
        return QRectF(markup.points[0], QSizeF(metrics.horizontalAdvance(markup.text), metrics.height()));
    }
    QRectF rect = QRectF(markup.points[0], markup.points[1]).normalized();
    if (markup.type == "cloud")
        rect.adjust(-style.scallop * 0.6, -style.scallop * 0.6, style.scallop * 0.6, style.scallop * 0.6);
    if (markup.type == "measure") {
        const QFontMetricsF metrics(markupFont(style.text * 0.85));
        const qreal label = metrics.horizontalAdvance(Markups::formatLength(scale, QLineF(markup.points[0], markup.points[1]).length()));
        rect = rect.united(QRectF(rect.center() - QPointF(label / 2, metrics.height() * 1.5), QSizeF(label, metrics.height() * 1.5)));
    }
    return rect;
}

}

void MarkupPainter::paintOne(QPainter *painter, const Markup &markup, const SheetScale &scale, const Style &style,
                             bool selected) {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const QPen pen(color, style.line, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    if (markup.type == "cloud") {
        painter->drawPath(cloudPath(QRectF(markup.points[0], markup.points[1]).normalized(), style.scallop));
    } else if (markup.type == "arrow") {
        const QLineF line(markup.points[0], markup.points[1]);
        const qreal head = style.line * 7;
        // The shaft stops at the head, so the point stays sharp.
        const QLineF shaft(line.p1(), line.length() > head ? line.pointAt(1 - head / line.length()) : line.p1());
        painter->drawLine(shaft);
        const qreal angle = qAtan2(line.dy(), line.dx());
        QPolygonF tip;
        tip << line.p2()
            << line.p2() - QPointF(qCos(angle - 0.4), qSin(angle - 0.4)) * head
            << line.p2() - QPointF(qCos(angle + 0.4), qSin(angle + 0.4)) * head;
        painter->setBrush(color);
        painter->drawPolygon(tip);
    } else if (markup.type == "text") {
        const QFont font = markupFont(style.text);
        haloText(painter, markup.points[0] + QPointF(0, QFontMetricsF(font).ascent()), markup.text, font, style.line * 3);
    } else if (markup.type == "measure") {
        const QLineF line(markup.points[0], markup.points[1]);
        painter->drawLine(line);
        // Ticks across both ends, as on a dimension string.
        const QPointF across = line.length() > 0
            ? QPointF(-line.dy(), line.dx()) / line.length() * style.line * 5 : QPointF();
        painter->drawLine(line.p1() - across, line.p1() + across);
        painter->drawLine(line.p2() - across, line.p2() + across);

        const QFont font = markupFont(style.text * 0.85);
        const QString label = Markups::formatLength(scale, line.length());
        const QFontMetricsF metrics(font);
        // Along the line, kept upright, just above it.
        qreal angle = qRadiansToDegrees(qAtan2(line.dy(), line.dx()));
        if (angle > 90 || angle < -90)
            angle += 180;
        painter->translate(line.center());
        painter->rotate(angle);
        haloText(painter, QPointF(-metrics.horizontalAdvance(label) / 2, -style.line * 3), label, font, style.line * 3);
    }

    painter->restore();
    if (selected) {
        painter->save();
        QPen dashed(color, style.line * 0.75, Qt::DashLine);
        painter->setPen(dashed);
        painter->setBrush(Qt::NoBrush);
        const qreal pad = style.line * 4;
        painter->drawRect(bounds(markup, scale, style).adjusted(-pad, -pad, pad, pad));
        painter->restore();
    }
}

void MarkupPainter::paint(QPainter *painter, const QVector<Markup> &markups, const SheetScale &scale,
                          const QSizeF &sheet, int selectedId, const QPointF &selectedOffset) {
    const Style style = Style::forSheet(sheet);
    for (Markup markup : markups) {
        const bool selected = markup.id == selectedId;
        if (selected)
            for (QPointF &p : markup.points)
                p += selectedOffset;
        paintOne(painter, markup, scale, style, selected);
    }
}
