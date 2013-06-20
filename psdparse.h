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

#ifndef _PSDPARSE_H_
#define _PSDPARSE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>

#if defined(__SC__) || defined(MPW_C)
	// MPW 68K or PPC
	#include <MacTypes.h>
	#include "getopt.h"

	typedef SInt64 int64_t;
	typedef UInt16 uint16_t;
	typedef UInt8  uint8_t;

	#define vsnprintf(s,n,f,ap) vsprintf(s,f,ap)
	#define strdup(s) strcpy(checkmalloc(strlen(s)+1),(s))

	int mkdir(char *s, int mode);
#else
	#include <stdint.h>
	#include <sys/types.h>
	#include <wchar.h>
	#include <ctype.h>
	#include <errno.h>
	#include <sys/stat.h>
#endif

#ifdef _WIN32
	#include <direct.h>
	#include "getopt.h"

	#define MKDIR(name,mode) _mkdir(name) // laughable, isn't it.
#else
	#include <getopt.h>

	#define MKDIR mkdir
#endif

#if defined(HAVE_SYS_MMAN_H) || defined(_WIN32)
	#define CAN_MMAP
#endif

#ifdef HAVE_UNISTD_H
	#include <unistd.h>
#endif

#ifdef HAVE_ICONV_H
	#include <iconv.h>

	extern iconv_t ic;
#endif

#ifdef PSBSUPPORT
	#define _FILE_OFFSET_BITS 64
	// #define _LARGEFILE_SOURCE // defined in Makefile

	typedef uint64_t psd_bytes_t;
	#define GETPSDBYTES(f) (h->version==1 ? get4B(f) : get8B(f))

	// macro chooses the '%ll' version of format strings involving psd_bytes_t type
	#define LL_L(llfmt,lfmt) llfmt
#else
	typedef uint32_t psd_bytes_t;
	#define GETPSDBYTES get4B

	// macro chooses the '%l' version of format strings involving psd_bytes_t type
	#define LL_L(llfmt,lfmt) lfmt
#endif

// macro for size of the PSD/PSB variant fields, given PSD version
// this is for common case where PSD(v1) uses 4 byte field & PSB(v2) uses 8 byte field.
// N.B. exception: RLE row count array, where counts are 2 bytes & 4 bytes respectively
#define PSDBSIZE(version) ((version) << 2)

// this is paired with %u format specifications so may break
// if not same size as 'unsigned'.
typedef uint32_t psd_pixels_t;

// libpsd types (used only by psd_zip.c)
typedef int psd_status, psd_int, psd_bool;
typedef uint8_t psd_uchar;
typedef uint16_t psd_ushort;

#ifdef PSDPARSE_PLUGIN
	#include "world.h" // for DIRSEP
	#include "file_compat.h"

	typedef FILEREF psd_file_t; // appropriate file handle type for platform

	#define fgetc pl_fgetc
	#define fread pl_fread
	#define fseeko pl_fseeko
	#define ftello pl_ftello
	#undef feof // it's a macro in Metrowerks stdio
	#define feof pl_feof

	int pl_fgetc(psd_file_t f);
	int pl_feof(psd_file_t f);
	size_t pl_fread(void *ptr, size_t s, size_t n, psd_file_t f);
	int pl_fseeko(psd_file_t f, off_t pos, int wh);
	off_t pl_ftello(psd_file_t f);
	void pl_fatal(char *s);
	void *pl_malloc(size_t n, char *file, int line);
	void pl_free(void *p, char *file, int line);

	Boolean warndialog(char *s);

	#define checkmalloc(N) pl_malloc(N, __FILE__, __LINE__)
	#define free(P) pl_free(P, __FILE__, __LINE__)
#else
	#ifdef _WIN32
		#include <direct.h>

		//#define MKDIR(name,mode) _mkdir(name) // laughable, isn't it.

		#define fseeko _fseeki64
		#define ftello _ftelli64
	#else
		#if defined(macintosh) && !defined(HAVE_SYS_STAT_H)
			// don't clash with OS X header -- this prototype is meant for MPW build.
			int mkdir(char *s, int mode);
		#endif
		#define MKDIR mkdir
	#endif

	typedef FILE *psd_file_t;
	#ifdef _MSC_VER
		#define fseeko _fseeki64
		#define ftello _ftelli64
	#else
		#if defined(__SC__) || defined(MPW_C) || defined(_WIN32)
			#define fseeko fseek
			#define ftello ftell
		#endif
	#endif

	#define checkmalloc(N) ckmalloc(N, __FILE__, __LINE__)
#endif

#ifndef PATH_MAX
	#define PATH_MAX FILENAME_MAX
#endif

enum{RAWDATA,RLECOMP,ZIPNOPREDICT,ZIPPREDICT}; // ZIP types from CS doc

/* Photoshop's mode constants */
#define SCAVENGE_MODE -1
#define ModeBitmap		 0
#define ModeGrayScale		 1
#define ModeIndexedColor 2
#define ModeRGBColor		 3
#define ModeCMYKColor		 4
#define ModeHSLColor		 5
#define ModeHSBColor		 6
#define ModeMultichannel 7
#define ModeDuotone		 8
#define ModeLabColor		 9
#define ModeGray16		10
#define ModeRGB48			11
#define ModeLab48			12
#define ModeCMYK64		13
#define ModeDeepMultichannel 14
#define ModeDuotone16		15

#define PAD2(x) (((x)+1) & -2) // same or next even
#define PAD4(x) (((x)+3) & -4) // same or next multiple of 4
#define PAD_BYTE 0

#define VERBOSE if(verbose) printf
#define UNQUIET if(!quiet) printf

#define FIXEDPT(x) ((x)/65536.)

#define KEYMATCH(p, str) (!memcmp(p, str, strlen(str)))

#define TRANS_CHAN_ID (-1)
#define LMASK_CHAN_ID (-2)
#define UMASK_CHAN_ID (-3)

struct psd_header{
	char sig[4];
	unsigned short version;
	char reserved[6];
	unsigned short channels;
	uint32_t rows;
	uint32_t cols;
	unsigned short depth;
	short mode; // we use -1 as flag for 'scavenging' where actual mode is not known

	// following fields are for our purposes, not actual header fields
	psd_bytes_t colormodepos; // set by dopsd()
	psd_bytes_t resourcepos;  // set by dopsd()
	int nlayers, mergedalpha; // set by dopsd()->dolayermaskinfo()
	struct layer_info *linfo;     // layer info array, set by dopsd()->dolayermaskinfo()
	psd_bytes_t lmistart, lmilen; // layer & mask info section, set by dopsd()->dolayermaskinfo()
	psd_bytes_t layerdatapos; // set by dopsd()
	psd_bytes_t global_lmi_pos, global_lmi_len;
	struct channel_info *merged_chans; // set by doimage()
};

struct layer_mask_info{
	psd_bytes_t size; // 36, 20, or zero

	// if size == 20:
	int32_t top;
	int32_t left;
	int32_t bottom;
	int32_t right;
	char default_colour;
	char flags;

	// if size == 36:
	char real_flags;
	char real_default_colour;
	int32_t real_top;
	int32_t real_left;
	int32_t real_bottom;
	int32_t real_right;
};

struct blend_mode_info{
	char sig[4];
	char key[4];
	unsigned char opacity;
	unsigned char clipping;
	unsigned char flags;
	//unsigned char filler;
};

struct channel_info{
	int id;                   // channel id
	int comptype;             // channel's compression type
	psd_pixels_t rows, cols, rowbytes;  // set by dochannel()
	psd_bytes_t length;       // channel byte count in file

	// used in rebuild
	psd_bytes_t length_rebuild; // channel byte count in file

