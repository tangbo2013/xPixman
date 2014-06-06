#-------------------------------------------------
#
# Project created by QtCreator 2014-05-15T11:02:35
#
#-------------------------------------------------

QT       -= gui

TARGET = xPixman
TEMPLATE = lib
CONFIG += staticlib
INCLUDEPATH += ../../../

DEFINES += HAVE_CONFIG_H

QMAKE_CFLAGS_WARN_ON += -Wno-unused-parameter -Wno-missing-field-initializers -Wno-unused-function -Wno-sign-compare
QMAKE_CXXFLAGS_WARN_ON += -Wno-unused-parameter -Wno-missing-field-initializers -Wno-unused-function -Wno-sign-compare

unix:!symbian {
    maemo5 {
        target.path = /opt/usr/lib
    } else {
        target.path = /usr/lib
    }
    INSTALLS += target
}

HEADERS += \
    ../../pixman.h \
    ../../pixman-version.h \
    ../pixman-version.h.in \
    ../pixman-private.h \
    ../pixman-inlines.h \
    ../pixman-edge-imp.h \
    ../pixman-compiler.h \
    ../pixman-combine32.h \
    ../pixman-accessor.h \
    ../config.h

OTHER_FILES += \

SOURCES += \
    ../pixman.c \
    ../pixman-utils.c \
    ../pixman-trap.c \
    ../pixman-solid-fill.c \
    ../pixman-region32.c \
    ../pixman-region16.c \
    ../pixman-region.c \
    ../pixman-radial-gradient.c \
    ../pixman-noop.c \
    ../pixman-matrix.c \
    ../pixman-linear-gradient.c \
    ../pixman-implementation.c \
    ../pixman-image.c \
    ../pixman-gradient-walker.c \
    ../pixman-glyph.c \
    ../pixman-general.c \
    ../pixman-filter.c \
    ../pixman-fast-path.c \
    ../pixman-edge.c \
    ../pixman-edge-accessors.c \
    ../pixman-conical-gradient.c \
    ../pixman-combine32.c \
    ../pixman-combine-float.c \
    ../pixman-bits-image.c \
    ../pixman-access.c \
    ../pixman-access-accessors.c
