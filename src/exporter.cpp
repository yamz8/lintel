#include "exporter.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QPageLayout>
#include <QPainter>
#include <QPdfWriter>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>
#include <QTemporaryDir>
#include <cstdio>

#include "markuppainter.h"
#include "markups.h"

namespace {
QString tr(const char *text) { return QCoreApplication::translate("Exporter", text); }
}

bool Exporter::exportMarkedUp(const QString &pdfPath, const QVector<QSizeF> &sheetSizes, const Markups &markups,
                              const QList<int> &sheets, const QString &output, QString *error) {
    const QString qpdf = QStandardPaths::findExecutable("qpdf");
    if (qpdf.isEmpty()) {
        *error = tr("Exporting needs qpdf. Install it with: omarchy-pkg-add qpdf");
        return false;
    }
    QList<int> pages = sheets;
    if (pages.isEmpty())
        for (int sheet = 0; sheet < sheetSizes.size(); ++sheet)
            pages.append(sheet);
    for (const int sheet : pages) {
        if (sheet < 0 || sheet >= sheetSizes.size()) {
            *error = tr("There is no sheet %1 to export.").arg(sheet + 1);
            return false;
        }
    }
    if (QFileInfo(output).absoluteFilePath() == QFileInfo(pdfPath).absoluteFilePath()) {
        *error = tr("Export to a new file; the original drawing set is never changed.");
        return false;
    }

    QTemporaryDir temporary;
    if (!temporary.isValid()) {
        *error = tr("Could not make a temporary folder to export in.");
        return false;
    }

    // One transparent page per exported sheet, at its exact size, with its
    // markups drawn in PDF points.
    const QString overlay = temporary.filePath("markups.pdf");
    {
        QPdfWriter writer(overlay);
        writer.setResolution(72);
        writer.setCreator("Lintel");
        QPainter painter;
        for (int i = 0; i < pages.size(); ++i) {
            const QSizeF size = sheetSizes.at(pages[i]);
            writer.setPageLayout(QPageLayout(QPageSize(size, QPageSize::Point, {}, QPageSize::ExactMatch),
                                             QPageLayout::Portrait, QMarginsF()));
            if (i == 0) {
                if (!painter.begin(&writer)) {
                    *error = tr("Could not draw the markups.");
                    return false;
                }
            } else {
                writer.newPage();
            }
            MarkupPainter::paint(&painter, markups.forSheet(pages[i]), markups.scaleFor(pages[i]), size);
        }
        painter.end();
    }

    QStringList ranges;
    for (const int sheet : pages)
        ranges.append(QString::number(sheet + 1));

    // qpdf writes beside the target, which is then renamed over it.
    const QString partial = output + ".lintel-partial";
    QProcess process;
    process.start(qpdf, {pdfPath, "--pages", pdfPath, ranges.join(','), "--",
                         "--overlay", overlay, "--", partial});
    const bool finished = process.waitForFinished(10 * 60 * 1000);
    // qpdf exits 3 when it succeeded with warnings about the input.
    const bool written = finished && process.exitStatus() == QProcess::NormalExit
                      && (process.exitCode() == 0 || process.exitCode() == 3) && QFileInfo::exists(partial);
    if (!written) {
        if (!finished)
            process.kill();
        QFile::remove(partial);
        const QString detail = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        *error = detail.isEmpty() ? tr("qpdf could not write the export.")
                                  : tr("qpdf could not write the export: %1").arg(detail.section('\n', 0, 0));
        return false;
    }
    if (std::rename(QFile::encodeName(partial).constData(), QFile::encodeName(output).constData()) != 0) {
        QFile::remove(partial);
        *error = tr("Could not write %1.").arg(output);
        return false;
    }
    return true;
}
