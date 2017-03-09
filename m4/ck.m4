dnl ZFSD_WITH_CONCURRENCY_KIT(basevar, action-if-not-found)
dnl
dnl Search for Concurrency Kit, setting CK_LDFLAGS and CK_CPPFLAGS as
dnl to the tested paths. Concurrency Kit ships with a pkg-config file
dnl that you are supposed to use, but it places the linker path in the
dnl $LIBS variable and doesn't set the rpath.
AC_DEFUN([ZFSD_WITH_CONCURRENCY_KIT], [
    AC_ARG_WITH([concurrency-kit],
        AS_HELP_STRING([--with-concurrency-kit=DIR],
            [use the Concurrency Kit installed in DIR @<:@default=bundled@:>@]),
        [],
        [ with_concurrency_kit=bundled]
    )

    AS_CASE($with_concurrency_kit,
    [yes|no|bundled], [
        AC_MSG_CHECKING([for Concurrency Kit])
        AC_MSG_RESULT([bundled])

        $1_LDFLAGS=""
        $1_CPPFLAGS="-I\$(abs_top_srcdir)/ext/libck/include"
        $1_LIBS="\$(abs_top_builddir)/ext/libck/src/libck.a"

        AC_CONFIG_SUBDIRS([ext/libck])

        AC_SUBST($1_LDFLAGS)
        AC_SUBST($1_CPPFLAGS)
        AC_SUBST($1_LIBS)

        # Add the ext/libck SIBDIR into the build.
        BUNDLED_LIBCK=libck
        AC_SUBST(BUNDLED_LIBCK)
    ], [
        ZFSD_CHECK_CONCURRENCY_KIT([$1], [$with_concurrency_kit], [$2])
    ])
])

dnl ZFSD_CHECK_CONCURRENCY_KIT(basevar, prefix, action-if-not-found)
AC_DEFUN([ZFSD_CHECK_CONCURRENCY_KIT], [
    _z_ck_FOUND=no
    _z_ck_LDPATH=
    _z_ck_CPPFLAGS=$CPPFLAGS
    _z_ck_LDFLAGS=$LDFLAGS
    _z_ck_LIBS=$LIBS

    CPPFLAGS="-I$2/include"

    AC_CHECK_HEADER([ck_epoch.h], [
        for dir in lib64 lib ; do
            AS_IF([test x$_z_ck_FOUND = xno], [
                _z_ck_LDPATH="$2/$dir"
                LDFLAGS="-L$_z_ck_LDPATH"
                # Unset the cache variable so force a search after
                # we changed $LDFLAGS
                unset ac_cv_lib_ck_ck_epoch_init
                AC_CHECK_LIB([ck], [ck_epoch_init], [_z_ck_FOUND=yes], [_z_ck_FOUND=no])
            ])
        done
    ])

    AC_MSG_CHECKING([for Concurrency Kit in $2])
    AC_MSG_RESULT([$_z_ck_FOUND])

    AS_IF([test x$_z_ck_FOUND = xyes], [
        # action-if-not-found

        $1_LDFLAGS="-L$_z_ck_LDPATH -R$_z_ck_LDPATH"
        $1_CPPFLAGS=$CPPFLAGS
        $1_LIBS=-lck

        AC_SUBST($1_LDFLAGS)
        AC_SUBST($1_CPPFLAGS)
        AC_SUBST($1_LIBS)

    ], [
        # action-if-not-found
        $3
    ])

    CPPFLAGS=$_z_ck_CPPFLAGS
    LDFLAGS=$_z_ck_LDFLAGS
    LIBS=$_z_ck_LIBS

    unset _z_ck_FOUND
    unset _z_ck_LDPATH
    unset _z_ck_CPPFLAGS
    unset _z_ck_LDFLAGS
    unset _z_ck_LIBS
])

dnl vim: set sw=4 ts=4 sts=4 et :
