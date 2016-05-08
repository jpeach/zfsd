SUBDIRS = libck

.PHONY: all-luajit
all-luajit:
	$(MAKE) -C luajit $(AM_MAKEFLAGS) all \
		BUILDMODE="static" \
                CC="$(CC)" \
                CFLAGS="$(CFLAGS)" \
		Q=

.PHONY: clean-luajit
clean-luajit:
	$(MAKE) -C luajit $(AM_MAKEFLAGS) clean

all-local: all-luajit
clean-local: clean-luajit

