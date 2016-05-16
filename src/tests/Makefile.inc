check_PROGRAMS += check-tests

check_tests_SOURCES = \
	tests/link.cc \
	tests/main.cc

check_tests_LDADD = \
	$(LIBCK_LIBS) \
	$(LUAJIT_LIBS) \
	$(LIBLTDL) \
	libnvpair.la \
	libspl.la \
	-ldl

# Always build test programs.
noinst_PROGRAMS += $(check_PROGRAMS)

# vim: sw=8 ts=8 sts=8 noet ft=make:
