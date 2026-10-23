#include "backend.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QHash>
#include <QMutexLocker>
#include <QPdfDocument>
#include <QRegularExpression>
#include <QtConcurrentRun>
#include <utility>

#include "compare.h"
#include "filepicker.h"

Backend::Backend(FilePicker *picker, QObject *parent)
    : QObject(parent), m_picker(picker), m_current(new QPdfDocument(this)), m_previous(new QPdfDocument(this)) {
    if (m_picker) {
        connect(m_picker, &FilePicker::openSelected, this, [this](const QUrl &url) {
            m_picking == Target::Current ? load(url) : loadPrevious(url);
        });
        connect(m_picker, &FilePicker::failed, this, &Backend::setError);
    }
}

Backend::~Backend() {
    stopScan();
    // Loader threads may still be rendering; wait for the last one.
    QMutexLocker locker(&m_lock);
}

void Backend::setSheet(int sheet) {
    sheet = qBound(0, sheet, qMax(0, sheetCount() - 1));
    if (sheet == m_sheet)
        return;
    m_sheet = sheet;
    emit sheetChanged();
}

void Backend::setMode(const QString &mode) {
    static const QStringList modes{"current", "previous", "overlay"};
    // Without an older revision there is only the current set to look at.
    const QString next = modes.contains(mode) && (mode == "current" || hasPrevious()) ? mode : "current";
    if (next == m_mode)
        return;
    m_mode = next;
    emit modeChanged();
}

QString Backend::sheetLabel(int sheet) const {
    if (sheet < 0 || sheet >= m_labels.size())
        return {};
    return m_labels.at(sheet).isEmpty() ? QString::number(sheet + 1) : m_labels.at(sheet);
}

QSizeF Backend::sizeOf(int sheet) const {
    return sheet >= 0 && sheet < m_sizes.size() ? m_sizes.at(sheet) : QSizeF();
}

int Backend::matchFor(int sheet) const {
    return sheet >= 0 && sheet < m_matches.size() ? m_matches.at(sheet) : -1;
}

void Backend::openDrawingSet() {
    if (!m_picker)
        return;
    m_picking = Target::Current;
    m_picker->openPdf(tr("Open Drawing Set"));
}

void Backend::openPrevious() {
    if (!m_picker)
        return;
    if (m_fileName.isEmpty()) {
        setError(tr("Open a drawing set before comparing it with an older revision."));
        return;
    }
    m_picking = Target::Previous;
    m_picker->openPdf(tr("Open Previous Revision"));
}

QString Backend::describe(int pdfError) const {
    switch (QPdfDocument::Error(pdfError)) {
    case QPdfDocument::Error::FileNotFound: return tr("The file could not be found.");
    case QPdfDocument::Error::InvalidFileFormat: return tr("The file is not a valid PDF.");
    case QPdfDocument::Error::IncorrectPassword: return tr("The PDF is password protected.");
    case QPdfDocument::Error::UnsupportedSecurityScheme: return tr("The PDF uses an unsupported security scheme.");
    case QPdfDocument::Error::DataNotYetAvailable: return tr("The PDF is not fully downloaded yet.");
    default: return tr("The PDF could not be read.");
    }
}

// Loads into a fresh document and swaps it in only once it opened, so a
// file that fails to load leaves whatever was open on screen.
bool Backend::loadInto(Target target, const QUrl &url) {
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    const QString name = QFileInfo(path).fileName();
    auto *document = new QPdfDocument(this);
    const auto status = document->load(path);
    if (status != QPdfDocument::Error::None) {
        delete document;
        setError(tr("Could not open %1: %2").arg(name, describe(int(status))));
        return false;
    }
    if (document->pageCount() == 0) {
        delete document;
        setError(tr("%1 has no sheets.").arg(name));
        return false;
    }

    QStringList labels;
    QVector<QSizeF> sizes;
    for (int page = 0; page < document->pageCount(); ++page) {
        labels.append(document->pageLabel(page));
        sizes.append(document->pagePointSize(page));
    }

    QPdfDocument *replaced;
    {
        QMutexLocker locker(&m_lock);
        if (target == Target::Current) {
            replaced = std::exchange(m_current, document);
            m_fileName = name;
            m_labels = labels;
            m_sizes = sizes;
        } else {
            replaced = std::exchange(m_previous, document);
            m_previousFileName = name;
            m_previousLabels = labels;
        }
    }
    delete replaced;
    setError({});
    return true;
}

bool Backend::load(const QUrl &url) {
    stopScan();
    if (!loadInto(Target::Current, url)) {
        startScan();
        return false;
    }
    rematch();
    ++m_generation;
    startScan();
    m_sheet = 0;
    emit documentsChanged();
    emit sheetChanged();
    return true;
}

