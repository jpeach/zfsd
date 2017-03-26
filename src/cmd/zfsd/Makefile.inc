sbin_PROGRAMS += zfsd

zfsd_SOURCES = \
	cmd/zfsd/init.c \
	cmd/zfsd/zfsd.cc

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
