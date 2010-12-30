#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([Virtual OSC Controller], [dev])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/controller.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC(gcc cc owcc)
AC_PROG_CC_C_O
AC_PROG_CC_C99
AC_PROG_INSTALL
AC_PROG_LIBTOOL

# Checks for libraries.
AC_CHECK_LIB(expat, XML_ParserCreate, , [
	AC_MSG_ERROR([Required libexpat is missing!])
])

# libSDL depends on sdl-config script
AC_CHECK_PROG(SDL_CONFIG, sdl-config, sdl-config)
if test x$SDL_CONFIG = x; then
	AC_MSG_ERROR([Required sdl-config script is missing! libSDL cannot be configured.])
fi

CFLAGS="$CFLAGS `$SDL_CONFIG --cflags`"
LIBS="$LIBS `$SDL_CONFIG --libs`"
AC_DEFINE(HAVE_LIBSDL, , [We've got libSDL])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h netdb.h netinet/in.h stdlib.h string.h sys/socket.h sys/select.h unistd.h])

AC_CHECK_HEADERS([expat.h], , [
	AC_MSG_ERROR([Required libexpat headers are missing!])
])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_CHECK_TYPE(in_addr_t, [
	AC_DEFINE([HAVE_IN_ADDR_T], , [netinet/in.h defines in_addr_t type])
], , [
	#include <netinet/in.h>
])

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([atexit gethostbyname memset socket strcasecmp strchr strdup strrchr strtoul])

# Arbitrary defines
AC_DEFINE([OSC_NOBUNDLES], , [Don't include OSC bundle support in OSC-client.c])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT

