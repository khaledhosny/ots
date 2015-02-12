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

AC_ARG_WITH(freetype,
	    [AS_HELP_STRING([--with-freetype=@<:@yes/no/auto@:>@],
			    [Use the FreeType library @<:@default=auto@:>@])],,
	    [with_freetype=auto])
have_freetype=false
if test "x$with_freetype" = "xyes" -o "x$with_freetype" = "xauto"; then
	PKG_CHECK_MODULES(FREETYPE, freetype2, have_freetype=true, :)
fi
if test "x$with_freetype" = "xyes" -a "x$have_freetype" != "xtrue"; then
	AC_MSG_ERROR([FreeType support requested but libfreetype2 not found])
fi
if $have_freetype; then
	AC_DEFINE(HAVE_FREETYPE, 1, [Have FreeType 2 library])
fi
AM_CONDITIONAL(HAVE_FREETYPE, $have_freetype)

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
