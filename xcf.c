/*
    This file is part of "psdparse"
    Copyright (C) 2004-2010 Toby Thain, toby@telegraphics.com.au

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

/* Gimp XCF writing routines
 * -------------------------
 * Based on documentation at:
 * http://svn.gnome.org/viewvc/gimp/trunk/devel-docs/xcf.txt?view=markup
 *
 * This documentation, in contrast to any PSD "specification" from Adobe,
 * is excellent. It allowed me to write and debug XCF writing in just
 * two nights of work (compared to several hundred hours to get a working
 * PSD parser).
 */

#include <stdio.h>
#include <string.h>
#ifndef _WIN32
	#include <arpa/inet.h>
#else
	#include <winsock2.h>
#endif

#include "xcf.h"

static int xcf_mode;
static int xcf_img_chans;

FILE *xcf_open(char *psd_name, struct psd_header *h){
	char *ext, fname[PATH_MAX];
	const char *xcf_ext = ".xcf";
	FILE *xcf;

	switch(h->mode){
	case ModeGrayScale:    xcf_mode = 1; break;
	case ModeIndexedColor: xcf_mode = 2; break;
	case ModeRGBColor:     xcf_mode = 0; break;
	default: fatal("can only convert grey scale, indexed, and RGB mode images\n");
	}
	xcf_img_chans = mode_channel_count[h->mode];

	strcpy(fname, psd_name);
	if( (ext = strrchr(fname, '.')) ) // FIXME: won't work correctly if '.' is in directory names and not filename
		strcpy(ext, xcf_ext);
	else
		strcat(fname, xcf_ext);

	if( (xcf = fopen(fname, "wb")) ){
		fputs("gimp xcf ", xcf); // File type magic
		fputs("v001", xcf);      // version
		fputc(0, xcf);           // Zero-terminator for version tag
		put4xcf(xcf, h->cols);   // Width of canvas
		put4xcf(xcf, h->rows);   // Height of canvas
		put4xcf(xcf, xcf_mode);
	}

	return xcf;
}

// write 32 bit integer in network byte order per xcf convention
// return 1 if succeeded, or zero if an error occurred
size_t put4xcf(FILE *f, uint32_t v){
	uint32_t nl = htonl(v);
	return fwrite(&nl, sizeof(nl), 1, f);
}

// write float in network byte order
size_t putfxcf(FILE *f, float v){
	union {
		uint32_t i;
		float f;
	} u;

	u.f = v;
	return put4xcf(f, u.i);
}

// write C string per xcf convention
// return 1 if succeeded, or zero if error
size_t putsxcf(FILE *f, char *s){
	size_t len = strlen(s);
	return put4xcf(f, len+1) && fwrite(s, 1, len+1, f);
}

/*
316	PROP_COLORMAP (essential)
317	  uint32  1     The type number for PROP_COLORMAP is 1
318	  uint32  3n+4  The payload length
319	  uint32  n     The number of colors in the colormap (should be <256)
320	  ,------------ Repeat n times:
321	  | byte  r     The red component of a colormap color
322	  | byte  g     The green component of a colormap color
323	  | byte  b     The blue component of a colormap color
324	  `--
 */

#define CTABSIZE 0x300

// FIXME: add proper return results
void xcf_prop_colormap(FILE *xcf, FILE *psd, struct psd_header *h){
	size_t len;
	int i, entries = CTABSIZE/3;
	unsigned char ctab[CTABSIZE];

	fseeko(psd, h->colormodepos, SEEK_SET);
	len = get4B(psd);
	if(len == CTABSIZE && fread(ctab, 1, CTABSIZE, psd) == CTABSIZE){
		put4xcf(xcf, PROP_COLORMAP);
		put4xcf(xcf, 4 + CTABSIZE);
		put4xcf(xcf, entries);
		for(i = 0; i < entries; ++i){
			fputc(ctab[i], xcf);
			fputc(ctab[i+0x100], xcf);
			fputc(ctab[i+0x200], xcf);
		}
	}else{
		fatal("indexed color mode data wrong size\n");
	}

	VERBOSE("xcf_prop_colormap, %d entries\n", entries);
}