bool Backend::loadPrevious(const QUrl &url) {
    if (m_fileName.isEmpty()) {
        setError(tr("Open a drawing set before comparing it with an older revision."));
        return false;
    }
    stopScan();
    if (!loadInto(Target::Previous, url)) {
        startScan();
        return false;
    }
    rematch();
    ++m_generation;
    startScan();
    emit documentsChanged();
    emit sheetChanged();
    setMode("overlay");
    return true;
}
void Backend::closePrevious() {
    stopScan();
    {
        QMutexLocker locker(&m_lock);
        m_previous->close();
        m_previousFileName.clear();
        m_previousLabels.clear();
    }
    rematch();
    ++m_generation;
    startScan();
    setMode("current");
    emit documentsChanged();
    emit sheetChanged();
}

void Backend::rematch() {
    QMutexLocker locker(&m_lock);
    m_matches = m_previousFileName.isEmpty() ? QVector<int>(m_labels.size(), -1)
                                             : matchSheets(m_labels, m_previousLabels);
    m_alignments = QVector<std::optional<Compare::Alignment>>(m_labels.size());
    ++m_alignmentEpoch;
}

QVector<int> Backend::matchSheets(const QStringList &current, const QStringList &previous) {
    // Labels that are just the page number say nothing about the sheet, so
    // only real labels (A-101, S2.01) are matched by name.
    static const QRegularExpression pageNumber("^\\d+$");
    QHash<QString, int> byLabel;
    for (int i = 0; i < previous.size(); ++i)
        if (!previous.at(i).isEmpty() && !pageNumber.match(previous.at(i)).hasMatch() && !byLabel.contains(previous.at(i)))
            byLabel.insert(previous.at(i), i);

    QVector<int> matches(current.size(), -1);
    for (int i = 0; i < current.size(); ++i) {
        const QString &label = current.at(i);
        if (byLabel.contains(label))
            matches[i] = byLabel.value(label);
        else if (byLabel.isEmpty() && i < previous.size())
            matches[i] = i;
    }
    return matches;
}

QSize Backend::renderSize(const QSizeF &points, const QSize &requested) {
    if (points.isEmpty())
        return {};
    QSizeF size = points;
    if (requested.width() > 0 && requested.height() > 0)
        size = points.scaled(QSizeF(requested), Qt::KeepAspectRatio);
    else if (requested.width() > 0)
        size = QSizeF(requested.width(), points.height() * requested.width() / points.width());
    else if (requested.height() > 0)
        size = QSizeF(points.width() * requested.height() / points.height(), requested.height());
    else
        size = points.scaled(QSizeF(1600, 1600), Qt::KeepAspectRatio);
    const double longEdge = qMax(size.width(), size.height());
    if (longEdge > maxRenderEdge)
        size *= maxRenderEdge / longEdge;
    return QSize(qMax(1, qRound(size.width())), qMax(1, qRound(size.height())));
}

QImage Backend::render(const QString &id, const QSize &requested) {
    const QStringList parts = id.split('/');
    if (parts.size() != 3)
        return {};
    const QString kind = parts.at(0);
    const int sheet = parts.at(2).toInt();

    const Compare::Alignment alignment = kind == "current" ? Compare::Alignment() : alignmentFor(sheet);
    QImage older, newer;
    QSize size;
    {
        QMutexLocker locker(&m_lock);
        if (sheet < 0 || sheet >= m_sizes.size())
            return {};
        // Both revisions render at the current sheet's size, so they line up
        // pixel for pixel whatever their page sizes in points.
        size = renderSize(m_sizes.at(sheet), requested);
        const int match = sheet < m_matches.size() ? m_matches.at(sheet) : -1;
        if (kind == "current" || kind == "overlay")
            newer = m_current->render(sheet, size);
        if ((kind == "previous" || kind == "overlay") && match >= 0)
            older = m_previous->render(match, size);
    }
    if (kind == "current")
        return newer;
    if (kind == "previous")
        return older.isNull() ? older : Compare::aligned(older, size, alignment);
    if (kind == "overlay")
        return Compare::overlay(Compare::aligned(older, size, alignment), newer);
    return {};
}

