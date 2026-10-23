#include <QDir>
#include <QFile>
#include <QPainter>
#include <QPdfWriter>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "cli.h"
#include "markuppainter.h"
#include "markups.h"
#include "compare.h"
#include "filepicker.h"
#include "theme.h"
#include "translator.h"

// Hands back a chosen file instead of showing the portal's dialog.
class FakePicker : public FilePicker {
    Q_OBJECT

public:
    QUrl next;
    QString lastTitle;
    void openPdf(const QString &title) override {
        lastTitle = title;
        emit openSelected(next);
    }
};

class LintelTests : public QObject {
    Q_OBJECT

private slots:
    void overlayColorsRemovedAddedAndUnchanged()
    {
        QImage older(40, 10, QImage::Format_RGB32), newer(40, 10, QImage::Format_RGB32);
        older.fill(Qt::white);
        newer.fill(Qt::white);
        // Unchanged: ink on both. Removed: only the older. Added: only the newer.
        older.setPixel(5, 5, qRgb(0, 0, 0));
        newer.setPixel(5, 5, qRgb(0, 0, 0));
        older.setPixel(20, 5, qRgb(0, 0, 0));
        newer.setPixel(35, 5, qRgb(0, 0, 0));

        const QImage result = Compare::overlay(older, newer);
        QCOMPARE(result.size(), newer.size());
        QCOMPARE(QColor(result.pixel(20, 5)), Compare::removedColor);
        QCOMPARE(QColor(result.pixel(35, 5)), Compare::addedColor);
        const QColor unchanged = result.pixel(5, 5);
        QVERIFY(unchanged.red() == unchanged.green() && unchanged.green() == unchanged.blue());
        QVERIFY(unchanged.red() > 100 && unchanged.red() < 200);
        QCOMPARE(QColor(result.pixel(0, 0)), QColor(Qt::white));
    }

    void overlayToleratesOnePixelShifts()
    {
        QImage older(20, 20, QImage::Format_RGB32), newer(20, 20, QImage::Format_RGB32);
        older.fill(Qt::white);
        newer.fill(Qt::white);
        for (int y = 2; y < 18; ++y) {
            older.setPixel(10, y, qRgb(0, 0, 0));
            newer.setPixel(11, y, qRgb(0, 0, 0));
        }
        const QImage tolerant = Compare::overlay(older, newer);
        QVERIFY(QColor(tolerant.pixel(11, 8)) != Compare::addedColor);
        QVERIFY(QColor(tolerant.pixel(10, 8)) != Compare::removedColor);

        const QImage strict = Compare::overlay(older, newer, 0);
        QCOMPARE(QColor(strict.pixel(11, 8)), Compare::addedColor);
        QCOMPARE(QColor(strict.pixel(10, 8)), Compare::removedColor);
    }

    void overlayTreatsTransparencyAsPaperAndScales()
    {
        // PDF renders come back transparent where there's no ink.
        QImage older(10, 10, QImage::Format_ARGB32), newer(20, 20, QImage::Format_ARGB32);
        older.fill(Qt::transparent);
        newer.fill(Qt::transparent);
        newer.setPixel(15, 15, qRgba(0, 0, 0, 255));

        const QImage result = Compare::overlay(older, newer);
        QCOMPARE(result.size(), QSize(20, 20));
        QCOMPARE(QColor(result.pixel(2, 2)), QColor(Qt::white));
        QCOMPARE(QColor(result.pixel(15, 15)), Compare::addedColor);
    }

    void overlayWithoutOlderSheetIsAllAdded()
    {
        QImage newer(10, 10, QImage::Format_RGB32);
        newer.fill(Qt::white);
        newer.setPixel(3, 3, qRgb(0, 0, 0));
        const QImage result = Compare::overlay(QImage(), newer);
        QCOMPARE(QColor(result.pixel(3, 3)), Compare::addedColor);
        QVERIFY(Compare::overlay(newer, QImage()).isNull());
    }