/*
348	PROP_COMPRESSION (essential)
349	  uint32  17  The type number for PROP_COMPRESSION is 17
350	  uint32  1   One byte of payload
351	  byte    c   Compression indicator; one of
352	                0: No compression
353	                1: RLE encoding
354	                2: (Never used, but reserved for zlib compression)
355	                3: (Never used, but reserved for some fractal compression)
...
360	  Note that unlike most other properties whose payload is always a
361	  small integer, PROP_COMPRESSION does _not_ pad the value to a full
362	  32-bit integer.
 */
void xcf_prop_compression(FILE *xcf, int compr){
	put4xcf(xcf, PROP_COMPRESSION);
	put4xcf(xcf, 1);
	fputc(compr, xcf);
	VERBOSE("xcf_prop_compression: %d\n", compr);
}

/*
383	PROP_RESOLUTION (not editing state, but not _really_ essential either)
384	  uint32  19  The type number for PROP_RESOLUTION is 19
385	  uint32  8   Eight bytes of payload
386	  float   x   Horizontal resolution in pixels per 25.4 mm
387	  float   y   Vertical resolution in pixels per 25.4 mm
 */
void xcf_prop_resolution(FILE *xcf, float x_per_cm, float y_per_cm){
	put4xcf(xcf, PROP_RESOLUTION);
	put4xcf(xcf, 8);
	putfxcf(xcf, x_per_cm);
	putfxcf(xcf, y_per_cm);
	VERBOSE("xcf_prop_resolution: %g/cm x %g/cm\n", x_per_cm, y_per_cm);
}

/*
586	PROP_MODE (essential)
587	  uint32  7   The type number for PROP_MODE is 7
588	  uint32  4   Four bytes of payload
589	  unit32  m   The layer mode; one of
 */
void xcf_prop_mode(FILE *xcf, int m){
	put4xcf(xcf, PROP_MODE);
	put4xcf(xcf, 4);
	put4xcf(xcf, m);
	VERBOSE("xcf_prop_mode: %d\n", m);
}

/*
669	PROP_OFFSETS (essential)
670	  uint32  15  The type number for PROP_OFFSETS is 15
671	  uint32  8   Eight bytes of payload
672	  int32   dx  Horizontal offset
673	  int32   dy  Vertical offset
674
675	  Gives the coordinates of the topmost leftmost corner of the layer
676	  relative to the topmost leftmost corner of the canvas. The
677	  coordinates can be negative; this corresponds to a layer that
678	  extends to the left of or above the canvas boundary.
 */
void xcf_prop_offsets(FILE *xcf, int dx, int dy){
	put4xcf(xcf, PROP_OFFSETS);
	put4xcf(xcf, 8);
	put4xcf(xcf, dx);
	put4xcf(xcf, dy);
	VERBOSE("xcf_prop_offsets: (%d,%d)\n", dx, dy);
}

/*
922	PROP_OPACITY (essential)
923	  uint32  6   The type number for PROP_OPACITY is 6
924	  uint32  4   Four bytes of payload
925	  uint32  x   The opacity on a scale from 0 (fully transparent) to 255
926	              (fully opaque)
 */
void xcf_prop_opacity(FILE *xcf, int x){
	put4xcf(xcf, PROP_OPACITY);
	put4xcf(xcf, 4);
	put4xcf(xcf, x);
	VERBOSE("xcf_prop_opacity: %d (%.1f%%)\n", x, x/2.55);
}

/*
938	PROP_VISIBLE (essential)
939	  uint32  8   The type number for PROP_VISIBLE is 8
940	  uint32  4   Four bytes of payload
941	  uint32  b   1 if the layer/channel is visible; 0 if not
 */
void xcf_prop_visible(FILE *xcf, int b){
	put4xcf(xcf, PROP_VISIBLE);
	put4xcf(xcf, 4);
	put4xcf(xcf, b);
	VERBOSE("xcf_prop_visible: %d\n", b);
}

