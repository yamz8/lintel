#pragma once

#include <QFuture>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QSizeF>
#include <QUrl>
#include <QVariantList>
#include <QVector>

#include <atomic>
#include <optional>

#include "compare.h"

class FilePicker;
class QPdfDocument;

// The open drawing set, and optionally an older revision of it to compare
// against. QML reads state from here; sheet images come from render(), which
// the image provider calls from Qt Quick's loader threads.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString fileName READ fileName NOTIFY documentsChanged)
    Q_PROPERTY(QString previousFileName READ previousFileName NOTIFY documentsChanged)
    Q_PROPERTY(int sheetCount READ sheetCount NOTIFY documentsChanged)
    Q_PROPERTY(bool hasPrevious READ hasPrevious NOTIFY documentsChanged)
    Q_PROPERTY(int generation READ generation NOTIFY documentsChanged)
    Q_PROPERTY(int sheet READ sheet WRITE setSheet NOTIFY sheetChanged)
    Q_PROPERTY(int previousSheet READ previousSheet NOTIFY sheetChanged)
    Q_PROPERTY(QSizeF sheetSize READ sheetSize NOTIFY sheetChanged)
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(int analysisRevision READ analysisRevision NOTIFY analysisChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY analysisChanged)
    Q_PROPERTY(double scanProgress READ scanProgress NOTIFY analysisChanged)
    Q_PROPERTY(int changedCount READ changedCount NOTIFY analysisChanged)
    Q_PROPERTY(int newCount READ newCount NOTIFY analysisChanged)
    Q_PROPERTY(int removedCount READ removedCount NOTIFY analysisChanged)

public:
    // Renders never exceed this many pixels on their long edge, which keeps a
    // full sheet at high zoom to about 250 MB rather than gigabytes.
    static constexpr int maxRenderEdge = 8000;
    // Sheets are compared at this many pixels on their long edge: enough to
    // catch a moved door on a full-size sheet, quick enough for big sets.
    static constexpr int analysisEdge = 2000;

    explicit Backend(FilePicker *picker = nullptr, QObject *parent = nullptr);
    ~Backend() override;

    QString fileName() const { return m_fileName; }
    QString previousFileName() const { return m_previousFileName; }
    int sheetCount() const { return m_labels.size(); }
    bool hasPrevious() const { return !m_previousFileName.isEmpty(); }
    int generation() const { return m_generation; }
    int sheet() const { return m_sheet; }
    int previousSheet() const { return matchFor(m_sheet); }
    QSizeF sheetSize() const { return sizeOf(m_sheet); }
    QString mode() const { return m_mode; }
    QString error() const { return m_error; }
    int analysisRevision() const { return m_analysisRevision; }
    bool scanning() const { return hasPrevious() && m_analyzed < sheetCount(); }
    double scanProgress() const { return sheetCount() ? double(m_analyzed) / sheetCount() : 0; }
    int changedCount() const;
    int newCount() const;
    int removedCount() const;

    void setSheet(int sheet);
    void setMode(const QString &mode);

    Q_INVOKABLE QString sheetLabel(int sheet) const;
    Q_INVOKABLE QSizeF sizeOf(int sheet) const;
    // The previous revision's sheet that matches this one, or -1.
    Q_INVOKABLE int matchFor(int sheet) const;

    // "" with nothing to compare against, else "new", "pending", "changed"
    // or "unchanged".
    Q_INVOKABLE QString sheetStatus(int sheet) const;
    // The changed areas of a sheet, as fractions of it, in reading order.
    Q_INVOKABLE QVariantList changeRegions(int sheet) const;
    // The next new or changed sheet after from, stepping by +1 or -1 and
    // wrapping around, or -1 when there is none.
    Q_INVOKABLE int nextChangedSheet(int from, int step) const;
    // Whether the older revision of this sheet had to be moved to line up.
    Q_INVOKABLE bool isAligned(int sheet) const;

    Q_INVOKABLE void openDrawingSet();
    Q_INVOKABLE void openPrevious();
    Q_INVOKABLE bool load(const QUrl &url);
    Q_INVOKABLE bool loadPrevious(const QUrl &url);
    Q_INVOKABLE void closePrevious();
    Q_INVOKABLE void nextSheet() { setSheet(m_sheet + 1); }
    Q_INVOKABLE void previousSheetInSet() { setSheet(m_sheet - 1); }
    Q_INVOKABLE void clearError() { setError({}); }

    // Thread-safe. id is "<current|previous|overlay>/<generation>/<sheet>",
    // where sheet indexes the current set; requested bounds the image size.
    QImage render(const QString &id, const QSize &requested);

    // Thread-safe. Renders a sheet and its match at analysisEdge and compares
    // them; nothing when the sheet has no match.
    std::optional<Compare::Changes> analyze(int sheet);
    // Thread-safe. How the older revision of a sheet lines up with it,
    // worked out on first use.
    Compare::Alignment alignmentFor(int sheet);
    // Blocks until the background comparison of every sheet has finished.
    void waitForScan();

    // Pairs sheets by page label (A-101 to A-101) where the two sets label
    // their pages, and by position otherwise.
    static QVector<int> matchSheets(const QStringList &current, const QStringList &previous);
    // The pixel size a sheet of this many points renders at within requested.
    static QSize renderSize(const QSizeF &points, const QSize &requested);

signals:
    void documentsChanged();
    void sheetChanged();
    void modeChanged();
    void errorChanged();
    void analysisChanged();

private:
    enum class Target { Current, Previous };

    bool loadInto(Target target, const QUrl &url);
    void rematch();
    void startScan();
    void stopScan();
    void setError(const QString &error);
    QString describe(int pdfError) const;

    FilePicker *m_picker = nullptr;
    Target m_picking = Target::Current;

    // Guards the documents and the tables render() reads off the GUI thread.
    mutable QMutex m_lock;
    QPdfDocument *m_current = nullptr;
    QPdfDocument *m_previous = nullptr;
    QStringList m_labels, m_previousLabels;
    QVector<QSizeF> m_sizes;
    QVector<int> m_matches;
    QVector<std::optional<Compare::Alignment>> m_alignments;
    // Bumped whenever the matches change, so an alignment worked out for the
    // sets that were open before is never stored for the new ones.
    int m_alignmentEpoch = 0;

    QString m_fileName, m_previousFileName;
    int m_generation = 0;
    int m_sheet = 0;
    QString m_mode = "current";
    QString m_error;

    // Filled in on the GUI thread as the background scan reports each sheet.
    struct Analysis {
        bool done = false;
        Compare::Changes changes;
    };
    QVector<Analysis> m_analysis;
    int m_analyzed = 0;
    int m_analysisRevision = 0;
    QFuture<void> m_scan;
    // The generation the running scan belongs to; anything else stops it.
    std::atomic<int> m_scanGeneration{-1};
};
