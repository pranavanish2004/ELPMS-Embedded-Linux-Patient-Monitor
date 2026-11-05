# Qt project file (qmake). Qt Creator opens this file directly.
# Command line:   qmake dashboard.pro && make
QT += core gui widgets sql
CONFIG += c++17

TARGET = elpms_dashboard
TEMPLATE = app

SOURCES += main.cpp mainwindow.cpp
HEADERS += mainwindow.h