    void changesAreGroupedIntoRegions()
    {
        QImage older(400, 300, QImage::Format_RGB32);
        older.fill(Qt::white);
        QImage newer = older;
        QPainter painter(&newer);
        painter.fillRect(300, 40, 30, 20, Qt::black);   // top right
        painter.fillRect(40, 200, 20, 30, Qt::black);   // lower left
        painter.fillRect(48, 238, 10, 6, Qt::black);    // close by, same change
        painter.end();

        const auto changes = Compare::changes(older, newer);
        QCOMPARE(changes.regions.size(), 2);
        QVERIFY(changes.added > 0);
        QCOMPARE(changes.removed, 0);
        // Reading order: the top one first.
        QVERIFY(changes.regions[0].contains(QPointF(315 / 400.0, 50 / 300.0)));
        QVERIFY(changes.regions[1].contains(QPointF(50 / 400.0, 215 / 300.0)));
        QVERIFY(changes.regions[1].contains(QPointF(53 / 400.0, 241 / 300.0)));
    }

    void identicalOrSpeckledSheetsHaveNoChanges()
    {
        QImage sheet(400, 300, QImage::Format_RGB32);
        sheet.fill(Qt::white);
        QPainter painter(&sheet);
        painter.drawRect(20, 20, 200, 100);
        painter.end();
        QVERIFY(!Compare::changes(sheet, sheet).any());

        QImage speckled = sheet;
        speckled.setPixel(300, 200, qRgb(0, 0, 0));
        QVERIFY(!Compare::changes(sheet, speckled).any());
    }

    void alignmentRecoversAShiftedReplot()
    {
        auto drawing = [](QPointF offset, double scale) {
            QImage image(600, 400, QImage::Format_RGB32);
            image.fill(Qt::white);
            QPainter painter(&image);
            painter.translate(offset);
            painter.scale(scale, scale);
            painter.setPen(QPen(Qt::black, 3));
            painter.drawRect(20, 20, 560, 360);
            painter.drawLine(100, 60, 100, 340);
            painter.drawLine(100, 200, 480, 200);
            painter.drawEllipse(QPointF(300, 120), 40, 40);
            return image;
        };
        const QImage newer = drawing({0, 0}, 1);
        const QImage older = drawing({9, 6}, 0.98);
        QVERIFY(Compare::changes(older, newer).any());

        const auto alignment = Compare::align(older, newer);
        QVERIFY(!alignment.isIdentity());
        const auto left = Compare::changes(Compare::aligned(older, newer.size(), alignment), newer);
        QVERIFY(!left.any());
    }

    void alignmentLeavesRealChangesAlone()
    {
        QImage older(600, 400, QImage::Format_RGB32);
        older.fill(Qt::white);
        QPainter painter(&older);
        painter.setPen(QPen(Qt::black, 3));
        painter.drawRect(20, 20, 560, 360);
        painter.end();
        QImage newer = older;
        painter.begin(&newer);
        painter.setPen(QPen(Qt::black, 3));
        painter.drawLine(300, 60, 300, 340);
        painter.end();

        QVERIFY(Compare::align(older, newer).isIdentity());
        QVERIFY(Compare::align(older, older).isIdentity());
    }

    void sheetsMatchByLabel()
    {
        const auto matches = Backend::matchSheets({"A-001", "A-101", "A-102", "A-201"},
                                                  {"A-001", "A-101", "A-201"});
        QCOMPARE(matches, QVector<int>({0, 1, -1, 2}));
    }

    void sheetsMatchByPositionWithoutLabels()
    {
        QCOMPARE(Backend::matchSheets({"", "", ""}, {"", ""}), QVector<int>({0, 1, -1}));
        // Plain page numbers say nothing about which sheet a page is.
        QCOMPARE(Backend::matchSheets({"1", "2"}, {"1", "2", "3"}), QVector<int>({0, 1}));
    }

    void renderSizeKeepsAspectAndCaps()
    {
        const QSizeF archD(2592, 1728);
        QCOMPARE(Backend::renderSize(archD, QSize(1296, 1296)), QSize(1296, 864));
        QCOMPARE(Backend::renderSize(archD, QSize(200, 0)), QSize(200, 133));
        const QSize huge = Backend::renderSize(archD, QSize(40000, 40000));
        QCOMPARE(qMax(huge.width(), huge.height()), Backend::maxRenderEdge);
        QVERIFY(Backend::renderSize(QSizeF(), QSize(100, 100)).isEmpty());
    }

