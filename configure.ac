#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
#
AC_PREREQ([2.67])
AC_INIT([aria2],[1.37.0],[https://github.com/aria2/aria2/issues],[aria2],[https://aria2.github.io/])

AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([subdir-objects])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_USE_SYSTEM_EXTENSIONS

LT_PREREQ([2.2.6])
LT_INIT([disable-static])
dnl See versioning rule:
dnl  http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
AC_SUBST(LT_CURRENT, 0)
AC_SUBST(LT_REVISION, 0)
AC_SUBST(LT_AGE, 0)

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/a2io.h])
AC_CONFIG_HEADERS([config.h])

# extra flags
EXTRACFLAGS=
EXTRACXXFLAGS=
EXTRACPPFLAGS=
EXTRALDFLAGS=
EXTRALIBS=

AC_DEFINE_UNQUOTED([BUILD], ["$build"], [Define build-type])
AC_DEFINE_UNQUOTED([HOST], ["$host"], [Define build-type])
AC_DEFINE_UNQUOTED([TARGET], ["$target"], [Define target-type])

# Checks for arguments.
ARIA2_ARG_WITHOUT([openssl])
ARIA2_ARG_WITHOUT([sqlite3])
ARIA2_ARG_WITHOUT([libcares])
ARIA2_ARG_WITHOUT([libz])
ARIA2_ARG_WITH([jemalloc])

ARIA2_ARG_DISABLE([ssl])
ARIA2_ARG_DISABLE([bittorrent])
ARIA2_ARG_DISABLE([websocket])
ARIA2_ARG_ENABLE([libaria2])
ARIA2_ARG_ENABLE([werror])

AC_ARG_WITH([ca-bundle],
  AS_HELP_STRING([--with-ca-bundle=FILE],[Use FILE as default CA bundle.]),
  [AC_DEFINE_UNQUOTED([CA_BUNDLE], ["$withval"], [Define to choose default CA bundle.])
   ca_bundle=$withval
  ], [])

AC_ARG_WITH([disk-cache],
  AS_HELP_STRING([--with-disk-cache=SIZE],[Use SIZE as the default disk-cache size.]),
  [AC_DEFINE_UNQUOTED([DEFAULT_DISK_CACHE], ["$withval"], [Define to choose default disk-cache size])],
  [])

AC_ARG_WITH([bashcompletiondir],
  AS_HELP_STRING([--with-bashcompletiondir=DIR],
                 [Directory to install bash_completion file]),
  [bashcompletiondir=$withval], [bashcompletiondir=$docdir/bash_completion])

AC_ARG_VAR([ARIA2_STATIC], [Set 'yes' to build a statically linked aria2])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MKDIR_P
AC_PROG_YACC
AM_PROG_AS

# Speed GCC compilation up.
if test "$GCC" = yes; then
  EXTRACFLAGS="$EXTRACFLAGS -pipe"
fi
if test "$GXX" = yes; then
  EXTRACXXFLAGS="$EXTRACXXFLAGS -pipe"
fi

AC_CHECK_TOOL([AR], [ar], [:])
if test "x$AR" = "x:"; then
  AC_MSG_FAILURE([ar is not found in the system.])
fi
AC_SUBST([AR])

AC_PATH_PROGS([SPHINXBUILD], [sphinx-build])
AC_SUBST([SPHINXBUILD])
AM_CONDITIONAL([HAVE_SPHINXBUILD], [ test "x$SPHINXBUILD" != "x" ])

AC_PATH_PROGS([RST2HTML], [rst2html.py rst2html])
AC_SUBST([RST2HTML])
AM_CONDITIONAL([HAVE_RST2HTML], [ test "x$RST2HTML" != "x" ])

# Setting language choice
AC_LANG([C++])

# Check pkg-config is available
PKG_PROG_PKG_CONFIG([0.20])

# Check C++ compiler supports C++20 feature
save_CXXFLAGS="$CXXFLAGS"
CXXFLAGS=

AX_CXX_COMPILE_STDCXX([23], [noext], [mandatory])

CXXSTDCXXFLAGS="$CXXFLAGS"
CXXFLAGS="$save_CXXFLAGS"
AC_SUBST([CXXSTDCXXFLAGS])

# Check static build is requested
if test "x$ARIA2_STATIC" = "xyes"; then
  # Make pkg-config produce static linking variables
  PKG_CONFIG="$PKG_CONFIG --static"
else
  dnl Make variable empty to avoid confusion
  ARIA2_STATIC=no
fi

# Checks for libraries.

# Check availability of cppunit
PKG_CHECK_MODULES([CPPUNIT], [cppunit >= 1.10.2],
                  [have_cppunit=yes], [have_cppunit=no])
if test "x$have_cppunit" != "xyes"; then
  AC_MSG_WARN([$CPPUNIT_PKG_ERRORS])
fi

# Check availability of libz
if test "x$with_libz" = "xyes"; then
  PKG_CHECK_MODULES([ZLIB], [zlib >= 1.2.3], [have_zlib=yes], [have_zlib=no])
  if test "x$have_zlib" != "xyes"; then
    AC_MSG_WARN([$ZLIB_PKG_ERRORS])
    AC_CHECK_LIB([z], [zlibVersion], [have_zlib=yes], [have_zlib=no])

    if test "x$have_zlib" = "xyes"; then
      ZLIB_CFLAGS=
      ZLIB_LIBS="-lz"
      AC_SUBST([ZLIB_CFLAGS])
      AC_SUBST([ZLIB_LIBS])
    elif test "x$with_libz_requested" = "xyes"; then
      ARIA2_DEP_NOT_MET([libz])
    fi
  fi
  if test "x$have_zlib" = "xyes"; then
    AC_DEFINE([HAVE_ZLIB], [1], [Define to 1 if you have zlib.])
    save_CPPFLAGS=$CPPFLAGS
    save_LIBS=$LIBS
    CPPFLAGS="$CPPFLAGS $ZLIB_CFLAGS"
    LIBS="$ZLIB_LIBS $LIBS"
    AC_CHECK_FUNCS([gzbuffer gzsetparams])
    CPPFLAGS=$save_CPPFLAGS
    LIBS=$save_LIBS
  fi
fi

have_libuv=yes
PKG_CHECK_MODULES([LIBUV], [libuv >= 1.13])

have_sqlite3=no
if test "x$with_sqlite3" = "xyes"; then
  PKG_CHECK_MODULES([SQLITE3],[sqlite3],[have_sqlite3=yes],[have_sqlite3=no])
  if test "x$have_sqlite3" = "xyes"; then
    AC_DEFINE([HAVE_SQLITE3], [1], [Define to 1 if you have sqlite3.])
    save_LIBS=$LIBS
    LIBS="$SQLITE3_LIBS $LIBS"
    AC_CHECK_FUNCS([sqlite3_open_v2])
    LIBS=$save_LIBS
  else
    AC_MSG_WARN([$SQLITE3_PKG_ERRORS])
    if test "x$with_sqlite3_requested" = "xyes"; then
      ARIA2_DEP_NOT_MET([sqlite3])
    fi
  fi
fi

# OpenSSL is the only optional TLS/crypto backend. Without it the
# internal implementations are used and HTTPS is disabled.
have_openssl=no
have_ssl=no
if test "x$enable_ssl" = "xyes" && test "x$with_openssl" = "xyes"; then
  PKG_CHECK_MODULES([OPENSSL], [openssl >= 0.9.8],
                    [have_openssl=yes], [have_openssl=no])
  if test "x$have_openssl" = "xyes"; then
    have_ssl=yes
    AC_DEFINE([HAVE_OPENSSL], [1], [Define to 1 if you have openssl.])
    save_LIBS=$LIBS
    LIBS="$OPENSSL_LIBS $LIBS"
    AC_CHECK_FUNCS([EVP_sha224])
    LIBS=$save_LIBS
  else
    AC_MSG_WARN([$OPENSSL_PKG_ERRORS])
    if test "x$with_openssl_requested" = "xyes"; then
      ARIA2_DEP_NOT_MET([openssl])
    fi
  fi
fi

have_libcares=no
if test "x$with_libcares" = "xyes"; then
  PKG_CHECK_MODULES([LIBCARES], [libcares >= 1.16.0], [have_libcares=yes],
                    [have_libcares=no])
  if test "x$have_libcares" = "xyes"; then
    AC_DEFINE([HAVE_LIBCARES], [1], [Define to 1 if you have libcares.])

    # -DCARES_STATICLIB is appended by pkg-config file libcares.pc
  else
    AC_MSG_WARN([$LIBCARES_PKG_ERRORS])
    if test "x$with_libcares_requested" = "xyes"; then
      ARIA2_DEP_NOT_MET([libcares])
    fi
  fi
fi

