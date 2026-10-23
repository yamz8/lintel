#include "exportjob.h"

#include <QFileInfo>
#include <QtConcurrentRun>
#include <memory>

#include "backend.h"
#include "exporter.h"
#include "filepicker.h"
#include "markups.h"

ExportJob::ExportJob(Backend *backend, Markups *markups, FilePicker *picker, QObject *parent)
    : QObject(parent), m_backend(backend), m_markups(markups), m_picker(picker) {
    if (m_picker)
        connect(m_picker, &FilePicker::saveSelected, this, &ExportJob::exportTo);
}

ExportJob::~ExportJob() {
    m_future.waitForFinished();
}

void ExportJob::start() {
    if (m_busy || m_backend->filePath().isEmpty() || !m_picker)
        return;
    if (m_markups->total() == 0) {
        emit failed(tr("There are no markups to export yet."));
        return;
    }
    const QFileInfo source(m_backend->filePath());
    const QString suggested = source.absolutePath() + "/" + tr("%1 marked up.pdf").arg(source.completeBaseName());
    m_picker->savePdf(QUrl::fromLocalFile(suggested), tr("Export Marked-Up Sheets"),
                      m_markups->markedSheets().size() < m_backend->sheetCount());
}

void ExportJob::exportTo(const QUrl &url, bool markedOnly) {
    if (m_busy || m_backend->filePath().isEmpty())
        return;
    QString output = url.toLocalFile();
    if (!output.endsWith(".pdf", Qt::CaseInsensitive))
        output += ".pdf";

    // What's on screen now is what gets exported, whatever happens next.
    const QString pdf = m_backend->filePath();
    QVector<QSizeF> sizes;
    for (int sheet = 0; sheet < m_backend->sheetCount(); ++sheet)
        sizes.append(m_backend->sizeOf(sheet));
    auto snapshot = std::make_shared<Markups>();
    snapshot->adopt(m_markups->toJson(), m_backend->sheetCount());
    const QList<int> sheets = markedOnly ? m_markups->markedSheets() : QList<int>();

    m_busy = true;
    emit changed();
    m_future = QtConcurrent::run([this, pdf, sizes, snapshot, sheets, output] {
        QString error;
        const bool ok = Exporter::exportMarkedUp(pdf, sizes, *snapshot, sheets, output, &error);
        QMetaObject::invokeMethod(this, [this, ok, error, output] {
            m_busy = false;
            emit changed();
            ok ? emit finished(output) : emit failed(error);
        }, Qt::QueuedConnection);
    });
}