/*
624	PROP_PRESERVE_TRANSPARENCY (editing state)
625	(called PROP_LOCK_ALPHA in Gimp 3.3+)
626	  uint32  10  The type number for PROP_PRESERVE_TRANSPARENCY is 10
627	  uint32  4   Four bytes of payload
628	  uint32  b   1 if the Preserve Transparency flag is set; 0 if not
629
630	  The Preserve Transparency flag prevents all drawing tools in the
631	  Gimp from increasing the alpha of any pixel in the layer.
 */
void xcf_prop_preserve_transp(FILE *xcf, int b){
	put4xcf(xcf, PROP_PRESERVE_TRANSPARENCY);
	put4xcf(xcf, 4);
	put4xcf(xcf, b);
	VERBOSE("xcf_prop_preserve_transp: %d\n", b);
}

/*
915	PROP_END
916	  uint32  0   The type number for PROP_END is 0
917	  uint32  0   PROP_END has no payload
918
919	  This pseudo-property marks the end of a property list in the XCF
920	  file.
 */
void xcf_prop_end(FILE *xcf){
	put4xcf(xcf, PROP_END);
	put4xcf(xcf, 0);
}

/*
866	In the Run-Length Encoded format, each tile consists of a run-length
867	encoded stream of the first byte of each pixel, then a stream of the
868	second byte of each pixel, and so forth. In each of the streams,
869	multiple occurrences of the same byte value are represented in
870	compressed form. The representation of a stream is a series of
871	operations; the first byte of each operation determines the format and
872	meaning of the operation:
873
874	  byte          n     For 0 <= n <= 126: a short run of identical bytes
875	  byte          v     Repeat this value n+1 times
876	or
877	  byte          127   A long run of identical bytes
878	  byte          p
879	  byte          q
880	  byte          v     Repeat this value p*256 + q times
881	or
882	  byte          128   A long run of different bytes
883	  byte          p
884	  byte          q
885	  byte[p*256+q] data  Copy these verbatim to the output stream
886	or
887	  byte          n     For 129 <= n <= 255: a short run of different bytes
888	  byte[256-n]   data  Copy these verbatim to the output stream
889
890	The end of the stream for "the first byte of all pixels" (and the
891	following similar streams) must occur at the end of one of these
892	operations; it is not permitted to have one operation span the
893	boundary between streams.
...
902	A simple way for an XCF creator to avoid overflow is
903	 a) never using opcode 0 (but instead opcode 255)
904	 b) using opcodes 127 and 128 only for lengths larger than 127
905	 c) never emitting two "different bytes" opcodes next to each other
906	    in the encoding of a single stream.
 */

// based on http://telegraphics.com.au/svn/macpaintformat/trunk/packbits.c
size_t xcf_rle(FILE *xcf, unsigned char *src, size_t n){
	unsigned char *p, *run, *end;
	int count, maxrun, out;

	end = src + n;
	for(run = src, out = 0; n > 0; run = p, n -= count){
		// longest run possible from here
		maxrun = n < 0xffff ? n : 0xffff;

		for(p = run; p < (run+maxrun) && *p == run[0];)
			++p;
		count = p-run;

		// only a run of 3 equal bytes is worth compressing
		if(count >= 3){
			if(count <= 127){
				fputc(count-1, xcf);
				++out;
			}
			else{
				fputc(127, xcf);
				fputc(count >> 8, xcf);
				fputc(count, xcf);
				out += 3;
			}
			fputc(run[0], xcf);
			++out;
		}else{
			// three equal bytes marks the end of a 'verbatim' run,
			// because then we must switch to a compressed run
			for(p = run; p < (run+maxrun); ++p)
				if(p <= (end-3) && p[1] == p[0] && p[2] == p[0])
					break;

			count = p-run;
			if(count > 127){
				fputc(128, xcf);
				fputc(count >> 8, xcf);
				fputc(count, xcf);
				out += 3;
			}
			else{
				fputc(256-count, xcf);
				++out;
			}
			fwrite(run, 1, count, xcf);
			out += count;
		}
	}
	return out;
}

/*
797	  uint32   width  The width of the pixel array
798	  uint32   height The height of the pixel array
799	  ,-------------- Repeat for each of the ceil(width/64)*ceil(height/64) tiles
800	  | uint32 tptr   Pointer to tile data
801	  `--
802	  uint32   0      A zero marks the end of the array of tile pointers
 */

#define XCF_TILE 64

off_t xcf_level(FILE *xcf, FILE *psd, int w, int h,
				int channel_cnt, struct channel_info *xcf_chan[], int compr)
{
	unsigned char *chan_data[4], *rlebuf, *tilebuf, *dst, *src;
	int i, j, ch, xtile, ytile, tilew, tileh, ntiles = 0, tile_idx;
	off_t lptr, *tile_pos = NULL;

	if(xcf_chan){
		// break data into tiles up to 64x64 wide

		ntiles = ((w+XCF_TILE-1)/XCF_TILE) * ((h+XCF_TILE-1)/XCF_TILE);
		tile_pos = checkmalloc(sizeof(off_t)*ntiles);

		// need space for 64 rows of each mapped channel
		for(ch = 0; ch < 4; ++ch)
			if(xcf_chan[ch])
				chan_data[ch] = checkmalloc(XCF_TILE*w);

		rlebuf = checkmalloc(2*w);
		tilebuf = checkmalloc(XCF_TILE*XCF_TILE);

		// Break image into tiles, top-to-bottom, left-to-right,
		// where each tile is no larger than XCF_TILE.

		tile_idx = 0;
		for(ytile = 0; ytile < h; ytile += XCF_TILE){
			tileh = (h - ytile) > XCF_TILE ? XCF_TILE : h - ytile;

			// read the next 64 row strip from each channel
			for(ch = 0; ch < channel_cnt; ++ch){
				if(xcf_chan[ch]){
					for(i = 0; i < tileh; ++i){
						readunpackrow(psd,          // input file
									  xcf_chan[ch], // pointer to channel information
									  ytile+i,      // row index
									  chan_data[ch] + i*w,  // destination buffer
									  rlebuf);      // temporary decompression buffer
					}
				}
			}

			for(xtile = 0; xtile < w; xtile += XCF_TILE){
				tilew = (w - xtile) > XCF_TILE ? XCF_TILE : w - xtile;

				tile_pos[tile_idx++] = ftello(xcf);

				// Tiles may be RLE compressed, or uncompressed.
				if(compr){
					for(ch = 0; ch < channel_cnt; ++ch)
						if(xcf_chan[ch]){
							// Tile data is concatenation of channels (planar),
							// but each channel is a separate RLE "stream".
							for(j = 0, dst = tilebuf, src = chan_data[ch] + xtile;
								j < tileh;
								++j, dst += tilew, src += w)
							{
								memcpy(dst, src, tilew);
							}
							xcf_rle(xcf, tilebuf, tileh*tilew);
						}
				}
				else{
					// raw data, without compression (channels are interleaved)
					for(j = 0; j < tileh; ++j)
						for(i = 0; i < tilew; ++i)
							for(ch = 0; ch < channel_cnt; ++ch)
								if(xcf_chan[ch])
									fputc(chan_data[ch][j*w + xtile + i], xcf);
				}

			}
		}

		for(ch = 0; ch < 4; ++ch)
			if(xcf_chan[ch])
				free(chan_data[ch]);
		free(tilebuf);
		free(rlebuf);
	}

	lptr = ftello(xcf);

	VERBOSE("xcf_level @ %ld w:%4d h:%4d channels:%d compr:%d\n",
			(long)lptr, w, h, channel_cnt, compr);

	put4xcf(xcf, w);
	put4xcf(xcf, h);
	for(i = 0; i < ntiles; ++i){
		put4xcf(xcf, tile_pos[i]);
		//VERBOSE("  xcf_tile @ %ld\n", (long)tile_pos[i]);
	}
	put4xcf(xcf, 0);

	if(tile_pos)
		free(tile_pos);
	return lptr;
}

