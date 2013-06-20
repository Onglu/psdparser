#    This file is part of "psdparse"
#    Copyright (C) 2004-2012 Toby Thain, toby@telegraphics.com.au
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by  
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License  
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

AC_INIT(psdparse, 3.6.6)
AC_CONFIG_SRCDIR(main.c)
AM_INIT_AUTOMAKE

AC_PROG_CC
AC_PROG_INSTALL

# Theoretically we could still build a restricted tool 
# if libpng weren't available, but this is not attempted yet.
AC_CHECK_LIB(png, png_create_info_struct,
			 [], [AC_MSG_ERROR(required library libpng not found)])

# Don't bother checking, we can't build at all without these.
#AC_CHECK_HEADERS([stdarg.h stdlib.h string.h getopt.h limits.h sys/stat.h])
AC_CHECK_HEADERS([iconv.h sys/mman.h zlib.h])

# Only test for functions where it is possible to work around
# their absence.
#AC_CHECK_FUNC(vsnprintf)

AC_OUTPUT(Makefile)
