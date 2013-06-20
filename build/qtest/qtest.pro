#-------------------------------------------------
#
# Project created by QtCreator 2013-05-08T09:45:52
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = qtest
TEMPLATE = app


SOURCES += main.cpp\
        ParserDialog.cpp \
    json.cpp

HEADERS  += ParserDialog.h \
    ../lib/psdparser.h \
    json.h

FORMS    += ParserDialog.ui

#LIBS += -L ./ -lpsdparser

CONFIG += debug_and_release

CONFIG(debug, debug|release)
{
    LIBS += -L ./debug -lpsdparser
}

CONFIG(release, debug|release)
{
    LIBS += -L ./release -lpsdparser
}
