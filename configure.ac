m4_define([package_version_major],[0])
m4_define([package_version_minor],[1])
m4_define([package_version_micro],[0])
m4_define([api_version],[1])
AC_INIT([libwaftp],
        [package_version_major.package_version_minor.package_version_micro],
        [liaojunxuan@whu.edu.cn]
        )
AC_PREREQ([2.71])
AM_PROG_AR
AC_CONFIG_AUX_DIR([build_aux])
AC_CONFIG_MACRO_DIRS([m4])
AC_CONFIG_SRCDIR([src/ftp.c])
AM_INIT_AUTOMAKE([-Wall -Werror foreign 1.11.2])
LT_INIT

PKG_INSTALLDIR


AC_PROG_CC
AX_CHECK_COMPILE_FLAG([-std=gnu11], [
    CFLAGS="$CFLAGS -std=gnu17"
], [
    echo "-std=gnu17 required"
])
PKG_CHECK_MODULES([CHECK], [check >= 0.9.6])

AC_SUBST([PACKAGE_VERSION_MAJOR],package_version_major)
AC_SUBST([PACKAGE_VERSION_MINOR],package_version_minor)
AC_SUBST([PACKAGE_VERSION_MICRO],package_version_micro)
AC_SUBST([API_VERSION],api_version)

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
    Makefile src/Makefile tests/Makefile
    libwaftp-$API_VERSION.pc:libwaftp.pc.in
], [], [API_VERSION='$API_VERSION'])

DX_INIT_DOXYGEN([libwaftp])

AC_OUTPUT
