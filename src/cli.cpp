#include "cli.h"

#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QSaveFile>
#include <QUrl>
#include <cstdio>

#include "backend.h"

namespace {

void print(FILE *stream, const QString &text) { fprintf(stream, "%s\n", qPrintable(text)); }
void print(const QJsonObject &object) { fputs(QJsonDocument(object).toJson().constData(), stdout); }

int fail(const QString &message) {
    print(stderr, message);
    return 1;
}

struct Command {
    QCommandLineParser parser;
    Command(const QString &name, const QString &description, const QStringList &files) {
        parser.setApplicationDescription(description);
        parser.addHelpOption();
        parser.addPositionalArgument(name, description, name);
        for (const QString &file : files)
            parser.addPositionalArgument(file, "PDF drawing set", "<" + file + ">");
    }
    void json() { parser.addOption({"json", "Print the result as JSON"}); }
    bool wantsJson() const { return parser.isSet("json"); }
    QString argument(int index) const { return parser.positionalArguments().value(index); }
};

QUrl fileUrl(const QString &path) {
    return QUrl::fromUserInput(path, QDir::currentPath(), QUrl::AssumeLocalFile);
}

bool load(Backend &backend, const QString &path) {
    if (path.isEmpty())
        return fail("Name a PDF drawing set."), false;
    if (!backend.load(fileUrl(path)))
        return fail(backend.error()), false;
    return true;
}

bool loadPrevious(Backend &backend, const QString &path) {
    if (path.isEmpty())
        return fail("Name the older revision to compare with."), false;
    if (!backend.loadPrevious(fileUrl(path)))
        return fail(backend.error()), false;
    return true;
}

// A sheet named by its label (A-101) or its 1-based number, or -1.
int findSheet(const Backend &backend, const QString &name) {
    for (int sheet = 0; sheet < backend.sheetCount(); ++sheet)
        if (backend.sheetLabel(sheet).compare(name, Qt::CaseInsensitive) == 0)
            return sheet;
    bool number = false;
    const int index = name.toInt(&number) - 1;
    return number && index >= 0 && index < backend.sheetCount() ? index : -1;
}

double round2(double value) { return qRound(value * 100) / 100.0; }
double round4(double value) { return qRound(value * 10000) / 10000.0; }

QJsonObject sheetJson(const Backend &backend, int sheet) {
    const QSizeF size = backend.sizeOf(sheet);
    return {{"number", sheet + 1},
            {"label", backend.sheetLabel(sheet)},
            {"width_in", round2(size.width() / 72)},
            {"height_in", round2(size.height() / 72)}};
}

int sheets(const QStringList &arguments) {
    Command command("sheets", "List the sheets in a drawing set.", {"set"});
    command.json();
    command.parser.process(arguments);
    Backend backend;
    if (!load(backend, command.argument(1)))
        return 1;
    QJsonArray list;
    for (int sheet = 0; sheet < backend.sheetCount(); ++sheet)
        list.append(sheetJson(backend, sheet));
    if (command.wantsJson()) {
        print({{"file", backend.fileName()}, {"sheets", list}});
        return 0;
    }
    for (const auto &value : list) {
        const QJsonObject sheet = value.toObject();
        print(stdout, QString("%1\t%2\t%3 x %4 in")
                          .arg(sheet["number"].toInt())
                          .arg(sheet["label"].toString())
                          .arg(sheet["width_in"].toDouble())
                          .arg(sheet["height_in"].toDouble()));
    }
    return 0;
}

int compare(const QStringList &arguments) {
    Command command("compare", "Compare a drawing set with an older revision, sheet by sheet.", {"set", "previous"});
    command.json();
    command.parser.process(arguments);
    Backend backend;
    if (!load(backend, command.argument(1)) || !loadPrevious(backend, command.argument(2)))
        return 1;
    backend.waitForScan();

    QJsonArray list;
    int unchanged = 0;
    for (int sheet = 0; sheet < backend.sheetCount(); ++sheet) {
        QJsonObject entry = sheetJson(backend, sheet);
        entry.remove("width_in");
        entry.remove("height_in");
        const QString status = backend.sheetStatus(sheet);
        unchanged += status == "unchanged";
        entry["status"] = status;
        const int match = backend.matchFor(sheet);
        entry["previous_number"] = match < 0 ? QJsonValue() : QJsonValue(match + 1);
        entry["aligned"] = backend.isAligned(sheet);
        QJsonArray regions;
        for (const QVariant &region : backend.changeRegions(sheet)) {
            const QRectF r = region.toRectF();
            // Fractions of the sheet, from its top left corner.
            regions.append(QJsonObject{{"x", round4(r.x())}, {"y", round4(r.y())},
                                       {"width", round4(r.width())}, {"height", round4(r.height())}});
        }
        entry["changes"] = regions;
        list.append(entry);
    }

    if (command.wantsJson()) {
        print({{"set", backend.fileName()},
               {"previous", backend.previousFileName()},
               {"summary", QJsonObject{{"changed", backend.changedCount()}, {"new", backend.newCount()},
                                       {"unchanged", unchanged}, {"removed", backend.removedCount()}}},
               {"sheets", list},
               {"removed", QJsonArray::fromStringList(backend.removedSheets())}});
        return 0;
    }
    for (const auto &value : list) {
        const QJsonObject sheet = value.toObject();
        const int changes = sheet["changes"].toArray().size();
        print(stdout, QString("%1\t%2%3%4")
                          .arg(sheet["label"].toString(), sheet["status"].toString())
                          .arg(changes ? QString("\t%1 change%2").arg(changes).arg(changes == 1 ? "" : "s") : QString())
                          .arg(sheet["aligned"].toBool() ? "\taligned" : ""));
    }
    for (const QString &label : backend.removedSheets())
        print(stdout, label + "\tremoved");
    print(stdout, QString("%1 changed, %2 new, %3 unchanged, %4 removed")
                      .arg(backend.changedCount()).arg(backend.newCount()).arg(unchanged).arg(backend.removedCount()));
    return 0;
}

int render(const QStringList &arguments) {
    Command command("render", "Render one sheet to PNG: as it is, or overlaid on an older revision.", {"set"});
    command.json();
    command.parser.addOption({{"s", "sheet"}, "Sheet label (A-101) or number; the first by default", "sheet"});
    command.parser.addOption({"against", "Overlay on this older revision: removed red, added green", "previous"});
    command.parser.addOption({"previous", "With --against, render the older revision's sheet instead"});
    command.parser.addOption({"width", "Image width in pixels (default 2400)", "pixels", "2400"});
    command.parser.addOption({{"o", "output"}, "PNG to write", "file"});
    command.parser.process(arguments);

    const QString output = command.parser.value("output");
    if (output.isEmpty())
        return fail("Name the PNG to write with -o.");
    bool ok = false;
    const int width = command.parser.value("width").toInt(&ok);
    if (!ok || width < 16 || width > Backend::maxRenderEdge)
        return fail(QString("The width must be between 16 and %1 pixels.").arg(Backend::maxRenderEdge));

    Backend backend;
    if (!load(backend, command.argument(1)))
        return 1;
    const bool comparing = command.parser.isSet("against");
    if (comparing && !loadPrevious(backend, command.parser.value("against")))
        return 1;
    if (command.parser.isSet("previous") && !comparing)
        return fail("--previous needs --against.");

    const QString name = command.parser.value("sheet");
    const int sheet = name.isEmpty() ? 0 : findSheet(backend, name);
    if (sheet < 0)
        return fail(QString("%1 has no sheet %2.").arg(backend.fileName(), name));
    const QString kind = !comparing ? "current" : command.parser.isSet("previous") ? "previous" : "overlay";
    if (kind == "previous" && backend.matchFor(sheet) < 0)
        return fail(QString("Sheet %1 isn't in %2.").arg(backend.sheetLabel(sheet), backend.previousFileName()));

    // Renders come back transparent where the paper is blank; a PNG of a
    // drawing should be on white paper.
    const QImage rendered = backend.render(QString("%1/%2/%3").arg(kind).arg(backend.generation()).arg(sheet),
                                           QSize(width, 0));
    QImage image(rendered.size(), QImage::Format_RGB32);
    image.fill(Qt::white);
    {
        QPainter painter(&image);
        painter.drawImage(0, 0, rendered);
    }
    QSaveFile file(output);
    if (rendered.isNull() || !file.open(QIODevice::WriteOnly) || !image.save(&file, "PNG") || !file.commit())
        return fail("Could not write " + output);

    if (command.wantsJson())
        print({{"output", QFileInfo(output).absoluteFilePath()}, {"sheet", backend.sheetLabel(sheet)},
               {"view", kind}, {"width", image.width()}, {"height", image.height()}});
    else
        print(stdout, "Wrote " + output);
    return 0;
}

// As Hype does: one shared copy, linked into Claude Code, found directly by Codex.
int skill(const QStringList &arguments) {
    Command command("skill", "Print the agent skill, or install it for your coding agents with: skill install.", {});
    command.parser.addPositionalArgument("install", "Copy to ~/.agents/skills/lintel and link into ~/.claude/skills", "[install]");
    command.parser.process(arguments);
    QFile bundled(":/skill.md");
    if (!bundled.open(QIODevice::ReadOnly))
        return 1;
    const QByteArray text = bundled.readAll();
    if (command.argument(1).isEmpty()) {
        fputs(text.constData(), stdout);
        return 0;
    }
    if (command.argument(1) != "install")
        return fail("Use lintel skill to print the skill, or lintel skill install.");
    const QDir home = QDir::home();
    const QString directory = home.filePath(".agents/skills/lintel");
    QSaveFile file(directory + "/SKILL.md");
    if (!QDir().mkpath(directory) || !file.open(QIODevice::WriteOnly) || file.write(text) < 0 || !file.commit())
        return fail("Could not write " + file.fileName());
    print(stdout, "Installed " + file.fileName());
    if (!home.exists(".claude"))
        return 0;
    const QString link = home.filePath(".claude/skills/lintel");
    const QFileInfo existing(link);
    if (existing.isSymLink())
        QFile::remove(link);
    else if (existing.exists())
        return fail(link + " exists and is not a link; leaving it alone.");
    if (!home.mkpath(".claude/skills") || !QFile::link("../../.agents/skills/lintel", link))
        return fail("Could not link " + link);
    print(stdout, "Linked " + link);
    return 0;
}

int help(const QStringList &arguments) {
    const QString topic = arguments.value(2);
    if (isCliCommand(topic) && topic != "help")
        return runCli({arguments[0], topic, "--help"});
    print(stdout, "Usage: lintel <command> [options]\n\n" + cliSummary());
    return 0;
}

}

QString cliSummary() {
    return "Viewer:\n"
           "  open [set] [previous]             Open the viewer, comparing when given two sets\n\n"
           "Commands that need no display (lintel help <command> for options):\n"
           "  sheets <set>                      List the sheets, with labels and sizes\n"
           "  compare <set> <previous>          Report which sheets changed, and where\n"
           "  render <set> -o <png>             Render a sheet, or its overlay with --against\n"
           "  skill [install]                   Print the skill for coding agents, or install it";
}

bool isCliCommand(const QString &word) {
    return QStringList{"sheets", "compare", "render", "skill", "help"}.contains(word);
}

int runCli(const QStringList &arguments) {
    const QString command = arguments.value(1);
    return command == "sheets"    ? sheets(arguments)
           : command == "compare" ? compare(arguments)
           : command == "render"  ? render(arguments)
           : command == "skill"   ? skill(arguments)
                                  : help(arguments);
}
