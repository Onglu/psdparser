# This file is part of "psdparse"
# Copyright (C) 2002-2010 Toby Thain, toby@telegraphics.com.au

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

# NMAKE Makefile

# Tested with Visual C++ 2008 Express Edition
#	http://www.microsoft.com/express/vc/
# and Windows SDK for Windows Server 2008
#	http://www.microsoft.com/downloads/details.aspx?FamilyId=E6E1C3DF-A74F-4207-8586-711EBE331CDC&displaylang=en

# Set environment variables as follows (fix paths for
# *your* actual installations and ignore line breaks :)
#	SET INCLUDE=H:\Microsoft Visual Studio 9.0\VC\include;
#               C:\Program Files\Microsoft SDKs\Windows\v6.1\Include
#	SET LIB=H:\Microsoft Visual Studio 9.0\Common7\IDE;
#           H:\Microsoft Visual Studio 9.0\VC\lib;
#           C:\Program Files\Microsoft SDKs\Windows\v6.1\Lib
#	SET PATH=H:\Microsoft Visual Studio 9.0\Common7\IDE;
#            H:\Microsoft Visual Studio 9.0\VC\bin;
#            C:\Program Files\Microsoft SDKs\Windows\v6.1\Bin

# To do a 64-bit build,
# - set LIB to {VC}\lib\amd64;Microsoft SDKs\Windows\v6.1\Lib\x64
#   and PATH to {VC}\bin\x86_amd64;{VC}\bin
#   where {VC} is the vc directory of the Visual Studio distribution.


# required libraries
	
	# zlib: http://www.zlib.net/
ZLIB = ..\zlib-1.2.5

	# libpng: http://www.libpng.org/pub/png/libpng.html
LIBPNG = ..\libpng-1.4.3

	# libiconv win32 binaries
	# e.g. http://mirror.csclub.uwaterloo.ca/gnu/libiconv/libiconv-1.9.1.bin.woe32.zip
	# unzip into a new directory: libiconv-1.9.1
	### This implies a runtime dependency on libiconv-1.9.1/bin/iconv.dll
	### Move DLL into same directory as PSDPARSE.EXE or (if using Linux/WINE) hard link it
LIBICONV = libiconv-1.9.1

	# gettext sources
	# e.g. http://mirror.csclub.uwaterloo.ca/gnu/gettext/gettext-0.17.tar.gz
GETTEXT = gettext-0.18.1.1

CFLAGS = /O2 \
         -I$(ZLIB) -I$(LIBPNG) -Imsinttypes -I$(GETTEXT)\gettext-tools\gnulib-lib \
         -DDEFAULT_VERBOSE=0 -DPSBSUPPORT \
         -DDIRSEP='\\' \
         -DHAVE_ICONV_H -I$(LIBICONV)\include

ZLIBOBJ = $(ZLIB)\adler32.obj $(ZLIB)\deflate.obj $(ZLIB)\inftrees.obj \
          $(ZLIB)\uncompr.obj $(ZLIB)\compress.obj $(ZLIB)\zutil.obj \
          $(ZLIB)\crc32.obj $(ZLIB)\inflate.obj $(ZLIB)\inffast.obj \
          $(ZLIB)\trees.obj

PNGOBJ = $(LIBPNG)\png.obj $(LIBPNG)\pngerror.obj $(LIBPNG)\pngget.obj \
         $(LIBPNG)\pngmem.obj \
         $(LIBPNG)\pngset.obj $(LIBPNG)\pngtrans.obj $(LIBPNG)\pngwio.obj \
         $(LIBPNG)\pngwrite.obj $(LIBPNG)\pngwtran.obj $(LIBPNG)\pngwutil.obj

OBJ = main.obj writepng.obj writeraw.obj unpackbits.obj write.obj \
      resources.obj icc.obj extra.obj constants.obj util.obj descriptor.obj \
      channel.obj psd.obj scavenge.obj pdf.obj psd_zip.obj mmap_win.obj \
      packbits.obj duotone.obj rebuild.obj \
      getopt.obj getopt1.obj \
      version.res \
      $(ZLIBOBJ) $(PNGOBJ)

PSD2XCF_OBJ = psd2xcf.obj xcf.obj \
	  unpackbits.obj resources.obj icc.obj extra.obj constants.obj \
	  util.obj descriptor.obj channel.obj psd.obj pdf.obj psd_zip.obj \
      getopt.obj getopt1.obj \
      version.res

all : psdparse.exe

clean :
	-del psdparse.exe psd2xcf.exe $(OBJ) $(PSD2XCF_OBJ) version.res

psdparse.exe : $(OBJ)
	$(CC) /Fe$@ $(**F) $(LIBICONV)\lib\*

psd2xcf.exe : $(PSD2XCF_OBJ)
	$(CC) /Fe$@ $(**F) $(LIBICONV)\lib\* /MT Ws2_32.lib
