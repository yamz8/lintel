QT += core gui quick quickcontrols2 testlib dbus pdf
CONFIG += c++17 testcase
TARGET = lintel_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/backend.h ../src/compare.h ../src/filepicker.h ../src/theme.h ../src/translator.h
SOURCES += lintel_tests.cpp ../src/backend.cpp ../src/compare.cpp ../src/theme.cpp ../src/translator.cpp
