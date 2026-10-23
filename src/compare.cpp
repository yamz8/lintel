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
