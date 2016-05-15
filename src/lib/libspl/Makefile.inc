noinst_LTLIBRARIES += libspl.la

libspl_la_SOURCES = \
	lib/libspl/fnvpair.c \
	lib/libspl/nvpair.c \
	lib/libspl/nvpair_alloc_fixed.c \
	lib/libspl/spl/nvpair.h \
	lib/libspl/spl/nvpair_impl.h \
	lib/libspl/spl/sysmacros.h \
	lib/libspl/spl/types.h

# vim: sw=8 ts=8 sts=8 noet ft=make:
