noinst_LTLIBRARIES += libnvpair.la

libnvpair_la_SOURCES = \
	lib/libnvpair/libnvpair.c \
	lib/libnvpair/libnvpair.h \
	lib/libnvpair/nvpair_alloc_system.c \
	lib/libnvpair/nvpair_json.c

# vim: sw=8 ts=8 sts=8 noet ft=make:
