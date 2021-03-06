AC_INIT([zfsd], [1.0], [jpeach@apache.org])

AC_PREREQ([2.69])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([m4/aux])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([1.11 -Wall -Wno-portability silent-rules foreign subdir-objects])
AM_MAINTAINER_MODE([enable])
AM_SILENT_RULES([yes])

AC_PROG_CC([cc gcc clang])
AC_PROG_CXX([c++ g++ clang++])
AC_PROG_CC_C_O
AC_PROG_CXX_C_O
AM_PROG_AR

LT_CONFIG_LTDL_DIR([libltdl])
LT_INIT([dlopen])
LTDL_INIT([recursive convenience])
AC_CONFIG_FILES([libltdl/Makefile])

AX_COMPILER_VENDOR
AX_PTHREAD([], AC_MSG_ERROR(pthread support is required))

AC_ARG_ENABLE([debug],
    AC_HELP_STRING([--enable-debug], [enable additional debugging features (default: no)])
)

AX_APPEND_FLAG([-D__zfsd__], [CPPFLAGS])
AX_APPEND_FLAG([-D_GNU_SOURCE=1], [CPPFLAGS])
AX_APPEND_FLAG([-D__STDC_LIMIT_MACROS=1], [CPPFLAGS])
AX_APPEND_FLAG([-D__STDC_FORMAT_MACROS=1], [CPPFLAGS])

# Force rich debug symbol generation.
ZFSD_REMOVE_COMPILE_FLAGS([C C++], [-g])
AS_CASE($ax_cv_c_compiler_vendor,
    [gnu], [ZFSD_APPEND_COMPILE_FLAGS([C C++], [-ggdb3])],
    [ZFSD_APPEND_COMPILE_FLAGS([C C++], [-g])]
)

AS_IF([test "x$enable_debug" != "xyes"], [
    AX_APPEND_FLAG([-DNDEBUG=1], [CPPFLAGS])
], [
    # DEBUG=1 has the size-effecto of turning on ZFS_DEBUG.
    AX_APPEND_FLAG([-DDEBUG=1], [CPPFLAGS])
    # Remove autoconf's default optimizations.
    ZFSD_REMOVE_COMPILE_FLAGS([C C++], [-O2])
    ZFSD_APPEND_COMPILE_FLAGS([C C++], [-O0])
])

ZFSD_APPEND_COMPILE_FLAGS([C], [-std=gnu11])
ZFSD_APPEND_COMPILE_FLAGS([C++], [-std=gnu++11])
ZFSD_APPEND_COMPILE_FLAGS([C C++], [-Wall -Wno-unknown-pragmas -fno-omit-frame-pointer])

# openssl-devel is required.
PKG_CHECK_MODULES([CRYPTO], [libcrypto], [], AC_MSG_ERROR(libcrypto support is required))

# zlib-devel is required.
PKG_CHECK_MODULES([ZLIB], [zlib], [], AC_MSG_ERROR(zlib support is required))

ZFSD_WITH_CONCURRENCY_KIT([LIBCK], AC_MSG_ERROR(ConcurrencyKit support is required))

# aio-devel is required.
AX_CHECK_LIBRARY([AIO], [libaio.h], [aio],
    AC_SUBST([AIO_LIBS], [-laio]),
    AC_MSG_ERROR(libaio support is required)
)

# libacl-devel is required.
AX_CHECK_LIBRARY([ACL], [sys/acl.h], [acl],
    AC_SUBST([ACL_LIBS], [-lacl]),
    AC_MSG_ERROR(libacl support is required)
)

AC_OUTPUT([
    Makefile
    ext/Makefile
    src/Makefile
])

AC_MSG_NOTICE([Build option summary:
    CC:        $CC
    CXX:       $CXX
    CFLAGS:    $CFLAGS
    CXXFLAGS:  $CXXFLAGS
    CPPFLAGS:  $CPPFLAGS
    LDFLAGS:   $LDFLAGS
])
# vim: sw=4 sts=4 ts=4 et:
