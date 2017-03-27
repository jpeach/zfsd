sbin_PROGRAMS += zfsd

zfsd_SOURCES = \
	cmd/zfsd/init.c \
	cmd/zfsd/zfsd.cc

# NOTE: We add "-dlopen force" to tell libtool to always generate the
# preloaded symbol table for LTDL_SET_PRELOADED_SYMBOLS.
zfsd_LDFLAGS = \
	$(AM_LDFLAGS) \
	-dlopen force

zfsd_LDADD = \
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

# vim: sw=8 ts=8 sts=8 noet ft=make:
