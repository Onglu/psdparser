/*
    This file is part of "psdparse"
    Copyright (C) 2004-2011 Toby Thain, toby@telegraphics.com.au

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdarg.h>
#include <ctype.h>
#include <errno.h>

#include "psdparse.h"
#include "version.h"
#include "cwprefix.h"

#define WARNLIMIT 10

#ifdef HAVE_ICONV_H
	iconv_t ic = (iconv_t)-1;
#endif

void fatal(char *s){
	fflush(stdout);
	fputs(s, stderr);
#ifdef PSDPARSE_PLUGIN
	// FIXME: show user a message
	// caller must be prepared for this function to return, of course
	pl_fatal(s);
#else
	exit(EXIT_FAILURE);
#endif
}

int nwarns = 0;

void warn_msg(char *fmt, ...){
	char s[0x200];
	va_list v;

	if(nwarns == WARNLIMIT) fputs("#   (further warnings suppressed)\n", stderr);
	++nwarns;
	if(nwarns <= WARNLIMIT){
		va_start(v, fmt);
		vsnprintf(s, 0x200, fmt, v);
		va_end(v);
		fflush(stdout);
		fprintf(stderr, "#   warning: %s\n", s);
	}
}

void alwayswarn(char *fmt, ...){
	char s[0x200];
	va_list v;

	va_start(v, fmt);
	vsnprintf(s, 0x200, fmt, v);
	va_end(v);
	fflush(stdout);
	fputs(s, stderr);
}

void *ckmalloc(size_t n, char *file, int line){
	void *p = malloc(n);
	if(p){
		//printf("Allocated %d bytes %s %d: %p\n", n, file, line, p);
		return p;
	}
	else{
		fprintf(stderr, "can't get %ld bytes @ %s:%d\n", n, file, line);
		exit(1);
	}
	return NULL;
}

// escape XML special characters to entities
// see: http://www.w3.org/TR/xml/#sec-predefined-ent

void fputcxml(char c, FILE *f){
	// see: http://www.w3.org/TR/REC-xml/#charsets
	// http://triptico.com/docs/unicode.html
	// http://www.unicode.org/unicode/faq/utf_bom.html
	// http://asis.epfl.ch/GNU.MISC/recode-3.6/recode_5.html
	// http://www.terena.org/activities/multiling/unicode/utf16.html
	switch(c){
	case '<':  fputs("&lt;", f); break;
	case '>':  fputs("&gt;", f); break;
	case '&':  fputs("&amp;", f); break;
	case '\'': fputs("&apos;", f); break;
	case '\"': fputs("&quot;", f); break;
	case 0x09:
	case 0x0a:
	case 0x0d:
		fputc(c, f);
		break;
	default:
		if(c >= 0x20 || (c & 0x80)) // pass through multibyte UTF-8
			fputc(c, f);
		else // no other characters are valid in XML.
			alwayswarn("XML: invalid character %#x skipped\n", c & 0xff);
	}
}

void fputsxml(char *str, FILE *f){
	while(*str)
		fputcxml(*str++, f);
}

void fwritexml(char *buf, size_t count, FILE *f){
	while(count--)
		fputcxml(*buf++, f);
}

// fetch Pascal string (length byte followed by text)
// N.B. This returns a pointer to the string as a C string (no length
//      byte, and terminated by NUL).
char *getpstr(psd_file_t f){
	static char pstr[0x100];
	int len = fgetc(f);
	if(len != EOF){
		fread(pstr, 1, len, f);
		pstr[len] = 0;
	}else
		pstr[0] = 0;
	return pstr;
}

// Pascal string, padded to multiple of 2 bytes
char *getpstr2(psd_file_t f){
	static char pstr[0x100];
	int len = fgetc(f);
	if(len != EOF){
		fread(pstr, 1, len, f);
		pstr[len] = 0;
		if(!(len & 1))
			fgetc(f); // skip padding
	}else
		pstr[0] = 0;
	return pstr;
}

char *getkey(psd_file_t f){
	static char k[5];
	if(fread(k, 1, 4, f) == 4)
		k[4] = 0;
	else
		k[0] = 0; // or return NULL?
	return k;
}

static int platform_is_LittleEndian(){
	union{ int a; char b; } u;
	u.a = 1;
	return u.b;
}

double getdoubleB(psd_file_t f){
	union {
		double d;
		unsigned char c[8];
	} u, urev;

	if(fread(u.c, 1, 8, f) == 8){
		if(platform_is_LittleEndian()){
			urev.c[0] = u.c[7];
			urev.c[1] = u.c[6];
			urev.c[2] = u.c[5];
			urev.c[3] = u.c[4];
			urev.c[4] = u.c[3];
			urev.c[5] = u.c[2];
			urev.c[6] = u.c[1];
			urev.c[7] = u.c[0];
			return urev.d;
		}
		else
			return u.d;
	}
	return 0;
}

// Read a 4-byte signed binary value in BigEndian format.
// Assumes sizeof(long) == 4 (and two's complement CPU :)
int32_t get4B(psd_file_t f){
	long n = fgetc(f)<<24;
	n |= fgetc(f)<<16;
	n |= fgetc(f)<<8;
	return n | fgetc(f);
}

#ifndef __SC__ // MPW 68K compiler does not support long long
// Read a 8-byte signed binary value in BigEndian format.
// Assumes sizeof(long) == 4
int64_t get8B(psd_file_t f){
	int64_t msl = (unsigned long)get4B(f);
	return (msl << 32) | (unsigned long)get4B(f);
}
#endif

// Read a 2-byte signed binary value in BigEndian format.
int get2B(psd_file_t f){
	unsigned n = fgetc(f)<<8;
	n |= fgetc(f);
	return n < 0x8000 ? n : n - 0x10000;
}

// Read a 2-byte unsigned binary value in BigEndian format.
unsigned get2Bu(psd_file_t f){
	unsigned n = fgetc(f)<<8;
	return n |= fgetc(f);
}


unsigned put4B(psd_file_t f, int32_t value){
	return fputc(value >> 24, f) != EOF
		&& fputc(value >> 16, f) != EOF
		&& fputc(value >>  8, f) != EOF
		&& fputc(value, f) != EOF;
}

unsigned put8B(psd_file_t f, int64_t value){
	return put4B(f, value >> 32) && put4B(f, value);
}

unsigned putpsdbytes(psd_file_t f, int version, uint64_t value){
	if(version == 1 && value > UINT32_MAX)
		fatal("## Value out of range for PSD format. Try without --rebuildpsd.\n");
	return version == 1 ? put4B(f, value) : put8B(f, value);
}

unsigned put2B(psd_file_t f, int value){
	return fputc(value >> 8, f) != EOF
		&& fputc(value, f) != EOF;
}


// memory-mapped analogues

// Read a 4-byte signed binary value in BigEndian format.
// Assumes sizeof(long) == 4 (and two's complement CPU :)
int32_t peek4B(unsigned char *p){
	return (p[0]<<24) | (p[1]<<16) | (p[2]<<8) | p[3];
}

// Read a 8-byte signed binary value in BigEndian format.
// Assumes sizeof(long) == 4
int64_t peek8B(unsigned char *p){
	int64_t msl = (unsigned long)peek4B(p);
	return (msl << 32) | (unsigned long)peek4B(p+4);
}

// Read a 2-byte signed binary value in BigEndian format.
// Meant to work even where sizeof(short) > 2
int peek2B(unsigned char *p){
	unsigned n = (p[0]<<8) | p[1];
	return n < 0x8000 ? n : n - 0x10000;
}

// Read a 2-byte unsigned binary value in BigEndian format.
unsigned peek2Bu(unsigned char *p){
	return (p[0]<<8) | p[1];
}

// return pointer to a string of n tabs
const char *tabs(int n){
	static const char forty[] = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";
	return forty + (sizeof(forty) - 1 - n);
}

int hexdigit(unsigned char c){
	c = toupper(c);
	return c - (c >= 'A' ? 'A'-10 : '0');
}

char indir[PATH_MAX];
FILE *rebuilt_psd;

void openfiles(char *psdpath, struct psd_header *h)
{
	char *ext, fname[PATH_MAX], *dirsuffix;

	strcpy(indir, psdpath);
	dirsuffix = h->depth < 32 ? "_png" : "_raw";
	if( (ext = strrchr(indir, '.')) )
		strcpy(ext, dirsuffix);
	else
		strcat(indir, dirsuffix);

	if(writelist){
		setupfile(fname, pngdir, "list", ".txt");
		listfile = fopen(fname, "w");
	}else{
		listfile = NULL;
	}

	if(rebuild){
		char *basename = strrchr(psdpath, DIRSEP);
		setupfile(fname, pngdir, basename ? basename : psdpath, "-rebuilt.psd");
		rebuilt_psd = fopen(fname, "w");
	}else{
		rebuilt_psd = NULL;
	}

	// see: http://hsivonen.iki.fi/producing-xml/
	if(xmlout){
		quiet = writexml = 1;
		verbose = 0;
		xml = stdout;
	}else if(writexml){
		setupfile(fname, pngdir, "psd", ".xml");
		xml = fopen(fname, "w");
	}else{
		xml = NULL;
	}
#ifdef HAVE_ICONV_H
	ic = iconv_open("UTF-8", "UTF-16BE");
	if(ic == (iconv_t)-1)
		alwayswarn("iconv_open(): failed, errno = %d\n", errno);
#endif
	if(xml){
		fputs("<?xml version='1.0' encoding='UTF-8'?>\n", xml);
		fputs("<!-- generated by psdparse version " VERSION_STR " -->\n", xml);
	}
}

// construct the destination filename, and create enclosing directories
// as needed (and if requested).

void setupfile(char *dstname, char *dir, char *name, char *suffix){
	char *last, d[PATH_MAX], c;

#ifdef MKDIR
	MKDIR(dir, 0755);

	if(strchr(name, DIRSEP)){
		if(!makedirs)
			alwayswarn("# warning: replaced %c's in filename (use --makedirs if you want subdirectories)\n", DIRSEP);
		for(last = name; (last = strchr(last+1, DIRSEP)); )
			if(makedirs){
				last[0] = 0;
				strcpy(d, dir);
				strcat(d, dirsep);

				// never create hidden directories
				if(*name == '.'){
					strcat(d, "_");
					strcat(d, name+1);
				}else{
					strcat(d, name);
				}

				if(!MKDIR(d, 0755))
					VERBOSE("# made subdirectory \"%s\"\n", d);
				last[0] = DIRSEP;
			}else
				last[0] = '_';
	}

	strcpy(dstname, dir);
	strcat(dstname, dirsep);
#else
	strcpy(dstname, dir);
	strcat(dstname, "_");
#endif

	// never create hidden files
	if(*name == '.'){
		strcat(dstname, "_");
		strcat(dstname, name+1);
	}else{
		strcat(dstname, name);
	}

	strcat(dstname, suffix);
}
