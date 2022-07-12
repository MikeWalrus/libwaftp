AC_INIT([libwaftp],[0.1],[liaojunxuan@whu.edu.cn])
AC_PREREQ([2.71])
AM_PROG_AR
AC_CONFIG_AUX_DIR([build_aux])
AC_CONFIG_MACRO_DIRS([m4])
AC_CONFIG_SRCDIR([src/ftp.c])
AM_INIT_AUTOMAKE([-Wall -Werror foreign 1.11.2])
LT_INIT

AC_PROG_CC
AX_CHECK_COMPILE_FLAG([-std=gnu11], [
    CFLAGS="$CFLAGS -std=gnu17"
], [
    echo "-std=gnu17 required"
])
PKG_CHECK_MODULES([CHECK], [check >= 0.9.6])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile tests/Makefile])

DX_INIT_DOXYGEN([libwaftp])

AC_OUTPUT
