SUBDIRS = dataextraction utils

bin_PROGRAMS = opentesarena

noinst_LTLIBRARIES = #libopentesarena.la

opentesarena_SOURCES = main.cpp

AM_CPPFLAGS = 														\
						-I$(top_srcdir)								\
						-I$(srcdir)

opentesarena_LDADD = dataextraction/libdataextraction.la	\
										 utils/libutils.la

