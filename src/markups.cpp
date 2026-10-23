#include "markups.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineF>
#include <QLocale>
#include <QRegularExpression>
#include <QRectF>
#include <QSaveFile>
#include <QStandardPaths>

namespace {

constexpr double pointsPerInch = 72;
constexpr double inchesPerMeter = 39.3700787;

QJsonArray pointsJson(const QVector<QPointF> &points) {
    QJsonArray list;
    for (const QPointF &p : points)
        list.append(QJsonArray{qRound(p.x() * 100) / 100.0, qRound(p.y() * 100) / 100.0});
    return list;
}

// Distance from p to the segment ab.
qreal distanceToSegment(const QPointF &p, const QPointF &a, const QPointF &b) {
    const QPointF ab = b - a;
    const qreal length = QPointF::dotProduct(ab, ab);
    const qreal t = length > 0 ? qBound(0.0, QPointF::dotProduct(p - a, ab) / length, 1.0) : 0;
    return QLineF(p, a + t * ab).length();
}

}

Markups::Markups(const QString &storageDirectory, QObject *parent)
    : QObject(parent),
      m_directory(storageDirectory.isEmpty()
                      ? QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/lintel/markups"
                      : storageDirectory) {}

bool Markups::open(const QString &pdfPath, int sheetCount) {
    m_sheets.clear();
    m_scales.clear();
    m_undo.clear();
    m_redo.clear();
    m_nextId = 1;
    m_sheetCount = sheetCount;
    m_storagePath.clear();
    ++m_revision;

    QFile pdf(pdfPath);
    if (pdfPath.isEmpty() || !pdf.open(QIODevice::ReadOnly)) {
        emit changed();
        return pdfPath.isEmpty();
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&pdf);
    m_storagePath = m_directory + "/" + hash.result().toHex() + ".json";

    QFile stored(m_storagePath);
    if (stored.open(QIODevice::ReadOnly)) {
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(stored.readAll(), &error);
        if (error.error != QJsonParseError::NoError) {
            // Keep the unreadable file for whoever wants to look at it, and
            // start fresh beside it rather than write over it.
            QFile::rename(m_storagePath, m_storagePath + ".unreadable");
            emit failed(tr("The markups saved for %1 could not be read; they were set aside.")
                            .arg(QFileInfo(pdfPath).fileName()));
        } else {
            fromJson(document.object());
        }
    }
    emit changed();
    return true;
}

int Markups::total() const {
    int count = 0;
    for (const auto &sheet : m_sheets)
        count += sheet.size();
    return count;
}

QList<int> Markups::markedSheets() const {
    QList<int> sheets;
    for (auto it = m_sheets.begin(); it != m_sheets.end(); ++it)
        if (!it.value().isEmpty())
            sheets.append(it.key());
    std::sort(sheets.begin(), sheets.end());
    return sheets;
}

int Markups::add(int sheet, const QString &type, const QVariantList &points, const QString &text) {
    static const QStringList types{"cloud", "arrow", "text", "measure"};
    const int needed = type == "text" ? 1 : 2;
    if (m_storagePath.isEmpty() || sheet < 0 || sheet >= m_sheetCount || !types.contains(type) || points.size() != needed)
        return -1;
    if (type == "text" && text.trimmed().isEmpty())
        return -1;
    Markup markup{m_nextId, type, {}, type == "text" ? text.trimmed() : QString()};
    for (const QVariant &point : points)
        markup.points.append(point.toPointF());
    // A click without a drag is no cloud, arrow or measurement.
    if (needed == 2 && QLineF(markup.points[0], markup.points[1]).length() < 1)
        return -1;

    const QJsonObject before = toJson();
    ++m_nextId;
    m_sheets[sheet].append(markup);
    commit(before);
    return markup.id;
}

void Markups::remove(int sheet, int id) {
    auto &list = m_sheets[sheet];
    for (int i = 0; i < list.size(); ++i) {
        if (list[i].id == id) {
            const QJsonObject before = toJson();
            list.removeAt(i);
            commit(before);
            return;
        }
    }
}

void Markups::move(int sheet, int id, qreal dx, qreal dy) {
    if (dx == 0 && dy == 0)
        return;
    for (Markup &markup : m_sheets[sheet]) {
        if (markup.id == id) {
            const QJsonObject before = toJson();
            for (QPointF &p : markup.points)
                p += QPointF(dx, dy);
            commit(before);
            return;
        }
    }
}

