SUBDIRS = $(BUNDLED_LIBCK)

# Support verbose LuaJIT builds with "make V=1".
luajit__v_ = $(luajit__v_$(AM_DEFAULT_VERBOSITY))
luajit__v_0 = @
luajit__v_1 =

LINK := cp --symbolic-link --recursive
RM_F := rm -rf

.PHONY: all-luajit
all-luajit:
	[[ -e "$(builddir)/luajit/Makefile" ]] || $(LINK) "$(abs_srcdir)/luajit" .
	$(MAKE) -C luajit $(AM_MAKEFLAGS) all \
		BUILDMODE="static" \
		CC="$(CC)" \
		CFLAGS="$(CFLAGS)" \
		Q=$(luajit__v_$(V))

.PHONY: clean-luajit
clean-luajit:
	[[ "$(top_builddir)" = "$(top_srcdir)" ]] || $(RM_F) luajit
	[[ "$(top_builddir)" != "$(top_srcdir)" ]] || $(MAKE) -C luajit $(AM_MAKEFLAGS) clean

.PHONY: all-phenom
all-phenom: libphenom/libphenom.la

libphenom/configure: libphenom/autogen.sh
	cd libphenom && ./autogen.sh

libphenom/Makefile: libphenom/configure
	cd libphenom && ./configure \
		--enable-lenient \
		--enable-static \
		--disable-shared \
		--disable-maintainer-mode \
		CC="$(CC)" CXX="$(CXX)" \
		CFLAGS="$(CFLAGS)" CXXFLAGS="$(CXXFLAGS)" \
		CPPFLAGS="$(CPPFLAGS)" LDFLAGS="$(LDFLAGS)" \
		LIBCK_CPPFLAGS="$(LIBCK_CPPFLAGS)" \
		LIBCK_LDFLAGS="$(LIBCK_LDFLAGS)"

libphenom/libphenom.la: libphenom/Makefile
	$(MAKE) -C libphenom libphenom.la V=$(V)

.PHONY: clean-phenom
clean-phenom:
	[ -r libphenom/Makefile ] && $(MAKE) -C libphenom clean
	$(RM_F) libphenom/Makefile
	$(RM_F) libphenom/configure

all-local: all-luajit all-phenom
clean-local: clean-luajit clean-phenom

