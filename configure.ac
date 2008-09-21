AC_PREREQ(2.62)
AC_INIT([opentesarena], [0.1], [trankas@gmail.com])
AC_CONFIG_SRCDIR([src/main.cpp])
AC_CONFIG_HEADERS([config.h])

AC_HEADER_STDBOOL
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB

AC_TYPE_OFF_T

AC_LANG_CPLUSPLUS
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_FILES([Makefile
                 src/Makefile
                 src/dataextraction/Makefile
                 src/utils/Makefile])
AC_CHECK_LIB(boost_signals, main,, [
  AC_MSG_ERROR([You need the C++ boost library.])])
PKG_CHECK_MODULES(OGRE, OGRE >= 0.14.1,,AC_MSG_ERROR("OGRE not found!"))

AC_OUTPUT