int Markups::hitTest(int sheet, const QPointF &point, qreal tolerance) const {
    const auto list = m_sheets.value(sheet);
    for (int i = list.size() - 1; i >= 0; --i) {
        const Markup &m = list[i];
        bool hit = false;
        if (m.type == "cloud") {
            // A cloud is hit on or near its outline, so the area inside stays
            // free for panning and for marks drawn within it.
            const QRectF rect = QRectF(m.points[0], m.points[1]).normalized();
            hit = rect.adjusted(-tolerance, -tolerance, tolerance, tolerance).contains(point)
               && !rect.adjusted(tolerance * 2, tolerance * 2, -tolerance * 2, -tolerance * 2).contains(point);
        } else if (m.type == "text") {
            // Roughly the label's box: wide enough for its characters.
            const QRectF box(m.points[0].x() - tolerance, m.points[0].y() - tolerance * 3,
                             m.text.size() * tolerance * 1.4 + tolerance * 2, tolerance * 4);
            hit = box.contains(point);
        } else {
            hit = distanceToSegment(point, m.points[0], m.points[1]) <= tolerance;
        }
        if (hit)
            return m.id;
    }
    return -1;
}

void Markups::undo() {
    if (m_undo.isEmpty())
        return;
    m_redo.append(toJson());
    fromJson(m_undo.takeLast());
    ++m_revision;
    save();
    emit changed();
}

void Markups::redo() {
    if (m_redo.isEmpty())
        return;
    m_undo.append(toJson());
    fromJson(m_redo.takeLast());
    ++m_revision;
    save();
    emit changed();
}

void Markups::setScale(int sheet, double pointsPerUnit, const QString &unit, const QString &label, bool allSheets) {
    if (pointsPerUnit <= 0 || (unit != "ft" && unit != "m") || m_storagePath.isEmpty())
        return;
    const QJsonObject before = toJson();
    const SheetScale scale{pointsPerUnit, unit, label};
    if (allSheets)
        for (int i = 0; i < m_sheetCount; ++i)
            m_scales[i] = scale;
    else
        m_scales[sheet] = scale;
    commit(before);
}

void Markups::calibrate(int sheet, qreal points, double length, const QString &unit, bool allSheets) {
    if (points <= 0 || length <= 0)
        return;
    setScale(sheet, points / length, unit, tr("Calibrated"), allSheets);
}

double Markups::parseLength(const QString &text, const QString &unit) {
    QString value = text.trimmed();
    value.replace(',', '.');
    bool ok = false;
    if (unit == "m") {
        value.remove(QRegularExpression("\\s*m$"));
        const double meters = value.toDouble(&ok);
        return ok && meters > 0 ? meters : -1;
    }
    // Feet and inches, either part optional, inches possibly fractional.
    static const QRegularExpression imperial(
        "^(?:(\\d+(?:\\.\\d+)?)\\s*(?:'|ft)?)?\\s*-?\\s*(?:(\\d+(?:\\.\\d+)?)\\s*(?:\"|in))?$");
    if (value.contains('\'') || value.contains('"') || value.contains("ft") || value.contains("in")) {
        // A number with only an inch mark is all inches.
        if (!value.contains('\'') && !value.contains("ft")) {
            QString inches = value;
            inches.remove(QRegularExpression("\\s*(\"|in)$"));
            const double total = inches.toDouble(&ok) / 12;
            return ok && total > 0 ? total : -1;
        }
        const auto match = imperial.match(value);
        if (!match.hasMatch() || (match.captured(1).isEmpty() && match.captured(2).isEmpty()))
            return -1;
        const double feet = match.captured(1).toDouble();
        const double inches = match.captured(2).toDouble();
        const double total = feet + inches / 12;
        return total > 0 ? total : -1;
    }
    const double feet = value.toDouble(&ok);
    return ok && feet > 0 ? feet : -1;
}

QVariantList Markups::presets() {
    QVariantList list;
    // Architectural: a fraction of an inch on paper per foot on site.
    const QList<std::pair<QString, double>> imperial{
        {"1/16\" = 1'-0\"", 1.0 / 16}, {"3/32\" = 1'-0\"", 3.0 / 32}, {"1/8\" = 1'-0\"", 1.0 / 8},
        {"3/16\" = 1'-0\"", 3.0 / 16}, {"1/4\" = 1'-0\"", 1.0 / 4}, {"3/8\" = 1'-0\"", 3.0 / 8},
        {"1/2\" = 1'-0\"", 1.0 / 2}, {"3/4\" = 1'-0\"", 3.0 / 4}, {"1\" = 1'-0\"", 1},
        {"1\" = 10'", 0.1}, {"1\" = 20'", 0.05}, {"1\" = 50'", 0.02}};
    for (const auto &[label, inches] : imperial)
        list.append(QVariantMap{{"label", label}, {"pointsPerUnit", inches * pointsPerInch}, {"unit", "ft"}});
    // Metric: one meter on site is 1/n meters on paper.
    for (const int n : {10, 20, 50, 100, 200, 500, 1000})
        list.append(QVariantMap{{"label", QString("1:%1").arg(n)},
                                {"pointsPerUnit", inchesPerMeter * pointsPerInch / n},
                                {"unit", "m"}});
    return list;
}

QString Markups::formatLength(int sheet, qreal points) const {
    return formatLength(m_scales.value(sheet), points);
}

QString Markups::formatLength(const SheetScale &scale, qreal points) {
    if (!scale.isSet())
        return tr("No scale");
    const double length = points / scale.pointsPerUnit;
    if (scale.unit == "m")
        return QLocale().toString(length, 'f', length < 10 ? 2 : 1) + " m";
    // Feet and inches, to the nearest inch, as dimensions read on drawings.
    const int inches = qRound(length * 12);
    return QString("%1'-%2\"").arg(inches / 12).arg(inches % 12);
}

QJsonObject Markups::toJson() const {
    QJsonObject sheets;
    for (int sheet = 0; sheet < m_sheetCount; ++sheet) {
        const auto list = m_sheets.value(sheet);
        const SheetScale scale = m_scales.value(sheet);
        if (list.isEmpty() && !scale.isSet())
            continue;
        QJsonArray marks;
        for (const Markup &m : list) {
            QJsonObject mark{{"id", m.id}, {"type", m.type}, {"points", pointsJson(m.points)}};
            if (!m.text.isEmpty())
                mark["text"] = m.text;
            marks.append(mark);
        }
        QJsonObject entry{{"markups", marks}};
        if (scale.isSet())
            entry["scale"] = QJsonObject{{"points_per_unit", scale.pointsPerUnit}, {"unit", scale.unit}, {"label", scale.label}};
        // Numbered from 1, as sheets are everywhere else people see them.
        sheets[QString::number(sheet + 1)] = entry;
    }
    return {{"version", 1}, {"next_id", m_nextId}, {"sheets", sheets}};
}

void Markups::fromJson(const QJsonObject &object) {
    m_sheets.clear();
    m_scales.clear();
    m_nextId = qMax(1, object["next_id"].toInt(1));
    const QJsonObject sheets = object["sheets"].toObject();
    for (auto it = sheets.begin(); it != sheets.end(); ++it) {
        const int sheet = it.key().toInt() - 1;
        if (sheet < 0 || sheet >= m_sheetCount)
            continue;
        const QJsonObject entry = it.value().toObject();
        for (const auto &value : entry["markups"].toArray()) {
            const QJsonObject mark = value.toObject();
            Markup m{mark["id"].toInt(), mark["type"].toString(), {}, mark["text"].toString()};
            for (const auto &point : mark["points"].toArray())
                m.points.append(QPointF(point.toArray().at(0).toDouble(), point.toArray().at(1).toDouble()));
            if (m.points.size() != (m.type == "text" ? 1 : 2))
                continue;
            m_nextId = qMax(m_nextId, m.id + 1);
            m_sheets[sheet].append(m);
        }
        const QJsonObject scale = entry["scale"].toObject();
        if (scale["points_per_unit"].toDouble() > 0)
            m_scales[sheet] = {scale["points_per_unit"].toDouble(), scale["unit"].toString(), scale["label"].toString()};
    }
}

void Markups::commit(const QJsonObject &before) {
    m_undo.append(before);
    m_redo.clear();
    ++m_revision;
    save();
    emit changed();
}

// Written beside the target and renamed over it, so a crash mid-save never
// leaves half a file.
void Markups::save() {
    if (m_storagePath.isEmpty())
        return;
    QSaveFile file(m_storagePath);
    if (!QDir().mkpath(m_directory) || !file.open(QIODevice::WriteOnly)
        || file.write(QJsonDocument(toJson()).toJson()) < 0 || !file.commit())
        emit failed(tr("Could not save markups to %1.").arg(m_storagePath));
}
