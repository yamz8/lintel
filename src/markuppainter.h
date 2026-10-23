#pragma once

#include <QColor>
#include <QPainter>
#include <QSizeF>
#include <QVector>

#include "markups.h"

// Paints markups in sheet points, for the window and for exported PDFs alike,
// so a markup looks the same on screen and on paper.
namespace MarkupPainter {

// Blue: distinct from the overlay's red and green.
inline const QColor color{0x1E, 0x6B, 0xFF};

// Line widths and text sizes grow with the sheet, so marks read the same on
// a letter-size detail and a full-size plan.
struct Style {
    qreal line, text, scallop;
    static Style forSheet(const QSizeF &sheet);
};

void paint(QPainter *painter, const QVector<Markup> &markups, const SheetScale &scale, const QSizeF &sheet,
           int selectedId = -1, const QPointF &selectedOffset = {});
void paintOne(QPainter *painter, const Markup &markup, const SheetScale &scale, const Style &style, bool selected);

}
