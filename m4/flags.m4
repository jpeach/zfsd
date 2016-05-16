dnl ZFSD_APPEND_COMPILE_FLAGS(language, flags)
dnl
dnl Append the given compiler flags to the given list of languages
dnl (as defined by AC_LANG_PUSH).
AC_DEFUN([ZFSD_APPEND_COMPILE_FLAGS], [
    for _z_lang in $1 ; do
        AS_CASE($_z_lang,
        [C], [
            AC_LANG_PUSH([C])
            AX_APPEND_COMPILE_FLAGS($2)
            AC_LANG_POP()
        ],
        [C++], [
            AC_LANG_PUSH([C++])
            AX_APPEND_COMPILE_FLAGS($2)
            AC_LANG_POP()
        ])
    done
    unset _z_lang
])

dnl ZFSD_REMOVE_COMPILE_FLAGS(language, flags)
dnl
dnl Given a list of languages (as defined by AC_LANG_PUSH), remove the
dnl list of flags from the corresponding FLAGS (CFLAGS) or (CXXFLAGS)
dnl variable.
AC_DEFUN([ZFSD_REMOVE_COMPILE_FLAGS], [
    for _z_lang in $1 ; do
        _z_varname=""
        _z_flags=""
        _z_keep=yes

        AS_CASE($_z_lang,
            [C], [ _z_varname=CFLAGS ],
            [C++], [ _z_varname=CXXFLAGS ]
        )

        for _z_candidate in `eval echo -n \\$$_z_varname` ; do
            _z_keep=yes
            for _z_target in $2 ; do
                AS_IF([test "x$_z_candidate" == "x$_z_target"], [_z_keep=no])
            done

            AS_IF([test "x$_z_keep" == "xyes"], [
                AS_IF([test "x$_z_flags" == "x"],
                    [_z_flags="$_z_candidate"],
                    [_z_flags="$_z_flags $_z_candidate"]
                )
            ])
        done

        eval $_z_varname=\"$_z_flags\"
    done

    unset _z_candidate
    unset _z_flags
    unset _z_keep
    unset _z_lang
    unset _z_target
    unset _z_varname
])
