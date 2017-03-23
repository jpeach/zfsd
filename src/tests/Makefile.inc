check_PROGRAMS += check-tests

check_tests_SOURCES = \
	tests/link.cc \
	tests/main.cc \
	tests/spa.cc \
	tests/spl.cc

check_tests_LDADD = \
	libzfs.la \
	libnvpair.la \
	libskein.la \
	libspl.la \
	$(PHENOM_LIBS) \
	$(LIBCK_LIBS) \
	$(LUAJIT_LIBS) \
	$(CRYPTO_LIBS) \
	$(PTHREAD_LIBS) \
	$(ZLIB_LIBS) \
	$(LIBLTDL) \
	$(LIBADD_DL)

# Always build test programs.
noinst_PROGRAMS += $(check_PROGRAMS)

# vim: sw=8 ts=8 sts=8 noet ft=make:
