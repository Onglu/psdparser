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
	{PNGDIR}png.c.x {PNGDIR}pngerror.c.x {PNGDIR}pngget.c.x ¶
	{PNGDIR}pngmem.c.x {PNGDIR}pngset.c.x ¶
	{PNGDIR}pngtrans.c.x {PNGDIR}pngwio.c.x {PNGDIR}pngwrite.c.x ¶
	{PNGDIR}pngwtran.c.x {PNGDIR}pngwutil.c.x 

ZLIBDIR = ::zlib-1.2.3:
ZLIBOBJ = ¶
	{ZLIBDIR}adler32.c.x {ZLIBDIR}compress.c.x {ZLIBDIR}crc32.c.x ¶
	{ZLIBDIR}deflate.c.x {ZLIBDIR}infback.c.x ¶
	{ZLIBDIR}inffast.c.x {ZLIBDIR}inflate.c.x {ZLIBDIR}inftrees.c.x ¶
	{ZLIBDIR}trees.c.x {ZLIBDIR}uncompr.c.x {ZLIBDIR}zutil.c.x

OBJ = main.c.x extra.c.x channel.c.x constants.c.x descriptor.c.x ¶
	psd.c.x icc.c.x resources.c.x write.c.x writeraw.c.x writepng.c.x ¶
	unpackbits.c.x mkdir_unimpl.c.x util.c.x psd_zip.c.x pdf.c.x ¶
	getopt.c.x getopt1.c.x ¶
	{PNGOBJ} {ZLIBOBJ}

LIB = "{SharedLibraries}StdCLib" ¶
	"{SharedLibraries}MathLib" ¶
	"{PPCLibraries}StdCRuntime.o" ¶
	"{PPCLibraries}PPCCRuntime.o" ¶
	"{PPCLibraries}PPCToolLibs.o"

# "-enum int -d STDC" are needed to make zlib build happily
COptions = -enum int -i {PNGDIR},{ZLIBDIR} -w 2,35 ¶
	-d STDC -d MAC_ENV -d DEFAULT_VERBOSE=0 -d DIRSEP=¶':¶' ¶
	-d PNG_NO_SNPRINTF

.c.x Ä .c
	{PPCC} {depDir}{default}.c -o {targDir}{default}.c.x {COptions}

psdparse ÄÄ {OBJ} {LIB}
	PPCLink -o {Targ} {OBJ} {LIB} -d -t 'MPST' -c 'MPS '
