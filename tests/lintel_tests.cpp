#include <QDir>
#include <QFile>
#include <QPainter>
#include <QPdfWriter>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
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

        backend.closePrevious();
        QVERIFY(!backend.hasPrevious());
        QCOMPARE(backend.mode(), QString("current"));
        QCOMPARE(backend.matchFor(0), -1);
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

    // A landscape drawing set with a numbered box on each sheet.
    static QString writeSet(const QString &path, int sheets)
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
        }
        painter.end();
        return path;
    }
};

QTEST_MAIN(LintelTests)
#include "lintel_tests.moc"
