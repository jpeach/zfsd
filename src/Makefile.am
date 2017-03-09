pkglib_LTLIBRARIES =
noinst_LTLIBRARIES =
bin_PROGRAMS =
check_PROGRAMS =
noinst_PROGRAMS =

EXTRA_DIST = include
TESTS = $(check_PROGRAMS)

CATCH = $(top_srcdir)/ext/catch
CATCH_CPPFLAGS = -I$(CATCH)

LUAJIT_CPPFLAGS = -I$(top_srcdir)/ext/luajit/src
LUAJIT_LIBS = $(top_builddir)/ext/luajit/src/libluajit.a

AM_CPPFLAGS = \
	-D_KERNEL \
	$(LTDLINCL) \
	$(CATCH_CPPFLAGS) \
	$(LIBCK_CPPFLAGS) \
	$(LUAJIT_CPPFLAGS) \
	-I$(srcdir)/include \
	-I$(srcdir)/fs/include \
	-I$(srcdir)/fs/zfs \
	-I$(srcdir)/fs/common \
	-I$(srcdir)/lib/libnvpair \
	-I$(srcdir)/lib/libspl \
	-I$(srcdir)/lib/libskein

AM_CFLAGS = \
	$(PTHREAD_CFLAGS)

AM_CXXFLAGS = \
	$(PTHREAD_CFLAGS)

AM_LDFLAGS = \
        $(ZLIB_LDFLAGS) \
        $(LIBCK_LDFLAGS) \
        $(CRYPTO_LDFLAGS)

include lib/libnvpair/Makefile.inc
include lib/libskein/Makefile.inc
include lib/libspl/Makefile.inc
include fs/Makefile.inc
include tests/Makefile.inc
