#pragma once

#include <QColor>
#include <QImage>

// Overlays two renders of the same sheet. Ink only on the older revision is
// painted red (removed), ink only on the newer one green (added), and ink on
// both is faded so the changes stand out against it.
namespace Compare {

inline const QColor removedColor{0xE0, 0x30, 0x2A};
inline const QColor addedColor{0x1F, 0xA3, 0x4A};

// Pixels darker than this count as ink.
constexpr int inkThreshold = 170;

// Ink that moved by up to this many pixels counts as unchanged, so
// anti-aliasing and rounding between two renders don't light up every line.
constexpr int defaultTolerance = 1;

// Both images are scaled to the newer one's size first, since revisions of a
// sheet share a sheet size even when their PDFs differ slightly in points.
QImage overlay(const QImage &older, const QImage &newer, int tolerance = defaultTolerance);

}
