#pragma once

#include <QColor>
#include <QImage>
#include <QRectF>
#include <QVector>

// Compares two renders of the same sheet. Ink only on the older revision is
// removed, ink only on the newer one added, and ink on both unchanged.
namespace Compare {

inline const QColor removedColor{0xE0, 0x30, 0x2A};
inline const QColor addedColor{0x1F, 0xA3, 0x4A};

// Pixels darker than this count as ink.
constexpr int inkThreshold = 170;

// Ink that moved by up to this many pixels counts as unchanged, so
// anti-aliasing and rounding between two renders don't light up every line.
constexpr int defaultTolerance = 1;

// What changed on a sheet: counts of changed pixels, and the areas around
// them as fractions of the sheet (0 to 1), in reading order.
struct Changes {
    int removed = 0;
    int added = 0;
    QVector<QRectF> regions;
    bool any() const { return !regions.isEmpty(); }
};

// Paints the overlay: removed ink red, added ink green, unchanged ink faded.
// Both images are scaled to the newer one's size first, since revisions of a
// sheet share a sheet size even when their PDFs differ slightly in points.
QImage overlay(const QImage &older, const QImage &newer, int tolerance = defaultTolerance);

// Finds the changed areas, grouping changes that sit close together so a
// moved wall is one region, not dozens of pixels.
Changes changes(const QImage &older, const QImage &newer, int tolerance = defaultTolerance);

}
