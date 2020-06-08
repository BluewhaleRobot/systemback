QT -= gui
QT += core

TARGET = sbscheduler

CONFIG -= app_bundle
CONFIG += console \
          c++11 \
          exceptions_off

TEMPLATE = app

DEFINES += _FILE_OFFSET_BITS=64

SOURCES += main.cpp \
           sbscheduler.cpp

HEADERS += sbscheduler.hpp

QMAKE_CXXFLAGS += -g \
                  -fno-rtti \
                  -fvisibility=hidden \
                  -fvisibility-inlines-hidden \
                  -fno-asynchronous-unwind-tables

CONFIG(debug, debug|release) {
    QMAKE_CXXFLAGS_WARN_ON += -Wextra \
                              -Wshadow \
                              -Werror
}

QMAKE_LFLAGS += -g \
                -Wl,-rpath=/usr/lib/systemback \
                -Wl,--as-needed \
                -fuse-ld=gold \
                -Wl,-z,relro

! equals(QMAKE_CXX, clang++) {
    QMAKE_CXXFLAGS += -flto
    QMAKE_LFLAGS += -flto
}

LIBS += -L../libsystemback \
        -lsystemback

INCLUDEPATH = ../libsystemback
