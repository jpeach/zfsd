noinst_LTLIBRARIES += libspl.la

libspl_la_SOURCES = \
	lib/libspl/avl.c \
	lib/libspl/bitmap.c \
	lib/libspl/bitmap_arch.c \
	lib/libspl/condvar.c \
	lib/libspl/debug.c \
	lib/libspl/fnvpair.c \
	lib/libspl/nvpair.c \
	lib/libspl/nvpair_alloc_fixed.c \
	lib/libspl/nvpair_alloc_system.c \
	lib/libspl/random.c \
	lib/libspl/rwlock.c \
	lib/libspl/spl/atomic.h \
	lib/libspl/spl/avl.h \
	lib/libspl/spl/avl_impl.h \
	lib/libspl/spl/bitmap.h \
	lib/libspl/spl/cmn_err.h \
	lib/libspl/spl/condvar.h \
	lib/libspl/spl/debug.h \
	lib/libspl/spl/mutex.h \
	lib/libspl/spl/nvpair.h \
	lib/libspl/spl/nvpair_impl.h \
	lib/libspl/spl/random.h \
	lib/libspl/spl/rwlock.h \
	lib/libspl/spl/sysmacros.h \
	lib/libspl/spl/time.h \
	lib/libspl/spl/types.h \
	lib/libspl/taskq.c

# vim: sw=8 ts=8 sts=8 noet ft=make:
