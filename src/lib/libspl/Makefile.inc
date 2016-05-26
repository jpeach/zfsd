noinst_LTLIBRARIES += libspl.la

libspl_la_SOURCES = \
	lib/libspl/avl.c \
	lib/libspl/bitmap.c \
	lib/libspl/bitmap_arch.c \
	lib/libspl/callb.c \
	lib/libspl/condvar.c \
	lib/libspl/cred.c \
	lib/libspl/cyclic.c \
	lib/libspl/debug.c \
	lib/libspl/fnvpair.c \
	lib/libspl/kmem.c \
	lib/libspl/list.c \
	lib/libspl/nvpair.c \
	lib/libspl/nvpair_alloc_fixed.c \
	lib/libspl/nvpair_alloc_system.c \
	lib/libspl/policy.c \
	lib/libspl/random.c \
	lib/libspl/rwlock.c \
	lib/libspl/spl/atomic.h \
	lib/libspl/spl/avl.h \
	lib/libspl/spl/avl_impl.h \
	lib/libspl/spl/bitmap.h \
	lib/libspl/spl/byteorder.h \
	lib/libspl/spl/callb.h \
	lib/libspl/spl/cmn_err.h \
	lib/libspl/spl/condvar.h \
	lib/libspl/spl/cred.h \
	lib/libspl/spl/cyclic.h \
	lib/libspl/spl/debug.h \
	lib/libspl/spl/dkio.h \
	lib/libspl/spl/door.h \
	lib/libspl/spl/file.h \
	lib/libspl/spl/idmap.h \
	lib/libspl/spl/kidmap.h \
	lib/libspl/spl/kmem.h \
	lib/libspl/spl/kstat.h \
	lib/libspl/spl/list.h \
	lib/libspl/spl/list_impl.h \
	lib/libspl/spl/mutex.h \
	lib/libspl/spl/nvpair.h \
	lib/libspl/spl/nvpair_impl.h \
	lib/libspl/spl/policy.h \
	lib/libspl/spl/random.h \
	lib/libspl/spl/rwlock.h \
	lib/libspl/spl/sdt.h \
	lib/libspl/spl/sid.h \
	lib/libspl/spl/sysevent.h \
	lib/libspl/spl/sysmacros.h \
	lib/libspl/spl/taskq.h \
	lib/libspl/spl/taskq_impl.h \
	lib/libspl/spl/thread.h
	lib/libspl/spl/time.h \
	lib/libspl/spl/types.h \
	lib/libspl/spl/u8_textprep.h \
	lib/libspl/spl/u8_textprep_data.h \
	lib/libspl/spl/uio.h \
	lib/libspl/spl/vfs.h \
	lib/libspl/spl/vnode.h \
	lib/libspl/spl/zone.h \
	lib/libspl/string.c \
	lib/libspl/taskq.c \
	lib/libspl/thread.c \
	lib/libspl/u8_textprep.c

# vim: sw=8 ts=8 sts=8 noet ft=make:
