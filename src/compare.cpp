#include "compare.h"

#include <QPainter>

#include <algorithm>
#include <vector>

namespace {

enum Pixel : quint8 { Paper, Unchanged, Removed, Added };

// One byte per pixel: 1 where there's ink.
std::vector<quint8> inkMask(const QImage &image) {
    const int width = image.width(), height = image.height();
    std::vector<quint8> mask(size_t(width) * height);
    for (int y = 0; y < height; ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        quint8 *out = mask.data() + size_t(y) * width;
        for (int x = 0; x < width; ++x)
            out[x] = qGray(line[x]) < Compare::inkThreshold;
    }
    return mask;
}

// Grows the ink by radius pixels in every direction (a square max filter),
// in two separable passes so it stays linear in the radius.
std::vector<quint8> dilate(const std::vector<quint8> &mask, int width, int height, int radius) {
    if (radius <= 0)
        return mask;
    std::vector<quint8> rows(mask.size()), out(mask.size());
    for (int y = 0; y < height; ++y) {
        const quint8 *in = mask.data() + size_t(y) * width;
        quint8 *row = rows.data() + size_t(y) * width;
        for (int x = 0; x < width; ++x) {
            quint8 any = 0;
            for (int dx = qMax(0, x - radius); dx <= qMin(width - 1, x + radius) && !any; ++dx)
                any = in[dx];
            row[x] = any;
        }
    }
    for (int y = 0; y < height; ++y) {
        quint8 *row = out.data() + size_t(y) * width;
        for (int x = 0; x < width; ++x) {
            quint8 any = 0;
            for (int dy = qMax(0, y - radius); dy <= qMin(height - 1, y + radius) && !any; ++dy)
                any = rows[size_t(dy) * width + x];
            row[x] = any;
        }
    }
    return out;
}

// Unchanged ink keeps its shade, washed out toward the paper.
QRgb faded(QRgb pixel) {
    const int value = 255 - (255 - qGray(pixel)) * 45 / 100;
    return qRgb(value, value, value);
}

// PDF renders have a transparent background; put them on white paper at the
// given size so blank and ink compare the same way in both revisions.
QImage onPaper(const QImage &image, const QSize &size) {
    QImage paper(size, QImage::Format_RGB32);
    paper.fill(Qt::white);
    if (!image.isNull()) {
        QPainter painter(&paper);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.drawImage(QRect(QPoint(0, 0), size), image);
    }
    return paper;
}

// The two revisions on paper, and what each pixel is. The overlay and the
// change list both come from this, so they always agree.
struct Classified {
    QImage older, newer;
    std::vector<quint8> pixels;
};

Classified classify(const QImage &older, const QImage &newer, int tolerance) {
    const QSize size = newer.size();
    Classified result{onPaper(older, size), onPaper(newer, size), {}};
    const int width = size.width(), height = size.height();
    const auto inkA = inkMask(result.older);
    const auto inkB = inkMask(result.newer);
    const auto nearA = dilate(inkA, width, height, tolerance);
    const auto nearB = dilate(inkB, width, height, tolerance);
    result.pixels.resize(inkA.size());
    for (size_t i = 0; i < inkA.size(); ++i)
        result.pixels[i] = inkB[i] ? (nearA[i] ? Unchanged : Added)
                         : inkA[i] ? (nearB[i] ? Unchanged : Removed)
                                   : Paper;
    return result;
}

}

QTransform Compare::Alignment::transform(const QSizeF &size) const {
    return QTransform(scaleX, 0, 0, scaleY, offsetX * size.width(), offsetY * size.height());
}

QImage Compare::aligned(const QImage &older, const QSize &size, const Alignment &alignment) {
    if (alignment.isIdentity())
        return onPaper(older, size);
    QImage paper(size, QImage::Format_RGB32);
    paper.fill(Qt::white);
    if (!older.isNull()) {
        QPainter painter(&paper);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.setTransform(alignment.transform(size));
        painter.drawImage(QRect(QPoint(0, 0), size), older);
    }
    return paper;
}