/*
757	A hierarchy contains data for a rectangular array of pixels. It is
758	pointed to from a layer or channel structure.
759
760	  uint32   width   The with of the pixel array
761	  uint32   height  The height of the pixel array
762	  uint32   bpp     The number of bytes per pixel given
763	  uint32   lptr    Pointer to the "level" structure
764	  ,--------------- Repeat zero or more times
765	  | uint32 dlevel  Pointer to an unused level structure
766	  `--
767	  uint32   0       A zero ends the list of level pointers
 */

off_t xcf_hierarchy(FILE *xcf, FILE *psd, int w, int h,
					int channel_cnt, struct channel_info *chan[], int compr){
	int n_levels, j, hh, ww;
	off_t hptr, level_ptrs[32];

	// first level is the image data, in tiles
	level_ptrs[0] = xcf_level(xcf, psd, w, h, channel_cnt, chan, compr);

	// remaining levels are dummies, each half the dimension of the preceding
	for(n_levels = 1, hh = h, ww = w; hh >= 64 || ww >= 64; ++n_levels)
		level_ptrs[n_levels] = xcf_level(xcf, NULL, ww /= 2, hh /= 2, 0, NULL, compr);

	hptr = ftello(xcf);
	VERBOSE("xcf_hierarchy @ %ld w:%d h:%d channels:%d\n", (long)hptr, w, h, channel_cnt);
	put4xcf(xcf, w);
	put4xcf(xcf, h);
	put4xcf(xcf, channel_cnt);
	for(j = 0; j < n_levels; ++j){
		//VERBOSE("  level @ %ld\n", (long)level_ptrs[j]);
		put4xcf(xcf, level_ptrs[j]);
	}
	put4xcf(xcf, 0);

	return hptr;
}

/*
699	Channel structures are pointed to from layer structures (in case of
700	layer masks) or from the master image structure (for all other
701	channels).
702
703	  uint32  width  The width of the channel
704	  uint32  height The height of the channel
705	  string  name   The name of the channel
706	  property-list  Layer properties (see below)
707	  uint32  hptr   Pointer to the hierarchy structure containing the pixels
 */

off_t xcf_channel(FILE *xcf, FILE *psd, int w, int h, char *name, int visible,
				  struct channel_info *chan, int compr){
	off_t hptr = xcf_hierarchy(xcf, psd, w, h, 1, &chan, compr), chptr;

	chptr = ftello(xcf);
	VERBOSE("xcf_channel @ %ld w:%d h:%d visible:%d \"%s\"\n",
			(long)chptr, w, h, visible, name);
	put4xcf(xcf, w);
	put4xcf(xcf, h);
	putsxcf(xcf, name);

	// properties...
	xcf_prop_visible(xcf, visible);
	xcf_prop_end(xcf);

	put4xcf(xcf, hptr);

	return chptr;
}

/*
524	  uint32  width  The width of the layer
525	  uint32  height The height of the layer
526	  uint32  type   Color mode of the layer: one of
527	                   0: RGB color without alpha; 3 bytes per pixel
528	                   1: RGB color with alpha;    4 bytes per pixel
529	                   2: Grayscale without alpha; 1 byte per pixel
530	                   3: Grayscale with alpha;    2 bytes per pixel
531	                   4: Indexed without alpha;   1 byte per pixel
532	                   5: Indexed with alpha;      2 bytes per pixel
533	                 (enum GimpImageType in libgimpbase/gimpbseenums.h)
534	  string  name   The name of the layer
535	  property-list  Layer properties (details below)
536	  uint32  hptr   Pointer to the hierarchy structure containing the pixels
537	  uint32  mptr   Pointer to the layer mask (a channel structure), or 0
*/

