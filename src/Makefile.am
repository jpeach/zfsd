pkglib_LTLIBRARIES =
noinst_LTLIBRARIES =
bin_PROGRAMS =
check_PROGRAMS =

CATCH = $(top_srcdir)/ext/catch
CATCH_CPPFLAGS = -I$(CATCH)

LIBCK_CPPFLAGS = -I$(top_srcdir)/ext/libck/include
LIBCK_LIBS = $(top_builddir)/ext/libck/src/libck.a

LUAJIT_CPPFLAGS = -I$(top_srcdir)/ext/luajit/src
LUAJIT_LIBS = $(top_builddir)/ext/luajit/src/libluajit.a

AM_CPPFLAGS = \
	$(LTDLINCL) \
	$(CATCH_CPPFLAGS) \
	$(LIBCK_CPPFLAGS) \
	$(LUAJIT_CPPFLAGS)

check_PROGRAMS += check-tests

check_tests_SOURCES = \
	tests/link.cc \
	tests/main.cc 

check_tests_LDADD = \
	$(LIBCK_LIBS) \
	$(LUAJIT_LIBS) \
	$(LIBLTDL) \
	-ldl


# Always build test programs.
noinst_PROGRAMS = $(check_PROGRAMS)
