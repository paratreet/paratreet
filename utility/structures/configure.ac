#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.


AC_PREREQ(2.59)
AC_INIT(FULL-PACKAGE-NAME, VERSION, BUG-REPORT-ADDRESS)
AC_CONFIG_SRCDIR([TipsyFile.cpp])
AC_CONFIG_HEADER([config.h])

AC_PROG_CXX
AC_CHECK_TOOL(AR, ar) 

# Checks for header files.
AC_CHECK_HEADERS([stdint.h endian.h machine/endian.h sys/bitypes.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_CHECK_TYPES([u_quad_t, u_int64_t])

# Checks for library functions.
AC_HEADER_STDC
AC_CHECK_FUNCS([floor memset sqrt xdr_hyper])

# 128 bit keys:
AC_ARG_ENABLE([bigkeys],
        [AS_HELP_STRING([--enable-bigkeys], [enable 128 bit hash keys])],
        [bigkeys=yes], [bigkeys=no])
if test "$bigkeys" = "yes" ; then
    echo "BIGKEYS selected"
    FLAG_BIGKEYS="-DBIGKEYS"
else
    FLAG_BIGKEYS=""
fi

AC_SUBST([FLAG_BIGKEYS])

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
