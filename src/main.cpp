// Lintel — a fast viewer for construction drawing sets, with revision compare.

#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

#include "backend.h"
#include "cli.h"
#include "exportjob.h"
#include "markuplayer.h"
#include "markups.h"
#include "portalfilepicker.h"
#include "sheetimageprovider.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[]) {
    // Commands and help draw no window, so they must not need a display, even
    // where the desktop exports QT_QPA_PLATFORM=wayland. Bare lintel prints
    // help, as a command line tool should; the launcher says lintel open.
    const QByteArray first = argc > 1 ? QByteArray(argv[1]) : QByteArray();
    const bool command = argc == 1 || isCliCommand(QString::fromLocal8Bit(first));
    bool windowless = command;
    for (int i = 1; i < argc; ++i) {
        const QByteArray argument(argv[i]);
        windowless = windowless || argument == "--help" || argument == "-h" || argument == "--version" || argument == "-v";
    }
    if (windowless)
        qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);
    app.setApplicationName("lintel");
    app.setApplicationVersion("0.1.0");

    // Matches the window to lintel.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("lintel");
    app.setWindowIcon(QIcon::fromTheme("lintel"));

    if (command)
        return runCli(app.arguments());

    QQuickStyle::setStyle("Material");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("main", "A fast viewer for construction drawing sets.")
                                     + "\n\n" + cliSummary());
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("drawings.pdf", QCoreApplication::translate("main", "The drawing set to open."));
    parser.addPositionalArgument("previous.pdf", QCoreApplication::translate("main", "An older revision to compare it with."));
    // "lintel open a.pdf" and "lintel a.pdf" both open the viewer.
    QStringList arguments = app.arguments();
    if (arguments.value(1) == "open")
        arguments.removeAt(1);
    parser.process(arguments);

    Theme theme;
    PortalFilePicker picker;
    Backend backend(&picker);

    // Markups follow whichever set is open.
    Markups markups;
    QString markupsFor;
    QObject::connect(&backend, &Backend::documentsChanged, &markups, [&] {
        if (backend.filePath() != markupsFor) {
            markupsFor = backend.filePath();
            markups.open(markupsFor, backend.sheetCount());
        }
    });
    qmlRegisterType<MarkupLayer>("Lintel", 1, 0, "MarkupLayer");
    qmlRegisterUncreatableType<Markups>("Lintel", 1, 0, "Markups", "Markups come from the app");

    ExportJob exportJob(&backend, &markups, &picker);
    QObject::connect(&exportJob, &ExportJob::failed, &backend, &Backend::reportError);

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    // The engine owns the provider and deletes it with itself.
    engine.addImageProvider("sheets", new SheetImageProvider(&backend));
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.rootContext()->setContextProperty("markupSet", &markups);
    engine.rootContext()->setContextProperty("exportJob", &exportJob);
    QObject::connect(&markups, &Markups::failed, &backend, &Backend::reportError);

    const QStringList files = parser.positionalArguments();
    if (!files.isEmpty() && backend.load(QUrl::fromUserInput(files.at(0), QDir::currentPath(), QUrl::AssumeLocalFile))
        && files.size() > 1)
        backend.loadPrevious(QUrl::fromUserInput(files.at(1), QDir::currentPath(), QUrl::AssumeLocalFile));

    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