if test "x$have_openssl" = "xyes"; then
  AC_DEFINE([USE_OPENSSL_MD], [1], [What message digest implementation to use])
  use_md="openssl"
else
  AC_DEFINE([USE_INTERNAL_MD], [1], [What message digest implementation to use])
  use_md="internal"
fi

if test "x$have_ssl" = "xyes"; then
  AC_DEFINE([ENABLE_SSL], [1], [Define to 1 if ssl support is enabled.])
  AM_CONDITIONAL([ENABLE_SSL], true)
else
  AM_CONDITIONAL([ENABLE_SSL], false)
fi

AM_CONDITIONAL([HAVE_OPENSSL], [test "x$have_openssl" = "xyes"])
AM_CONDITIONAL([USE_OPENSSL_MD], [test "x$use_md" = "xopenssl"])
AM_CONDITIONAL([USE_INTERNAL_MD], [test "x$use_md" = "xinternal"])

if test "x$have_openssl" = "xyes"; then
  AM_CONDITIONAL([USE_INTERNAL_BIGNUM], false)
else
  AC_DEFINE([USE_INTERNAL_BIGNUM], [1], [Define to 1 if internal BIGNUM support is enabled.])
  AM_CONDITIONAL([USE_INTERNAL_BIGNUM], true)
fi

if test "x$enable_bittorrent" = "xyes"; then
  AC_DEFINE([ENABLE_BITTORRENT], [1],
            [Define to 1 if BitTorrent support is enabled.])
  AM_CONDITIONAL([ENABLE_BITTORRENT], true)
else
  if test "x$enable_bittorrent_requested" = "xyes"; then
    ARIA2_FET_NOT_SUPPORTED([bittorrent])
  fi
  enable_bittorrent=no
  AM_CONDITIONAL([ENABLE_BITTORRENT], false)
fi

if test "x$have_libcares" = "xyes"; then
  AC_DEFINE([ENABLE_ASYNC_DNS], [1],
            [Define to 1 if asynchronous DNS support is enabled.])
  AM_CONDITIONAL([ENABLE_ASYNC_DNS], true)
else
  AM_CONDITIONAL([ENABLE_ASYNC_DNS], false)
fi

# Set conditional for libz
AM_CONDITIONAL([HAVE_ZLIB], [test "x$have_zlib" = "xyes"])

# Set conditional for sqlite3
AM_CONDITIONAL([HAVE_SQLITE3], [test "x$have_sqlite3" = "xyes"])

# Checks for header files.
AC_PROG_EGREP