namespace {

// Ink per row and per column.
struct Profiles {
    std::vector<int> rows, columns;
};

Profiles profiles(const std::vector<quint8> &ink, int width, int height) {
    Profiles p{std::vector<int>(height, 0), std::vector<int>(width, 0)};
    for (int y = 0; y < height; ++y) {
        const quint8 *line = ink.data() + size_t(y) * width;
        for (int x = 0; x < width; ++x) {
            p.rows[y] += line[x];
            p.columns[x] += line[x];
        }
    }
    return p;
}

// First and last index with real ink, ignoring the odd speck.
std::pair<int, int> extent(const std::vector<int> &profile) {
    int first = -1, last = -1;
    for (int i = 0; i < int(profile.size()); ++i) {
        if (profile[i] >= 2) {
            if (first < 0)
                first = i;
            last = i;
        }
    }
    return {first, last};
}

// The shift of b against a, within reach, that lines their profiles up
// best, to a fraction of a pixel: a parabola through the best score and its
// neighbors puts the peak between whole pixels.
double bestShift(const std::vector<int> &a, const std::vector<int> &b, int reach) {
    const int n = int(a.size());
    auto score = [&](int shift) {
        long long total = 0;
        for (int i = qMax(0, shift); i < qMin(n, n + shift); ++i)
            total += qMin(a[i - shift], b[i]);
        return double(total);
    };
    int best = 0;
    double bestScore = -1;
    for (int shift = -reach; shift <= reach; ++shift) {
        const double s = score(shift);
        if (s > bestScore || (s == bestScore && qAbs(shift) < qAbs(best))) {
            bestScore = s;
            best = shift;
        }
    }
    const double before = score(best - 1), after = score(best + 1);
    const double curvature = before - 2 * bestScore + after;
    return curvature < 0 ? best + qBound(-0.5, (before - after) / (2 * curvature), 0.5) : best;
}

int changedPixels(const QImage &older, const QImage &newer) {
    const Classified c = classify(older, newer, Compare::defaultTolerance);
    return int(std::count_if(c.pixels.begin(), c.pixels.end(), [](quint8 p) { return p >= Removed; }));
}

}

Compare::Alignment Compare::align(const QImage &olderImage, const QImage &newerImage) {
    if (olderImage.isNull() || newerImage.isNull())
        return {};
    const QSize size = newerImage.size();
    const QImage older = onPaper(olderImage, size), newer = onPaper(newerImage, size);
    const int width = size.width(), height = size.height();
    const Profiles a = profiles(inkMask(older), width, height);
    const Profiles b = profiles(inkMask(newer), width, height);
    const auto [ax0, ax1] = extent(a.columns);
    const auto [ay0, ay1] = extent(a.rows);
    const auto [bx0, bx1] = extent(b.columns);
    const auto [by0, by1] = extent(b.rows);
    if (ax0 < 0 || bx0 < 0 || ax1 <= ax0 || ay1 <= ay0 || bx1 <= bx0 || by1 <= by0)
        return {};

    // The frame around a sheet makes the extent of its ink a good guide to
    // scale. More than a few percent is a different drawing, not a re-plot.
    Alignment candidate;
    const double sx = double(bx1 - bx0) / (ax1 - ax0), sy = double(by1 - by0) / (ay1 - ay0);
    if (qAbs(sx - 1) < 0.04 && qAbs(sy - 1) < 0.04 && qAbs(sx - sy) < 0.01) {
        candidate.scaleX = sx;
        candidate.scaleY = sy;
    }
    candidate.offsetX = (bx0 - ax0 * candidate.scaleX) / width;
    candidate.offsetY = (by0 - ay0 * candidate.scaleY) / height;

    // Then line the ink up row by row and column by column, up to 2% away.
    const QImage moved = aligned(older, size, candidate);
    const Profiles m = profiles(inkMask(moved), width, height);
    candidate.offsetX += bestShift(m.columns, b.columns, width / 50) / width;
    candidate.offsetY += bestShift(m.rows, b.rows, height / 50) / height;

    // Snap away sub-pixel noise, so an unmoved sheet stays exactly put.
    auto snap = [](double value, double unit, double rest) { return qAbs(value - rest) < unit / 2 ? rest : value; };
    candidate.scaleX = snap(candidate.scaleX, 1.0 / width, 1);
    candidate.scaleY = snap(candidate.scaleY, 1.0 / height, 1);
    candidate.offsetX = snap(candidate.offsetX, 1.0 / width, 0);
    candidate.offsetY = snap(candidate.offsetY, 1.0 / height, 0);
    if (candidate.isIdentity())
        return {};

    // Keep it only when it clears a good share of the differences.
    const int before = changedPixels(older, newer);
    const int after = changedPixels(aligned(older, size, candidate), newer);
    return after < before * 0.7 ? candidate : Alignment{};
}

