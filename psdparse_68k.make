# This file is part of "psdparse"
# Copyright (C) 2004-9 Toby Thain, toby@telegraphics.com.au

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by  
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License  
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

PNGDIR = ::libpng-1.2.35:
PNGOBJ = ¶
	{PNGDIR}png.c.o {PNGDIR}pngerror.c.o {PNGDIR}pngget.c.o ¶
	{PNGDIR}pngmem.c.o {PNGDIR}pngset.c.o ¶
	{PNGDIR}pngtrans.c.o {PNGDIR}pngwio.c.o {PNGDIR}pngwrite.c.o ¶
	{PNGDIR}pngwtran.c.o {PNGDIR}pngwutil.c.o 

ZLIBDIR = ::zlib-1.2.3:
ZLIBOBJ = ¶
	{ZLIBDIR}adler32.c.o {ZLIBDIR}compress.c.o {ZLIBDIR}crc32.c.o ¶
	{ZLIBDIR}deflate.c.o {ZLIBDIR}infback.c.o ¶
	{ZLIBDIR}inffast.c.o {ZLIBDIR}inflate.c.o {ZLIBDIR}inftrees.c.o ¶
	{ZLIBDIR}trees.c.o {ZLIBDIR}uncompr.c.o {ZLIBDIR}zutil.c.o

OBJ = extra.c.o channel.c.o constants.c.o descriptor.c.o ¶
	psd.c.o icc.c.o resources.c.o write.c.o writeraw.c.o writepng.c.o ¶
	unpackbits.c.o mkdir_unimpl.c.o util.c.o psd_zip.c.o pdf.c.o ¶
	getopt.c.o getopt1.c.o ¶
	{PNGOBJ} {ZLIBOBJ} main.c.o

LIB =			  "{Libraries}MacRuntime.o" ¶
				  "{Libraries}Stubs.o" ¶
				  "{CLibraries}StdCLib.o" ¶
				  "{Libraries}IntEnv.o" ¶
				  "{Libraries}Interface.o"

# "-enum int -d STDC" are needed to make zlib build happily
COptions = -enum int -i {PNGDIR},{ZLIBDIR} -w 2,35 ¶
	-d STDC -d MAC_ENV -d DEFAULT_VERBOSE=0 -d DIRSEP=¶':¶' ¶
	-d PNG_NO_SNPRINTF -d comp=compvar # comp is an IEEE fp type, on Mac 68K

.c.o Ä .c
	{C} {depDir}{default}.c -o {targDir}{default}.c.o {COptions} -model far

psdparse_68k ÄÄ {OBJ} {LIB}
	Link -o {Targ} {OBJ} {LIB} -d -t 'MPST' -c 'MPS ' -model far