    void loadsASetAndRendersSheets()
    {
        QTemporaryDir dir;
        const QString path = writeSet(dir.filePath("set.pdf"), 3);
        Backend backend;
        QSignalSpy changed(&backend, &Backend::documentsChanged);

        QVERIFY(backend.load(QUrl::fromLocalFile(path)));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(backend.fileName(), QString("set.pdf"));
        QCOMPARE(backend.sheetCount(), 3);
        QCOMPARE(backend.sheetLabel(1), QString("2"));
        QVERIFY(backend.sheetSize().width() > backend.sheetSize().height());

        backend.nextSheet();
        backend.nextSheet();
        backend.nextSheet();
        QCOMPARE(backend.sheet(), 2);

        const QImage image = backend.render(QString("current/%1/1").arg(backend.generation()), QSize(600, 600));
        QCOMPARE(image.width(), 600);
        QVERIFY(backend.render("current/0/9", QSize(100, 100)).isNull());

        // A tile matches the same area of a whole-sheet render.
        const QImage whole = backend.render("current/0/0", QSize(2400, 2400)).convertToFormat(QImage::Format_ARGB32);
        const QImage tile = backend.render(QString("current/0/0/%1x%2/1000,200,512,512").arg(whole.width()).arg(whole.height()), {})
                                .convertToFormat(QImage::Format_ARGB32);
        QCOMPARE(tile.size(), QSize(512, 512));
        int differing = 0;
        for (int y = 0; y < 512; ++y)
            for (int x = 0; x < 512; ++x)
                differing += qAbs(qAlpha(tile.pixel(x, y)) - qAlpha(whole.pixel(1000 + x, 200 + y))) > 60;
        QVERIFY2(differing < 512, qPrintable(QString::number(differing)));
        QVERIFY(backend.render("current/0/0/100x100/0,0,9000,10", {}).isNull());
        QVERIFY(backend.render("nonsense", QSize(100, 100)).isNull());
    }

    void aFailedLoadKeepsTheOpenSet()
    {
        QTemporaryDir dir;
        Backend backend;
        QVERIFY(backend.load(QUrl::fromLocalFile(writeSet(dir.filePath("set.pdf"), 2))));

        QFile junk(dir.filePath("junk.pdf"));
        QVERIFY(junk.open(QIODevice::WriteOnly));
        junk.write("not a pdf");
        junk.close();

        QVERIFY(!backend.load(QUrl::fromLocalFile(junk.fileName())));
        QVERIFY(backend.error().contains("junk.pdf"));
        QCOMPARE(backend.fileName(), QString("set.pdf"));
        QCOMPARE(backend.sheetCount(), 2);

        QVERIFY(!backend.load(QUrl::fromLocalFile(dir.filePath("missing.pdf"))));
        QCOMPARE(backend.sheetCount(), 2);
    }

    void comparingNeedsASetAndShowsTheOverlay()
    {
        QTemporaryDir dir;
        const QString current = writeSet(dir.filePath("rev1.pdf"), 3);
        const QString previous = writeSet(dir.filePath("rev0.pdf"), 2);
        Backend backend;

        QVERIFY(!backend.loadPrevious(QUrl::fromLocalFile(previous)));
        QVERIFY(!backend.error().isEmpty());
        backend.setMode("overlay");
        QCOMPARE(backend.mode(), QString("current"));

        QVERIFY(backend.load(QUrl::fromLocalFile(current)));
        QVERIFY(backend.error().isEmpty());
        QVERIFY(backend.loadPrevious(QUrl::fromLocalFile(previous)));
        QVERIFY(backend.hasPrevious());
        QCOMPARE(backend.mode(), QString("overlay"));
        QCOMPARE(backend.previousSheet(), 0);
        QCOMPARE(backend.matchFor(2), -1);

        const QImage overlay = backend.render(QString("overlay/%1/0").arg(backend.generation()), QSize(400, 400));
        QCOMPARE(overlay.width(), 400);
        // Sheet 3 isn't in the older set, so it has no previous render.
        QVERIFY(backend.render(QString("previous/%1/2").arg(backend.generation()), QSize(400, 400)).isNull());

        backend.waitForScan();
        QVERIFY(!backend.scanning());
        // The sets differ in the box drawn on each sheet, which grows with it.
        QCOMPARE(backend.sheetStatus(0), QString("unchanged"));
        QCOMPARE(backend.sheetStatus(1), QString("unchanged"));
        QCOMPARE(backend.sheetStatus(2), QString("new"));
        QCOMPARE(backend.newCount(), 1);
        QCOMPARE(backend.nextChangedSheet(0, 1), 2);
        QCOMPARE(backend.nextChangedSheet(2, 1), 2);

        backend.closePrevious();
        QVERIFY(!backend.hasPrevious());
        QCOMPARE(backend.sheetStatus(0), QString());
        QCOMPARE(backend.mode(), QString("current"));
        QCOMPARE(backend.matchFor(0), -1);
    }