QImage Compare::overlay(const QImage &older, const QImage &newer, int tolerance) {
    if (newer.isNull())
        return {};
    const Classified c = classify(older, newer, tolerance);
    const int width = newer.width(), height = newer.height();
    const QRgb removed = removedColor.rgb(), added = addedColor.rgb();
    QImage result(newer.size(), QImage::Format_RGB32);
    for (int y = 0; y < height; ++y) {
        const auto *lineA = reinterpret_cast<const QRgb *>(c.older.constScanLine(y));
        const auto *lineB = reinterpret_cast<const QRgb *>(c.newer.constScanLine(y));
        const quint8 *kind = c.pixels.data() + size_t(y) * width;
        auto *out = reinterpret_cast<QRgb *>(result.scanLine(y));
        for (int x = 0; x < width; ++x) {
            switch (kind[x]) {
            case Added: out[x] = added; break;
            case Removed: out[x] = removed; break;
            // Paper keeps the anti-aliased edges of unchanged lines and text.
            default: out[x] = faded(qGray(lineA[x]) < qGray(lineB[x]) ? lineA[x] : lineB[x]);
            }
        }
    }
    return result;
}

Compare::Changes Compare::changes(const QImage &older, const QImage &newer, int tolerance) {
    Changes result;
    if (newer.isNull())
        return result;
    const Classified c = classify(older, newer, tolerance);
    const int width = newer.width(), height = newer.height();

    // Count changed pixels in square cells, about 1/250 of the long edge.
    const int cell = qMax(4, qMax(width, height) / 250);
    const int columns = (width + cell - 1) / cell, rows = (height + cell - 1) / cell;
    std::vector<int> counts(size_t(columns) * rows, 0);
    for (int y = 0; y < height; ++y) {
        const quint8 *kind = c.pixels.data() + size_t(y) * width;
        for (int x = 0; x < width; ++x) {
            if (kind[x] == Removed)
                ++result.removed;
            else if (kind[x] == Added)
                ++result.added;
            else
                continue;
            ++counts[size_t(y / cell) * columns + x / cell];
        }
    }

    // A cell with a single stray pixel is anti-aliasing noise, not a change.
    const int cellMinimum = 2;
    std::vector<quint8> changed(counts.size());
    for (size_t i = 0; i < counts.size(); ++i)
        changed[i] = counts[i] >= cellMinimum;

    // Changes within a few cells of each other belong to one region.
    const int gap = 3;
    const auto near = dilate(changed, columns, rows, gap);
    std::vector<int> component(counts.size(), -1);
    std::vector<int> stack;
    for (int start = 0; start < int(near.size()); ++start) {
        if (!near[start] || component[start] >= 0)
            continue;
        int left = columns, top = rows, right = -1, bottom = -1, total = 0;
        stack.assign(1, start);
        component[start] = start;
        while (!stack.empty()) {
            const int i = stack.back();
            stack.pop_back();
            const int cx = i % columns, cy = i / columns;
            if (changed[i]) {
                left = qMin(left, cx);
                right = qMax(right, cx);
                top = qMin(top, cy);
                bottom = qMax(bottom, cy);
                total += counts[i];
            }
            for (const auto [dx, dy] : {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
                const int nx = cx + dx, ny = cy + dy;
                if (nx < 0 || ny < 0 || nx >= columns || ny >= rows)
                    continue;
                const int n = ny * columns + nx;
                if (near[n] && component[n] < 0) {
                    component[n] = start;
                    stack.push_back(n);
                }
            }
        }
        if (right < 0 || total < cell)
            continue;
        // One cell of margin, so the change isn't flush with the frame.
        const QRectF pixels(QPointF(qMax(0, left - 1) * cell, qMax(0, top - 1) * cell),
                            QPointF(qMin(width, (right + 2) * cell), qMin(height, (bottom + 2) * cell)));
        result.regions.append(QRectF(pixels.x() / width, pixels.y() / height,
                                     pixels.width() / width, pixels.height() / height));
    }

    // Regions whose margins overlap read as one change.
    for (bool merged = true; merged;) {
        merged = false;
        for (int i = 0; i < result.regions.size() && !merged; ++i) {
            for (int j = i + 1; j < result.regions.size() && !merged; ++j) {
                if (result.regions[i].intersects(result.regions[j])) {
                    result.regions[i] = result.regions[i].united(result.regions[j]);
                    result.regions.removeAt(j);
                    merged = true;
                }
            }
        }
    }

    // Reading order: top to bottom in bands, then left to right.
    std::sort(result.regions.begin(), result.regions.end(), [](const QRectF &a, const QRectF &b) {
        const int bandA = int(a.center().y() * 8), bandB = int(b.center().y() * 8);
        return bandA != bandB ? bandA < bandB : a.center().x() < b.center().x();
    });
    return result;
}
