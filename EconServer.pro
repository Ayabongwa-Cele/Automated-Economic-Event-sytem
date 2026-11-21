QT += core network
CONFIG += console c++17
CONFIG -= app_bundle

TEMPLATE = app
TARGET = EconServer

SOURCES += \
    main.cpp\
    econserver.cpp

HEADERS += \
    econserver.h