    void scanFindsChangedSheets()
    {
        QTemporaryDir dir;
        Backend backend;
        QVERIFY(backend.load(QUrl::fromLocalFile(writeSet(dir.filePath("rev1.pdf"), 2, 1))));
        QVERIFY(backend.loadPrevious(QUrl::fromLocalFile(writeSet(dir.filePath("rev0.pdf"), 2))));
        backend.waitForScan();

        QCOMPARE(backend.sheetStatus(0), QString("unchanged"));
        QCOMPARE(backend.sheetStatus(1), QString("changed"));
        QCOMPARE(backend.changedCount(), 1);
        QCOMPARE(backend.changeRegions(1).size(), 1);
        const QRectF region = backend.changeRegions(1).first().toRectF();
        // The mark is 900 by 600 units of 1/1200 inch on a 36 by 24 inch sheet,
        // inside the writer's margins: small, toward the upper left.
        QVERIFY2(region.width() < 0.1 && region.height() < 0.15, qPrintable(QString::number(region.width())));
        QVERIFY(region.center().x() > 0.1 && region.center().x() < 0.2);
        QVERIFY(region.center().y() > 0.05 && region.center().y() < 0.2);
        QCOMPARE(backend.nextChangedSheet(0, 1), 1);
        QCOMPARE(backend.nextChangedSheet(1, -1), 1);
    }

    void pickerResultsGoToTheRightSet()
    {
        QTemporaryDir dir;
        FakePicker picker;
        Backend backend(&picker);

        picker.next = QUrl::fromLocalFile(writeSet(dir.filePath("rev0.pdf"), 1));
        backend.openPrevious();
        QVERIFY(!backend.hasPrevious());
        QVERIFY(!backend.error().isEmpty());

        picker.next = QUrl::fromLocalFile(writeSet(dir.filePath("rev1.pdf"), 2));
        backend.openDrawingSet();
        QCOMPARE(backend.fileName(), QString("rev1.pdf"));

        picker.next = QUrl::fromLocalFile(dir.filePath("rev0.pdf"));
        backend.openPrevious();
        QCOMPARE(backend.previousFileName(), QString("rev0.pdf"));
        QCOMPARE(backend.fileName(), QString("rev1.pdf"));
    }

    void renderCommandWritesAPng()
    {
        QTemporaryDir dir;
        const QString current = writeSet(dir.filePath("rev1.pdf"), 2, 1);
        const QString previous = writeSet(dir.filePath("rev0.pdf"), 2);
        const QString png = dir.filePath("overlay.png");

        QCOMPARE(runCli({"lintel", "render", current, "--sheet", "2", "--against", previous,
                         "--width", "600", "-o", png}), 0);
        const QImage image(png);
        QCOMPARE(image.width(), 600);
        // The mark only rev1 has shows in green.
        bool green = false;
        for (int y = 0; y < image.height() && !green; ++y)
            for (int x = 0; x < image.width() && !green; ++x)
                green = QColor(image.pixel(x, y)) == Compare::addedColor;
        QVERIFY(green);

        QCOMPARE(runCli({"lintel", "render", current, "--sheet", "9", "-o", png}), 1);
        QCOMPARE(runCli({"lintel", "render", dir.filePath("missing.pdf"), "-o", png}), 1);
        QCOMPARE(runCli({"lintel", "compare", current, previous}), 0);
        QVERIFY(isCliCommand("compare"));
        QVERIFY(!isCliCommand("open"));
    }

