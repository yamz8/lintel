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
#include "portalfilepicker.h"
#include "sheetimageprovider.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName("lintel");
    app.setApplicationVersion("0.1.0");

    // Matches the window to lintel.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("lintel");
    app.setWindowIcon(QIcon::fromTheme("lintel"));

    QQuickStyle::setStyle("Material");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("main", "A fast viewer for construction drawing sets."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("drawings.pdf", QCoreApplication::translate("main", "The drawing set to open."));
    parser.addPositionalArgument("previous.pdf", QCoreApplication::translate("main", "An older revision to compare it with."));
    parser.process(app);

    Theme theme;
    PortalFilePicker picker;
    Backend backend(&picker);

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    // The engine owns the provider and deletes it with itself.
    engine.addImageProvider("sheets", new SheetImageProvider(&backend));
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);

    const QStringList files = parser.positionalArguments();
    if (!files.isEmpty() && backend.load(QUrl::fromUserInput(files.at(0), QDir::currentPath(), QUrl::AssumeLocalFile))
        && files.size() > 1)
        backend.loadPrevious(QUrl::fromUserInput(files.at(1), QDir::currentPath(), QUrl::AssumeLocalFile));

    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
