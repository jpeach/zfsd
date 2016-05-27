noinst_LTLIBRARIES += libskein.la

libskein_la_SOURCES = \
	lib/libskein/skein.c \
	lib/libskein/skein/skein.h \
	lib/libskein/skein_block.c \
	lib/libskein/skein_impl.h \
	lib/libskein/skein_iv.c \
	lib/libskein/skein_port.h

# vim: sw=8 ts=8 sts=8 noet ft=make:
