QT += core gui qml quick quickcontrols2 dbus pdf concurrent

CONFIG += c++17 release
TARGET = lintel
TEMPLATE = app

HEADERS += \
    src/compare.h \
    src/backend.h \
    src/cli.h \
    src/filepicker.h \
    src/portalfilepicker.h \
    src/sheetimageprovider.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/compare.cpp \
    src/backend.cpp \
    src/cli.cpp \
    src/portalfilepicker.cpp \
    src/sheetimageprovider.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
