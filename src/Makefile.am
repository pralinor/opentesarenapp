SUBDIRS = dataextraction utils

bin_PROGRAMS = opentesarena

opentesarena_CPPFLAGS = -I$(top_srcdir)/dataextraction \
                        -I$(top_srcdir)/utils
noinst_LTLIBRARIES = #libopentesarena.la

opentesarena_SOURCES = main.cpp

AM_CPPFLAGS =                             \
            -I$(top_srcdir)               \
            -I$(srcdir)

opentesarena_LDADD = dataextraction/libdataextraction.a \
                     utils/libutils.a

