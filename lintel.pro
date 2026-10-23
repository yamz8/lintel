QT += core gui qml quick quickcontrols2 dbus pdf concurrent

CONFIG += c++17 release
TARGET = lintel
TEMPLATE = app

HEADERS += \
    src/compare.h \
    src/backend.h \
    src/cli.h \
    src/exporter.h \
    src/exportjob.h \
    src/filepicker.h \
    src/markuplayer.h \
    src/markuppainter.h \
    src/markups.h \
    src/portalfilepicker.h \
    src/sheetimageprovider.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/compare.cpp \
    src/exporter.cpp \
    src/exportjob.cpp \
    src/markuplayer.cpp \
    src/markuppainter.cpp \
    src/markups.cpp \
    src/backend.cpp \
    src/cli.cpp \
    src/portalfilepicker.cpp \
    src/sheetimageprovider.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