void Backend::startScan() {
    m_analysis = QVector<Analysis>(sheetCount());
    m_analyzed = 0;
    ++m_analysisRevision;
    if (!hasPrevious()) {
        emit analysisChanged();
        return;
    }
    // New sheets have nothing to compare with, so they're done already.
    for (int sheet = 0; sheet < sheetCount(); ++sheet) {
        if (matchFor(sheet) < 0) {
            m_analysis[sheet].done = true;
            ++m_analyzed;
        }
    }
    emit analysisChanged();

    const int generation = m_generation;
    const QVector<int> matches = m_matches;
    m_scanGeneration = generation;
    m_scan = QtConcurrent::run([this, generation, matches] {
        for (int sheet = 0; sheet < matches.size() && m_scanGeneration == generation; ++sheet) {
            if (matches.at(sheet) < 0)
                continue;
            const auto changes = analyze(sheet);
            if (!changes || m_scanGeneration != generation)
                return;
            QMetaObject::invokeMethod(this, [this, generation, sheet, changes = *changes] {
                if (generation != m_generation || sheet >= m_analysis.size())
                    return;
                m_analysis[sheet] = {true, changes};
                ++m_analyzed;
                ++m_analysisRevision;
                emit analysisChanged();
            }, Qt::QueuedConnection);
        }
    });
}

void Backend::stopScan() {
    m_scanGeneration = -1;
    m_scan.waitForFinished();
}

void Backend::waitForScan() {
    m_scan.waitForFinished();
    // Deliver the results the scan queued for this thread.
    QCoreApplication::sendPostedEvents(this);
}

std::optional<Compare::Changes> Backend::analyze(int sheet) {
    QImage older, newer;
    QSize size;
    std::optional<Compare::Alignment> alignment;
    int epoch;
    {
        QMutexLocker locker(&m_lock);
        epoch = m_alignmentEpoch;
        const int match = matchFor(sheet);
        if (match < 0 || sheet >= m_sizes.size() || match >= m_previous->pageCount())
            return std::nullopt;
        size = renderSize(m_sizes.at(sheet), QSize(analysisEdge, analysisEdge));
        newer = m_current->render(sheet, size);
        older = m_previous->render(match, size);
        alignment = m_alignments.value(sheet);
    }
    if (!alignment) {
        alignment = Compare::align(older, newer);
        QMutexLocker locker(&m_lock);
        if (epoch == m_alignmentEpoch && sheet < m_alignments.size())
            m_alignments[sheet] = alignment;
    }
    return Compare::changes(Compare::aligned(older, size, *alignment), newer);
}

Compare::Alignment Backend::alignmentFor(int sheet) {
    QImage older, newer;
    int epoch;
    {
        QMutexLocker locker(&m_lock);
        const int match = matchFor(sheet);
        if (match < 0 || sheet >= m_sizes.size() || match >= m_previous->pageCount())
            return {};
        if (const auto known = m_alignments.value(sheet))
            return *known;
        epoch = m_alignmentEpoch;
        const QSize size = renderSize(m_sizes.at(sheet), QSize(analysisEdge, analysisEdge));
        newer = m_current->render(sheet, size);
        older = m_previous->render(match, size);
    }
    const Compare::Alignment alignment = Compare::align(older, newer);
    QMutexLocker locker(&m_lock);
    if (epoch == m_alignmentEpoch && sheet < m_alignments.size())
        m_alignments[sheet] = alignment;
    return alignment;
}

bool Backend::isAligned(int sheet) const {
    QMutexLocker locker(&m_lock);
    const auto alignment = m_alignments.value(sheet);
    return alignment && !alignment->isIdentity();
}

QString Backend::sheetStatus(int sheet) const {
    if (!hasPrevious() || sheet < 0 || sheet >= m_analysis.size())
        return {};
    if (matchFor(sheet) < 0)
        return "new";
    const Analysis &analysis = m_analysis.at(sheet);
    return !analysis.done ? "pending" : analysis.changes.any() ? "changed" : "unchanged";
}

QVariantList Backend::changeRegions(int sheet) const {
    QVariantList regions;
    if (sheet >= 0 && sheet < m_analysis.size())
        for (const QRectF &region : m_analysis.at(sheet).changes.regions)
            regions.append(region);
    return regions;
}

int Backend::nextChangedSheet(int from, int step) const {
    const int count = sheetCount();
    step = step < 0 ? -1 : 1;
    for (int i = 1; i <= count; ++i) {
        const int sheet = ((from + step * i) % count + count) % count;
        const QString status = sheetStatus(sheet);
        if (status == "changed" || status == "new")
            return sheet;
    }
    return -1;
}

int Backend::changedCount() const {
    int count = 0;
    for (const Analysis &analysis : m_analysis)
        count += analysis.done && analysis.changes.any();
    return count;
}

int Backend::newCount() const {
    return hasPrevious() ? int(std::count(m_matches.begin(), m_matches.end(), -1)) : 0;
}

int Backend::removedCount() const {
    if (!hasPrevious())
        return 0;
    int count = 0;
    for (int sheet = 0; sheet < m_previousLabels.size(); ++sheet)
        count += !m_matches.contains(sheet);
    return count;
}

void Backend::setError(const QString &error) {
    if (error == m_error)
        return;
    m_error = error;
    emit errorChanged();
}