	// how to find image data, depending on compression type:
	psd_bytes_t rawpos;       // file offset of RAW channel data (AFTER compression type)
	psd_bytes_t *rowpos;      // row data file positions (RLE ONLY)
	unsigned char *unzipdata; // uncompressed data (ZIP ONLY)
};

struct layer_info{
	int32_t top;
	int32_t left;
	int32_t bottom;
	int32_t right;
	short channels;

	// runtime data (not in file)
	struct channel_info *chan;
	int *chindex;    // lookup channel number by id (inverse of chid[])
	struct blend_mode_info blend;
	struct layer_mask_info mask;
	char *name;
	char *unicode_name; // utf8. will be NULL unless unicode_filenames or extra flags set
	char *nameno; // "layerNN"
	psd_bytes_t additionalpos;
	psd_bytes_t additionallen;

	psd_bytes_t filepos; // only used in scavenge layers mode
	psd_bytes_t chpos; // only used in scavenge channels mode
	psd_bytes_t xcf_pos; // only used by psd2xcf tool
};

struct dictentry{
	int id;
	char *key, *tag, *desc;
	void (*func)(psd_file_t f, int level, int len, struct dictentry *dict);
};

struct colour_space{
	const unsigned short id; // colour space ID as defined in, e.g., DisplayInfo structure
	const char *name,        // colour space name, used for element name in XML
			   *components;  // points to a string of initials of each colour component, in order
							 // or "*" if the colour data may be a readable string (e.g. Pantone)
};

// constants.c:
extern const char *channelsuffixes[], *mode_names[];
extern struct colour_space colour_spaces[];
extern const int mode_channel_count[];
extern const int mode_colour_space[];

extern char dirsep[], *pngdir;
extern int verbose, quiet, rsrc, print_rsrc, resdump, extra, makedirs,
	numbered, help, split, nwarns, writepng, writelist,
	writexml, xmlout, unicode_filenames,
	rebuild, rebuild_v1, merged_only;
extern FILE *xml, *listfile, *rebuilt_psd;

void fatal(char *s);
void warn_msg(char *fmt, ...);
void alwayswarn(char *fmt, ...);

void *ckmalloc(size_t n, char *file, int line);

void fputcxml(char c, FILE *f);
void fputsxml(char *str, FILE *f);
void fwritexml(char *buf, size_t count, FILE *f);

char *getpstr(psd_file_t f);
char *getpstr2(psd_file_t f);
char *getkey(psd_file_t f);
double getdoubleB(psd_file_t f);
int32_t get4B(psd_file_t f);
int64_t get8B(psd_file_t f);
int get2B(psd_file_t f);
unsigned get2Bu(psd_file_t f);

unsigned put4B(psd_file_t f, int32_t);
unsigned put8B(psd_file_t f, int64_t);
unsigned putpsdbytes(psd_file_t f, int version, uint64_t value);
unsigned put2B(psd_file_t f, int);

int32_t peek4B(unsigned char *p);
int64_t peek8B(unsigned char *p);
int peek2B(unsigned char *p);
unsigned peek2Bu(unsigned char *p);

const char *tabs(int n);
int hexdigit(unsigned char c);
void openfiles(char *psdpath, struct psd_header *h);

int dopsd(psd_file_t f, char *fname, struct psd_header *h);
void processlayers(psd_file_t f, struct psd_header *h);
void dolayerinfo(psd_file_t f, struct psd_header *h);

void entertag(psd_file_t f, int level, int len, struct dictentry *parent, struct dictentry *d, int resetpos);
struct dictentry *findbykey(psd_file_t f, int level, struct dictentry *dict, char *key, int len, int resetpos);
void doadditional(psd_file_t f, struct psd_header *h, int level, psd_bytes_t length);
void layerblendmode(psd_file_t f, int level, int len, struct blend_mode_info *bm);
void colorspace(int level, int space, unsigned char data[]);
void ed_colorspace(psd_file_t f, int level, int len, struct dictentry *parent);

