#pragma once

#include <QList>
#include <QSizeF>
#include <QString>
#include <QVector>

class Markups;

namespace Exporter {

// Writes a copy of the PDF at pdfPath with the markups drawn on its pages,
// keeping only the given sheets (0-based, in order; every sheet when empty).
// The drawings stay as they are, vector and searchable: the markups are
// drawn on transparent pages that qpdf lays over the originals. The output
// appears whole or not at all. On failure, error says why.
bool exportMarkedUp(const QString &pdfPath, const QVector<QSizeF> &sheetSizes, const Markups &markups,
                    const QList<int> &sheets, const QString &output, QString *error);

}
