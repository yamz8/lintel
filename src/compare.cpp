#include "compare.h"

#include <QPainter>

#include <vector>

namespace {

// One byte per pixel: 1 where there's ink.
std::vector<quint8> inkMask(const QImage &image) {
    const int width = image.width(), height = image.height();
    std::vector<quint8> mask(size_t(width) * height);
    for (int y = 0; y < height; ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        quint8 *out = mask.data() + size_t(y) * width;
        for (int x = 0; x < width; ++x) {
            out[x] = qGray(line[x]) < Compare::inkThreshold;
        }
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

}

QImage Compare::overlay(const QImage &older, const QImage &newer, int tolerance) {
    if (newer.isNull())
        return {};
    const QSize size = newer.size();
    const QImage b = onPaper(newer, size);
    const QImage a = onPaper(older, size);

    const int width = size.width(), height = size.height();
    const auto inkB = inkMask(b);
    const auto inkA = inkMask(a);
    const auto nearA = dilate(inkA, width, height, tolerance);
    const auto nearB = dilate(inkB, width, height, tolerance);

    const QRgb removed = removedColor.rgb(), added = addedColor.rgb();
    QImage result(size, QImage::Format_RGB32);
    for (int y = 0; y < height; ++y) {
        const auto *lineA = reinterpret_cast<const QRgb *>(a.constScanLine(y));
        const auto *lineB = reinterpret_cast<const QRgb *>(b.constScanLine(y));
        auto *out = reinterpret_cast<QRgb *>(result.scanLine(y));
        const size_t row = size_t(y) * width;
        for (int x = 0; x < width; ++x) {
            const size_t i = row + x;
            if (inkB[i])
                out[x] = nearA[i] ? faded(lineB[x]) : added;
            else if (inkA[i])
                out[x] = nearB[i] ? faded(lineA[x]) : removed;
            else
                // Keeps the anti-aliased edges of unchanged lines and text.
                out[x] = faded(qGray(lineA[x]) < qGray(lineB[x]) ? lineA[x] : lineB[x]);
        }
    }
    return result;
}
