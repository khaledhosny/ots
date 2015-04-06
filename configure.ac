AC_INIT([OTS],
        [0.0.1],
        [https://github.com/khaledhosny/ots/issues],
        [ots],
        [https://github.com/khaledhosny/ots])

AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([foreign subdir-objects])
AM_SILENT_RULES([yes])

AC_PROG_CC
AC_PROG_CXX
AC_PROG_RANLIB
PKG_PROG_PKG_CONFIG([0.20])

PKG_CHECK_MODULES(FREETYPE, freetype2, have_freetype=true, :)
if $have_freetype; then
	AC_DEFINE(HAVE_FREETYPE, 1, [Have FreeType 2 library])
fi
AM_CONDITIONAL(HAVE_FREETYPE, $have_freetype)

AX_PTHREAD([have_pthread=true])
if $have_pthread; then
	GTEST_CPPFLAGS="-DGTEST_HAS_PTHREAD=1"
else
	GTEST_CPPFLAGS="-DGTEST_HAS_PTHREAD=0"
fi
AC_SUBST([GTEST_CPPFLAGS])

AC_CONFIG_FILES([
Makefile
test/Makefile
])

AC_OUTPUT
