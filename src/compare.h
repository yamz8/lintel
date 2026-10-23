#pragma once

#include <QColor>
#include <QImage>
#include <QRectF>
#include <QTransform>
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

// How the older revision's drawing sits on the newer one's sheet: a point at
// p (fractions of the sheet) on the older one lands at p * scale + offset.
struct Alignment {
    double scaleX = 1, scaleY = 1;
    double offsetX = 0, offsetY = 0;
    bool isIdentity() const { return scaleX == 1 && scaleY == 1 && offsetX == 0 && offsetY == 0; }
    // Maps the older render (of size) onto the newer one's pixels.
    QTransform transform(const QSizeF &size) const;
};

// Finds the shift and scale that line the older revision up with the newer
// one, as happens when a set is re-plotted with different margins. Returns
// the identity unless lining up removes a good share of the differences, so
// real changes near the frame never read as a shift.
Alignment align(const QImage &older, const QImage &newer);

// The older render on white paper of size, moved by alignment.
QImage aligned(const QImage &older, const QSize &size, const Alignment &alignment);

// Paints the overlay: removed ink red, added ink green, unchanged ink faded.
// Both images are scaled to the newer one's size first, since revisions of a
// sheet share a sheet size even when their PDFs differ slightly in points.
QImage overlay(const QImage &older, const QImage &newer, int tolerance = defaultTolerance);

// Finds the changed areas, grouping changes that sit close together so a
// moved wall is one region, not dozens of pixels.
Changes changes(const QImage &older, const QImage &newer, int tolerance = defaultTolerance);

}