char *conv_unicodestr(psd_file_t f, long count);
void xml_unicodestr(psd_file_t f, long count);
void descriptor(psd_file_t f, int level, int len, struct dictentry *dict);

void ed_versdesc(psd_file_t f, int level, int len, struct dictentry *parent);
struct colour_space *find_colour_space(int space);

void ir_raw(psd_file_t f, int level, int len, struct dictentry *parent);

void ir_icc34profile(psd_file_t f, int level, int len, struct dictentry *parent);
double s15fixed16(psd_file_t f);

void skipblock(psd_file_t f,char *desc);
void dumprow(unsigned char *b,long n,int group);
void readunpackrow(psd_file_t psd,        // input file handle
				   struct channel_info *chan, // channel info
				   psd_pixels_t row,      // row index
				   unsigned char *inrow,  // dest buffer for the uncompressed row (rb bytes)
				   unsigned char *outrow); // temporary buffer for compressed data
void dochannel(psd_file_t f,
		  struct layer_info *li,
		  struct channel_info *chan, // array of channel info
		  int channels, // how many channels are to be processed (>1 only for merged data)
		  struct psd_header *h);
void doimage(psd_file_t f,struct layer_info *li,char *name,struct psd_header *h);
void readlayerinfo(psd_file_t f, struct psd_header *h, int i);
void dolayermaskinfo(psd_file_t f,struct psd_header *h);
psd_bytes_t globallayermaskinfo(psd_file_t f, struct psd_header *h);
void doimageresources(psd_file_t f);

unsigned scavenge_psd(void *addr, size_t st_size, struct psd_header *h);
void scan_channels(unsigned char *addr, size_t len, struct psd_header *h);

void setupfile(char *dstname,char *dir,char *name,char *suffix);
FILE* pngsetupwrite(psd_file_t psd, char *dir, char *name, psd_pixels_t width, psd_pixels_t height,
					int channels, int color_type, struct layer_info *li, struct psd_header *h);
void pngwriteimage(
		FILE *png,
		psd_file_t psd,
		struct layer_info *li,
		struct channel_info *chan,
		int chancount,
		struct psd_header *h);

FILE* rawsetupwrite(psd_file_t psd, char *dir, char *name, psd_pixels_t width, psd_pixels_t height,
					int channels, int color_type, struct layer_info *li, struct psd_header *h);
void rawwriteimage(
		FILE *png,
		psd_file_t psd,
		struct layer_info *li,
		struct channel_info *chan,
		int chancount,
		struct psd_header *h);

// worst case PackBits performance for n bytes:
#define PACKBITSWORST(n) (129*((n)/128) + 1 + ((n) % 128))
psd_pixels_t packbits(unsigned char *src, unsigned char *dst, psd_pixels_t n);

psd_pixels_t unpackbits(unsigned char *outp, unsigned char *inp,
						psd_pixels_t rowbytes, psd_pixels_t inlen);

void *map_file(int fd, size_t len);
void unmap_file(void *addr, size_t len);

int is_pdf_white(char c);
int is_pdf_delim(char c);
size_t pdf_string(char **p, char *outbuf, size_t n);
size_t pdf_name(char **p, char *outbuf, size_t n);

int is_photo(const char *name);

psd_status psd_unzip_without_prediction(psd_uchar *src_buf, psd_int src_len,
	psd_uchar *dst_buf, psd_int dst_len);
psd_status psd_unzip_with_prediction(psd_uchar *src_buf, psd_int src_len,
	psd_uchar *dst_buf, psd_int dst_len,
	psd_int row_size, psd_int color_depth);

void duotone_data(psd_file_t f, int level);

void rebuild_psd(psd_file_t psd, int version, struct psd_header *h);

double calculate_angle(void *li);

#endif
