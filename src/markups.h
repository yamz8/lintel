#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointF>
#include <QVariantList>
#include <QVector>

// One mark on a sheet, in PDF points from the sheet's top left corner.
struct Markup {
    int id = 0;
    QString type;              // "cloud", "arrow", "text" or "measure"
    QVector<QPointF> points;   // two corners, two ends, or one anchor
    QString text;
};

// How long a PDF point is on site: one unit (a foot or a meter) spans
// pointsPerUnit points on the sheet.
struct SheetScale {
    double pointsPerUnit = 0;
    QString unit;              // "ft" or "m"
    QString label;             // 1/8" = 1'-0", 1:100, or "Calibrated"
    bool isSet() const { return pointsPerUnit > 0; }
};

// The markups and scales of one drawing set. They're kept apart from the PDF,
// which is never changed, keyed by a hash of its contents so they follow the
// file when it's renamed or moved. Every change saves at once.
class Markups : public QObject {
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY changed)
    Q_PROPERTY(int total READ total NOTIFY changed)

public:
    explicit Markups(const QString &storageDirectory = {}, QObject *parent = nullptr);

    // Loads what's stored for this PDF; an empty path closes.
    bool open(const QString &pdfPath, int sheetCount);
    QString storagePath() const { return m_storagePath; }

    int revision() const { return m_revision; }
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    int total() const;

    QVector<Markup> forSheet(int sheet) const { return m_sheets.value(sheet); }
    SheetScale scaleFor(int sheet) const { return m_scales.value(sheet); }

    Q_INVOKABLE int add(int sheet, const QString &type, const QVariantList &points, const QString &text = {});
    Q_INVOKABLE void remove(int sheet, int id);
    Q_INVOKABLE void move(int sheet, int id, qreal dx, qreal dy);
    // The topmost markup within tolerance points of the point, or -1.
    Q_INVOKABLE int hitTest(int sheet, const QPointF &point, qreal tolerance) const;
    Q_INVOKABLE int count(int sheet) const { return int(m_sheets.value(sheet).size()); }
    Q_INVOKABLE QList<int> markedSheets() const;
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

    // Sets a sheet's scale, or every sheet's.
    Q_INVOKABLE void setScale(int sheet, double pointsPerUnit, const QString &unit, const QString &label, bool allSheets);
    // Sets the scale from a line of this many points that is length units long.
    Q_INVOKABLE void calibrate(int sheet, qreal points, double length, const QString &unit, bool allSheets);
    Q_INVOKABLE QString scaleLabel(int sheet) const { return m_scales.value(sheet).label; }
    Q_INVOKABLE QString formatLength(int sheet, qreal points) const;

    // A length typed in the unit: 12'-6", 12' 6", 12.5 or 150" in feet;
    // 3.2 or 3,2 in meters. -1 when it isn't one.
    Q_INVOKABLE static double parseLength(const QString &text, const QString &unit);
    // The common drawing scales, each {label, pointsPerUnit, unit}.
    Q_INVOKABLE static QVariantList presets();
    static QString formatLength(const SheetScale &scale, qreal points);

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &object);
    // A detached copy, never saved, for reading off the GUI thread.
    void adopt(const QJsonObject &object, int sheetCount) {
        m_sheetCount = sheetCount;
        fromJson(object);
    }

signals:
    void changed();
    void failed(const QString &message);

private:
    void commit(const QJsonObject &before);
    void save();

    QString m_directory;
    QString m_storagePath;
    int m_sheetCount = 0;
    QHash<int, QVector<Markup>> m_sheets;
    QHash<int, SheetScale> m_scales;
    int m_nextId = 1;
    int m_revision = 0;
    QVector<QJsonObject> m_undo, m_redo;
};
