#include "markuplayer.h"

#include "markuppainter.h"

MarkupLayer::MarkupLayer(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
    connect(this, &MarkupLayer::changed, this, [this] { update(); });
}

void MarkupLayer::setMarkups(Markups *markups) {
    if (m_markups == markups)
        return;
    if (m_markups)
        disconnect(m_markups, nullptr, this, nullptr);
    m_markups = markups;
    if (m_markups)
        connect(m_markups, &Markups::changed, this, [this] { update(); });
    emit changed();
}

void MarkupLayer::paint(QPainter *painter) {
    if (!m_markups || m_sheet < 0 || m_sheetSize.isEmpty())
        return;
    painter->translate(m_origin);
    painter->scale(m_pixelsPerPoint, m_pixelsPerPoint);
    const SheetScale scale = m_markups->scaleFor(m_sheet);
    MarkupPainter::paint(painter, m_markups->forSheet(m_sheet), scale, m_sheetSize, m_selected, m_moveOffset);

    const QVariantList points = m_draft.value("points").toList();
    if (points.size() == 2) {
        Markup draft{0, m_draft.value("type").toString(), {points[0].toPointF(), points[1].toPointF()}, {}};
        // Calibrating draws as a measurement with no length yet.
        if (draft.type == "calibrate")
            draft.type = "measure";
        MarkupPainter::paintOne(painter, draft, draft.type == "measure" ? scale : SheetScale(),
                                MarkupPainter::Style::forSheet(m_sheetSize), false);
    }
}
