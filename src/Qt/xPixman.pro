#-------------------------------------------------
#
# Project created by QtCreator 2014-05-15T11:02:35
#
#-------------------------------------------------

QT       -= gui

TARGET = xPixman
TEMPLATE = lib
CONFIG += staticlib

DEFINES += PACKAGE
DEFINES += HAVE_PTHREADS

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
    ../pixman-mips-memcpy-asm.S \
    ../pixman-mips-dspr2.h \
    ../pixman-mips-dspr2-asm.S \
    ../pixman-mips-dspr2-asm.h \
    ../pixman-inlines.h \
    ../pixman-edge-imp.h \
    ../pixman-compiler.h \
    ../pixman-combine32.h \
    ../pixman-arm-simd-asm.S \
    ../pixman-arm-simd-asm.h \
    ../pixman-arm-simd-asm-scaled.S \
    ../pixman-arm-neon-asm.S \
    ../pixman-arm-neon-asm.h \
    ../pixman-arm-common.h \
    ../pixman-accessor.h \
    ../loongson-mmintrin.h

OTHER_FILES += \
    ../pixman-arm-neon-asm-bilinear.S

SOURCES += \
    ../pixman.c \
    ../pixman-x86.c \
    ../pixman-vmx.c \
    ../pixman-utils.c \
    ../pixman-trap.c \
    ../pixman-timer.c \
    ../pixman-ssse3.c \
    ../pixman-sse2.c \
    ../pixman-solid-fill.c \
    ../pixman-region32.c \
    ../pixman-region16.c \
    ../pixman-region.c \
    ../pixman-radial-gradient.c \
    ../pixman-ppc.c \
    ../pixman-noop.c \
    ../pixman-mmx.c \
    ../pixman-mips.c \
    ../pixman-mips-dspr2.c \
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
    ../pixman-arm.c \
    ../pixman-arm-simd.c \
    ../pixman-arm-neon.c \
    ../pixman-access.c \
    ../pixman-access-accessors.c