    void markupsSaveAndComeBack()
    {
        QTemporaryDir dir;
        const QString pdf = writeSet(dir.filePath("set.pdf"), 3);
        {
            Markups markups(dir.filePath("store"));
            QVERIFY(markups.open(pdf, 3));
            QVERIFY(markups.add(1, "cloud", {QPointF(10, 10), QPointF(200, 120)}) > 0);
            QVERIFY(markups.add(1, "text", {QPointF(50, 300)}, "  Verify in field ") > 0);
            QVERIFY(markups.add(2, "arrow", {QPointF(0, 0), QPointF(90, 90)}) > 0);
            // Nothing to draw: a click, an unknown type, empty text, no such sheet.
            QCOMPARE(markups.add(1, "arrow", {QPointF(5, 5), QPointF(5.2, 5)}), -1);
            QCOMPARE(markups.add(1, "scribble", {QPointF(0, 0), QPointF(9, 9)}), -1);
            QCOMPARE(markups.add(1, "text", {QPointF(0, 0)}, "  "), -1);
            QCOMPARE(markups.add(7, "cloud", {QPointF(0, 0), QPointF(9, 9)}), -1);
            QCOMPARE(markups.total(), 3);
            QCOMPARE(markups.markedSheets(), QList<int>({1, 2}));
        }

        // The same file under another name keeps its markups.
        QVERIFY(QFile::copy(pdf, dir.filePath("renamed.pdf")));
        Markups markups(dir.filePath("store"));
        QVERIFY(markups.open(dir.filePath("renamed.pdf"), 3));
        QCOMPARE(markups.total(), 3);
        QCOMPARE(markups.forSheet(1).at(1).text, QString("Verify in field"));
        // New ids never reuse old ones.
        const int id = markups.add(0, "cloud", {QPointF(0, 0), QPointF(50, 50)});
        QVERIFY(id > markups.forSheet(2).first().id);

        // A different file has none.
        Markups other(dir.filePath("store"));
        QVERIFY(other.open(writeSet(dir.filePath("other.pdf"), 2, 0), 2));
        QCOMPARE(other.total(), 0);
    }

    void markupsUndoAndRedo()
    {
        QTemporaryDir dir;
        Markups markups(dir.filePath("store"));
        QVERIFY(markups.open(writeSet(dir.filePath("set.pdf"), 1), 1));
        QVERIFY(!markups.canUndo());
        const int id = markups.add(0, "arrow", {QPointF(0, 0), QPointF(100, 0)});
        markups.move(0, id, 10, 5);
        QCOMPARE(markups.forSheet(0).first().points.first(), QPointF(10, 5));
        markups.undo();
        QCOMPARE(markups.forSheet(0).first().points.first(), QPointF(0, 0));
        markups.undo();
        QCOMPARE(markups.total(), 0);
        QVERIFY(markups.canRedo());
        markups.redo();
        markups.redo();
        QCOMPARE(markups.forSheet(0).first().points.first(), QPointF(10, 5));
        markups.remove(0, id);
        QCOMPARE(markups.total(), 0);
        QVERIFY(!markups.canRedo());
    }

    void markupsAreHitNearTheirLines()
    {
        QTemporaryDir dir;
        Markups markups(dir.filePath("store"));
        QVERIFY(markups.open(writeSet(dir.filePath("set.pdf"), 1), 1));
        const int cloud = markups.add(0, "cloud", {QPointF(100, 100), QPointF(300, 200)});
        const int arrow = markups.add(0, "arrow", {QPointF(0, 500), QPointF(400, 500)});
        QCOMPARE(markups.hitTest(0, QPointF(101, 150), 5), cloud);
        QCOMPARE(markups.hitTest(0, QPointF(200, 150), 5), -1);  // inside a cloud pans
        QCOMPARE(markups.hitTest(0, QPointF(200, 503), 5), arrow);
        QCOMPARE(markups.hitTest(0, QPointF(200, 520), 5), -1);
    }

