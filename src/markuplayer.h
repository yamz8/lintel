#pragma once

#include <QPointer>
#include <QQuickPaintedItem>
#include <QVariantMap>

#include "markups.h"

// Draws one sheet's markups over the view. It covers only the viewport and
// maps sheet points onto it itself, so zooming far into a big sheet never
// asks for a texture the size of the whole zoomed sheet.
class MarkupLayer : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(Markups *markups READ markups WRITE setMarkups NOTIFY changed)
    Q_PROPERTY(int sheet MEMBER m_sheet NOTIFY changed)
    Q_PROPERTY(QSizeF sheetSize MEMBER m_sheetSize NOTIFY changed)
    // Where the sheet's top left corner is in this item, and its zoom.
    Q_PROPERTY(QPointF origin MEMBER m_origin NOTIFY changed)
    Q_PROPERTY(qreal pixelsPerPoint MEMBER m_pixelsPerPoint NOTIFY changed)
    Q_PROPERTY(int selected MEMBER m_selected NOTIFY changed)
    Q_PROPERTY(QPointF moveOffset MEMBER m_moveOffset NOTIFY changed)
    // A markup being drawn: {type, points: [start, end]}, or empty.
    Q_PROPERTY(QVariantMap draft MEMBER m_draft NOTIFY changed)

public:
    explicit MarkupLayer(QQuickItem *parent = nullptr);

    Markups *markups() const { return m_markups; }
    void setMarkups(Markups *markups);
    void paint(QPainter *painter) override;

signals:
    void changed();

private:
    QPointer<Markups> m_markups;
    int m_sheet = -1;
    QSizeF m_sheetSize;
    QPointF m_origin;
    qreal m_pixelsPerPoint = 1;
    int m_selected = -1;
    QPointF m_moveOffset;
    QVariantMap m_draft;
};
