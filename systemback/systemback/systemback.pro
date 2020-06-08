QT += core \
      gui \
      widgets

TARGET = systemback

CONFIG += c++11 \
          exceptions_off

TEMPLATE = app

DEFINES += _FILE_OFFSET_BITS=64

SOURCES += main.cpp \
           systemback.cpp

HEADERS += systemback.hpp \
           bttnevent.hpp \
           chckbxevent.hpp \
           lblevent.hpp \
           lndtevent.hpp \
           pnlevent.hpp \
           tblwdgtevent.hpp

FORMS += systemback.ui

RESOURCES += pictures.qrc

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
        -lsystemback \
        -lcrypt \
        -lX11