    void lengthsParseAndFormat()
    {
        QCOMPARE(Markups::parseLength("12'-6\"", "ft"), 12.5);
        QCOMPARE(Markups::parseLength("12' 6\"", "ft"), 12.5);
        QCOMPARE(Markups::parseLength("12'", "ft"), 12.0);
        QCOMPARE(Markups::parseLength("18\"", "ft"), 1.5);
        QCOMPARE(Markups::parseLength("12.5", "ft"), 12.5);
        QCOMPARE(Markups::parseLength("3,2", "m"), 3.2);
        QCOMPARE(Markups::parseLength("3.2 m", "m"), 3.2);
        QCOMPARE(Markups::parseLength("wall", "ft"), -1.0);
        QCOMPARE(Markups::parseLength("0", "m"), -1.0);

        const SheetScale eighth{9, "ft", "1/8\" = 1'-0\""};  // 1/8 inch per foot
        QCOMPARE(Markups::formatLength(eighth, 9 * 12.5), QString("12'-6\""));
        QCOMPARE(Markups::formatLength(SheetScale(), 100), QString("No scale"));

        QTemporaryDir dir;
        Markups markups(dir.filePath("store"));
        QVERIFY(markups.open(writeSet(dir.filePath("set.pdf"), 2), 2));
        // 1:100 puts a meter on site at 10 mm on paper.
        QVariantMap metric;
        for (const QVariant &preset : Markups::presets())
            if (preset.toMap()["label"] == "1:100")
                metric = preset.toMap();
        QCOMPARE(metric["label"].toString(), QString("1:100"));
        QVERIFY(qAbs(metric["pointsPerUnit"].toDouble() - 28.3465) < 0.001);
        markups.calibrate(0, 180, 20, "ft", false);
        QCOMPARE(markups.formatLength(0, 90), QString("10'-0\""));
        QCOMPARE(markups.formatLength(1, 90), QString("No scale"));
        markups.setScale(0, metric["pointsPerUnit"].toDouble(), "m", "1:100", true);
        QCOMPARE(markups.scaleLabel(1), QString("1:100"));
        QVERIFY(markups.formatLength(1, 283.465).startsWith("10"));
    }

    void markupsPaintInBlue()
    {
        QImage image(400, 300, QImage::Format_RGB32);
        image.fill(Qt::white);
        QPainter painter(&image);
        MarkupPainter::paint(&painter, {{1, "cloud", {QPointF(50, 50), QPointF(250, 200)}, {}}}, {}, QSizeF(400, 300));
        painter.end();
        int blue = 0;
        for (int y = 0; y < 300; ++y)
            for (int x = 0; x < 400; ++x)
                blue += qBlue(image.pixel(x, y)) > qRed(image.pixel(x, y)) + 60;
        QVERIFY(blue > 100);
        QCOMPARE(QColor(image.pixel(150, 125)), QColor(Qt::white));
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\n");

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n");
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        write(response, "{\"int\":0}");

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        write(response, "{\"int\":6}");
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})");
        write(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("pt-BR"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void translatorPicksPluralForms()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "ru\n");
        write(dir.path() + "/en.json", R"({"%n clips": {"one": "%n clip", "other": "%n clips"}})");
        write(dir.path() + "/ru.json", R"({"%n clips": {"one": "%n клип", "few": "%n клипа", "many": "%n клипов", "other": "%n клипа"}})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 21), QString("%n клип"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 22), QString("%n клипа"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 25), QString("%n клипов"));
        QCOMPARE(Translator::pluralCategory("ar", 11), QString("many"));
        QCOMPARE(Translator::pluralCategory("ja", 1), QString("other"));
    }

    void shippedCatalogsCoverTheSameStrings()
    {
        // Every shipped language translates the same strings, so adding a
        // string to one catalog and forgetting another fails here.
        QFile danish(QFINDTESTDATA("../src/i18n/da.json"));
        QVERIFY(danish.open(QIODevice::ReadOnly));
        const QStringList keys = QJsonDocument::fromJson(danish.readAll()).object().keys();
        QVERIFY(!keys.isEmpty());
        for (const QString code : {"de"}) {
            QFile catalog(QFINDTESTDATA(QString("../src/i18n/%1.json").arg(code)));
            QVERIFY(catalog.open(QIODevice::ReadOnly));
            QCOMPARE(QJsonDocument::fromJson(catalog.readAll()).object().keys(), keys);
        }
    }

private:
    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }

    // A landscape drawing set with a numbered box on each sheet, and an
    // extra mark on the sheet numbered marked.
    static QString writeSet(const QString &path, int sheets, int marked = -1)
    {
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::ArchD));
        writer.setPageOrientation(QPageLayout::Landscape);
        QPainter painter(&writer);
        for (int sheet = 0; sheet < sheets; ++sheet) {
            if (sheet > 0)
                writer.newPage();
            painter.drawRect(400, 400, 4000 + sheet * 500, 3000);
            painter.drawText(600, 800, QString("Sheet %1").arg(sheet + 1));
            if (sheet == marked)
                painter.fillRect(5000, 2000, 900, 600, Qt::black);
        }
        painter.end();
        return path;
    }
};

QTEST_MAIN(LintelTests)
#include "lintel_tests.moc"