AC_CHECK_HEADERS([argz.h \
                  arpa/inet.h \
                  fcntl.h \
                  float.h \
                  inttypes.h \
                  langinfo.h \
                  limits.h \
                  locale.h \
                  malloc.h \
                  netdb.h \
                  netinet/in.h \
                  netinet/tcp.h \
                  poll.h \
                  signal.h \
                  stddef.h \
                  stdint.h \
                  stdio_ext.h \
                  stdlib.h \
                  string.h \
                  strings.h \
                  sys/ioctl.h \
                  sys/param.h \
                  sys/resource.h \
                  sys/signal.h \
                  sys/socket.h \
                  sys/time.h \
                  sys/types.h \
                  sys/uio.h \
                  sys/utsname.h \
                  termios.h \
                  unistd.h \
                  utime.h \
                  wchar.h \
                  ifaddrs.h \
                  pwd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_CHECK_HEADERS_ONCE([sys/time.h])

AC_STRUCT_TM
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_TYPE_PID_T
AC_C_VOLATILE
AC_CHECK_TYPES([ptrdiff_t])
AC_CHECK_TYPE([struct timespec], [have_timespec=yes], [have_timespec=no])
AC_C_BIGENDIAN
AC_SYS_LARGEFILE

if test "x$have_timespec" = "xyes"; then
  AC_DEFINE([HAVE_A2_STRUCT_TIMESPEC], [1],
            [Define to 1 if the system has the type `struct timespec'.])
fi

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE

AC_FUNC_MEMCMP
AC_FUNC_MKTIME
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_FUNC_VPRINTF
AC_FUNC_FORK
AC_FUNC_STRTOD
# Don't use AC_FUNC_MMAP because it fails on some platforms (e.g.,
# OpenWRT) which have mmap and it works in the way we use in aria2.
# Instead use mmap in AC_CHECK_FUNCS list.
AC_CHECK_FUNCS([__argz_count \
                __argz_next \
                __argz_stringify \
                atexit \
                ftruncate \
                getcwd \
                getentropy \
                gethostbyaddr \
                gethostbyname \
                getifaddrs \
                getpagesize \
                memchr \
                memmove \
                mempcpy \
                memset \
                mkdir \
                mmap \
                munmap \
                nl_langinfo \
                posix_fadvise \
                posix_memalign \
                pow \
                putenv \
                rmdir \
                select \
                setlocale \
                sigaction \
                sleep \
                socket \
                stpcpy \
                strcasecmp \
                strchr \
                strcspn \
                strdup \
                strerror \
                strncasecmp \
                strstr \
                strtol \
                strtoul \
                strtoull \
                tzset \
                unsetenv \
                usleep \
                utime \
                utimes])

dnl Put jemalloc check after the posix_memalign check.
dnl jemalloc may implement posix_memalign while the usual CRT may not.
dnl Since we aren't including its headers this will lead to undefined
dnl posix_memalign() errors when compiling.
have_jemalloc=no
if test "x$with_jemalloc" = "xyes"; then
  dnl Important: put malloc libs at the very end.
  dnl Usually jemalloc does not come with a .pc, as the official source does not
  dnl generate one.
  PKG_CHECK_MODULES([JEMALLOC], [jemalloc], [have_jemalloc=yes], [have_jemalloc=no])
  if test "x$have_jemalloc" != "xyes"; then
    AC_CHECK_LIB([jemalloc], [malloc], [have_jemalloc=yes], [have_jemalloc=no])
    if test "x$have_jemalloc" = "xyes"; then
      JEMALLOC_CFLAGS=
      JEMALLOC_LIBS="-ljemalloc"
      AC_SUBST([JEMALLOC_CFLAGS])
      AC_SUBST([JEMALLOC_LIBS])
    else
      if test "x$with_jemalloc_requested" = "xyes"; then
        ARIA2_DEP_NOT_MET([jemalloc (unprefixed)])
      fi
    fi
  fi
fi

AC_CHECK_FUNCS([posix_fallocate],[have_posix_fallocate=yes])
ARIA2_CHECK_FALLOCATE
if test "x$have_posix_fallocate" = "xyes" ||
   test "x$have_fallocate" = "xyes"; then
  AC_DEFINE([HAVE_SOME_FALLOCATE], [1],
            [Define to 1 if *_fallocate is available.])
fi
AM_CONDITIONAL([HAVE_SOME_FALLOCATE],
  [test "x$have_posix_fallocate" = "xyes" || test "x$have_fallocate" = "xyes"])

AC_CHECK_FUNCS([daemon], [have_daemon=yes])
AM_CONDITIONAL([HAVE_DAEMON], [test "x$have_daemon" = "xyes"])

AC_CHECK_FUNCS([poll], [have_poll=yes])
AM_CONDITIONAL([HAVE_POLL], [test "x$have_poll" = "xyes"])

AC_CHECK_MEMBER([struct sockaddr_in.sin_len],
                [AC_DEFINE([HAVE_SOCKADDR_IN_SIN_LEN],[1],
                  [Define to 1 if struct sockaddr_in has sin_len member.])],
                [],
                [[
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
]])

AC_CHECK_MEMBER([struct sockaddr_in6.sin6_len],
                [AC_DEFINE([HAVE_SOCKADDR_IN6_SIN6_LEN],[1],
                  [Define to 1 if struct sockaddr_in6 has sin6_len member.])],
                [],
                [[
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
]])

# Check struct option.name is assignable from const char*.
AC_MSG_CHECKING([whether struct option.name is assignable from const char*])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <unistd.h>
#include <getopt.h>
]],
[[
const char* s = "const char";
option op;
op.name = s;
]])],
[have_option_const_name=yes], [have_option_const_name=no])
AC_MSG_RESULT([$have_option_const_name])
if test "x$have_option_const_name" = "xyes"; then
  AC_DEFINE([HAVE_OPTION_CONST_NAME], [1], [Define 1 if struct option.name is const char*])
fi

if test "x$enable_websocket" = "xyes"; then
  AC_CONFIG_SUBDIRS([deps/wslay])
  enable_websocket=yes
  AC_DEFINE([ENABLE_WEBSOCKET], [1],
            [Define 1 if WebSocket support is enabled.])
  # $(top_srcdir) for `make distcheck`
  WSLAY_CFLAGS="-I\$(top_builddir)/deps/wslay/lib/includes -I\$(top_srcdir)/deps/wslay/lib/includes"
  WSLAY_LIBS="\$(top_builddir)/deps/wslay/lib/libwslay.la"
  AC_SUBST([WSLAY_CFLAGS])
  AC_SUBST([WSLAY_LIBS])
fi
AM_CONDITIONAL([ENABLE_WEBSOCKET], [test "x$enable_websocket" = "xyes"])

AM_CONDITIONAL([ENABLE_LIBARIA2], [test "x$enable_libaria2" = "xyes"])

AC_SUBST([bashcompletiondir])

if test "x$ARIA2_STATIC" = "xyes"; then
  # -static-libgcc and -static-libstdc++ are linker flags and not for
  # libtool.
  EXTRALDFLAGS="$EXTRALDFLAGS -all-static"
  dnl we need additional libs for static build.
  EXTRALIBS="$EXTRALIBS -lpthread -ldl -lrt"
fi

AC_SUBST([EXTRACFLAGS])
AC_SUBST([EXTRACXXFLAGS])
AC_SUBST([EXTRACPPFLAGS])
AC_SUBST([EXTRALDFLAGS])
AC_SUBST([EXTRALIBS])

save_CXXFLAGS=$CXXFLAGS
CXXFLAGS=

if test "x$enable_werror" = "xyes"; then
  AX_CHECK_COMPILE_FLAG([-Wall], [CXXFLAGS="$CXXFLAGS -Wall"])
  AX_CHECK_COMPILE_FLAG([-Wextra], [CXXFLAGS="$CXXFLAGS -Wextra"])
  AX_CHECK_COMPILE_FLAG([-Werror], [CXXFLAGS="$CXXFLAGS -Werror"])
  AX_CHECK_COMPILE_FLAG([-Wformat-security], [CXXFLAGS="$CXXFLAGS -Wformat-security"])
fi

WARNCXXFLAGS=$CXXFLAGS
CXXFLAGS=$save_CXXFLAGS

AC_SUBST([WARNCXXFLAGS])

AC_CONFIG_FILES([Makefile
                src/Makefile
                src/libaria2.pc
                src/includes/Makefile
                test/Makefile
                doc/Makefile
                doc/manual-src/Makefile
                doc/manual-src/en/Makefile
                doc/manual-src/en/conf.py
                deps/Makefile])
AC_OUTPUT

AC_MSG_NOTICE([summary of build options:

Build:          $build
Host:           $host
Target:         $target
Install prefix: $prefix
CC:             $CC
CXX:            $CXX
CPP:            $CPP
CXXFLAGS:       $CXXFLAGS
CFLAGS:         $CFLAGS
CPPFLAGS:       $CPPFLAGS
LDFLAGS:        $LDFLAGS
LIBS:           $LIBS
DEFS:           $DEFS
CXXSTDCXXFLAGS: $CXXSTDCXXFLAGS
EXTRACXXFLAGS:  $EXTRACXXFLAGS
EXTRACFLAGS:    $EXTRACFLAGS
EXTRACPPFLAGS:  $EXTRACPPFLAGS
EXTRALDFLAGS:   $EXTRALDFLAGS
EXTRALIBS:      $EXTRALIBS
WARNCXXFLAGS:   $WARNCXXFLAGS
LibUV:          $have_libuv (CFLAGS='$LIBUV_CFLAGS' LIBS='$LIBUV_LIBS')
SQLite3:        $have_sqlite3 (CFLAGS='$SQLITE3_CFLAGS' LIBS='$SQLITE3_LIBS')
SSL Support:    $have_ssl
OpenSSL:        $have_openssl (CFLAGS='$OPENSSL_CFLAGS' LIBS='$OPENSSL_LIBS')
CA Bundle:      $ca_bundle
LibCares:       $have_libcares (CFLAGS='$LIBCARES_CFLAGS' LIBS='$LIBCARES_LIBS')
Zlib:           $have_zlib (CFLAGS='$ZLIB_CFLAGS' LIBS='$ZLIB_LIBS')
Jemalloc:       $have_jemalloc (CFLAGS='$JEMALLOC_CFLAGS' LIBS='$JEMALLOC_LIBS')
Bittorrent:     $enable_bittorrent
Message Digest: $use_md
WebSocket:      $enable_websocket (CFLAGS='$WSLAY_CFLAGS' LIBS='$WSLAY_LIBS')
Libaria2:       $enable_libaria2 (shared=${enable_shared} static=${enable_static})
bash_completion dir: $bashcompletiondir
Static build:   $ARIA2_STATIC
])
