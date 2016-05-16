pkglib_LTLIBRARIES =
noinst_LTLIBRARIES =
bin_PROGRAMS =
check_PROGRAMS =
noinst_PROGRAMS =

EXTRA_DIST = include
TESTS = $(check_PROGRAMS)

CATCH = $(top_srcdir)/ext/catch
CATCH_CPPFLAGS = -I$(CATCH)

LIBCK_CPPFLAGS = -I$(top_srcdir)/ext/libck/include
LIBCK_LIBS = $(top_builddir)/ext/libck/src/libck.a

LUAJIT_CPPFLAGS = -I$(top_srcdir)/ext/luajit/src
LUAJIT_LIBS = $(top_builddir)/ext/luajit/src/libluajit.a

AM_CPPFLAGS = \
	-D_KERNEL \
	$(LTDLINCL) \
	$(CATCH_CPPFLAGS) \
	$(LIBCK_CPPFLAGS) \
	$(LUAJIT_CPPFLAGS) \
	-I$(srcdir)/include \
	-I$(srcdir)/lib/libnvpair \
	-I$(srcdir)/lib/libspl

include lib/libnvpair/Makefile.inc
include lib/libspl/Makefile.inc
include tests/Makefile.inc
