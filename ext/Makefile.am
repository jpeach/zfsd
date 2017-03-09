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

all-local: all-luajit
clean-local: clean-luajit