// maps xcf mode indexes to Photoshop blend mode keys;
// not all Gimp modes have a mapping.
static const char *xcf_modes[] = {
	"norm", //"NORMAL"
	"diss", //"DISSOLVE"
	"",     // Behind
	"mul ", //"MULTIPLY"
	"scrn", //"SCREEN"
	"over", //"OVERLAY"
	"diff", //"DIFFERENCE"
	"",     // Addition
	"fsub", // Subtract
	"dark", //"DARKEN"
	"lite", //"LIGHTEN"
	"hue ", //"HUE"
	"sat ", //"SATURATION"
	"colr", //"COLOR"
	"",     // Value
	"fdiv", // Divide
	"div ", //"COLOR DODGE"
	"idiv", //"COLOR BURN"
	"hLit", //"HARD LIGHT"
	"sLit", //"SOFT LIGHT"
	"",     // Grain Extract
	"",     // Grain Merge
	NULL
};

off_t xcf_layer(FILE *xcf, FILE *psd, struct layer_info *li, int compr)
{
	struct channel_info *xcf_chan[4] = {NULL, NULL, NULL, NULL};
	off_t hptr, lmptr = 0, layerptr;
	int ch, m, ltype, has_alpha = 0, layer_mask_idx = -1,
		w = li->right - li->left, h = li->bottom - li->top;
	const char **p;

	// Establish the mapping to XCF channels. Do not assume any
	// order of channels. Interpretation is given by the 'id' field.
	for(ch = 0; ch < li->channels; ++ch){
		int chid = li->chan[ch].id;
		if(chid == TRANS_CHAN_ID){
			has_alpha = 1;
			xcf_chan[xcf_img_chans] = li->chan+ch; // transparency/alpha: always last in XCF
			VERBOSE(" channel %d (%2d) -> xcf channel %d (alpha)\n",
					ch, chid, xcf_img_chans);
		}
		else if(chid == LMASK_CHAN_ID){
			layer_mask_idx = ch;
		}
		else if(chid >= 0 && chid < xcf_img_chans){
			xcf_chan[chid] = li->chan+ch; // positive id: index of image channel
			VERBOSE(" channel %d (%2d) -> xcf channel %d\n", ch, chid, chid);
		}
		else{
			VERBOSE(" channel %d (%2d) skipped\n", ch, chid);
			//alwayswarn("# can't map PS channel %d (id = %d)\n", ch, chid);
		}
	}

	hptr = xcf_hierarchy(xcf, psd, w, h, xcf_img_chans + has_alpha, xcf_chan, compr);

	if(layer_mask_idx != -1){
		alwayswarn("## layer mask not yet supported\n");
		// TODO: one complication is that Photoshop layer mask dimensions
		//       are not the same as the layer's, but xcf requires same.

		//lmptr = xcf_channel(xcf, psd, li->mask.cols, li->mask.rows,
		//					"Layer mask", &li->chan[ch], compr);
	}

	layerptr = ftello(xcf);
	ltype = xcf_mode*2 + has_alpha;
	VERBOSE("xcf_layer @ %ld type:%d \"%s\"\n", (long)layerptr, ltype, li->name);
	put4xcf(xcf, w);
	put4xcf(xcf, h);
	put4xcf(xcf, ltype);
	putsxcf(xcf, li->unicode_name ? li->unicode_name : li->name);

	// properties...
	xcf_prop_offsets(xcf, li->left, li->top);
	xcf_prop_opacity(xcf, li->blend.opacity);
	xcf_prop_visible(xcf, !(li->blend.flags & 2));
	xcf_prop_preserve_transp(xcf, li->blend.flags & 1);

	// find the Gimp mode index for Photoshop's blend mode key
	for(p = xcf_modes, m = 0; *p; ++m, ++p)
		if((*p)[0] && !memcmp(li->blend.key, *p, 4))
			break;
	if(*p)
		xcf_prop_mode(xcf, m);

	xcf_prop_end(xcf);

	put4xcf(xcf, hptr);
	put4xcf(xcf, lmptr);

	return layerptr;
}
