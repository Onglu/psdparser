/*
    This file is part of "psdparse"
    Copyright (C) 2004-9 Toby Thain, toby@telegraphics.com.au

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

#include <stdio.h>
#include <stdlib.h>

#include "psdparse.h"
#ifndef QMAKE_4_8_x
#include "png.h"
#else
#include "../../../include/libpng/png.h"
#endif

#ifdef HAVE_ZLIB_H
#ifndef QMAKE_4_8_x
#include "zlib.h"
#else
#include "../../../include/zlib/zlib.h"
#endif
#endif

static png_structp png_ptr;
static png_infop info_ptr;

// Prepare to write the PNG file. This function:
// - creates a directory for it, if needed
// - builds the PNG file name and opens the file for writing
// - checks colour mode and sets up libpng parameters
// - fetches the colour palette for Indexed Mode images, and gives to libpng 

// Parameters:
// psd         file handle for input PSD
// dir         pointer to output dir name
// name        name for this PNG (e.g. layer name)
// width,height  image dimensions (may not be the same as PSD header dimensions)
// channels    channel count - this is purely informational
// color_type  identified PNG colour type (determined by doimage())
// li          pointer to layer info for relevant layer, or NULL if no layer (e.g. merged composite)
// h           pointer to PSD file header struct

FILE* pngsetupwrite(psd_file_t psd, char *dir, char *name, psd_pixels_t width, psd_pixels_t height, 
					int channels, int color_type, struct layer_info *li, struct psd_header *h)
{
	char pngname[PATH_MAX], *pngtype = NULL;
	static FILE *f; // static, because it might get used post-longjmp()
	png_color *pngpal;
	int i, n;
	psd_bytes_t savepos;

	f = NULL;
	
	if(width && height){
		setupfile(pngname, dir, name, ".png");

		if(channels < 1 || channels > 4){
			alwayswarn("## (BUG) bad channel count (%d), writing PNG \"%s\"\n", channels, pngname);
			if(channels > 4)
				channels = 4; // try anyway
			else
				return NULL;
		}

		switch(color_type){
		case PNG_COLOR_TYPE_GRAY:       pngtype = "GRAY"; break;
		case PNG_COLOR_TYPE_GRAY_ALPHA: pngtype = "GRAY_ALPHA"; break;
		case PNG_COLOR_TYPE_PALETTE:    pngtype = "PALETTE"; break;
		case PNG_COLOR_TYPE_RGB:        pngtype = "RGB"; break;
		case PNG_COLOR_TYPE_RGB_ALPHA:  pngtype = "RGB_ALPHA"; break;
		default:
			alwayswarn("## (BUG) bad color_type (%d), %d channels (%s), writing PNG \"%s\"\n", 
					   color_type, channels, mode_names[h->mode], pngname);
			return NULL;
		}

		if( !(png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)) ){
			alwayswarn("### pngsetupwrite: png_create_write_struct failed\n");
			return NULL;
		}

		if( (f = fopen(pngname, "wb")) ){
			if(xml){
				fputs("\t\t<PNG NAME='", xml);
				fputsxml(name, xml);
				fputs("' DIR='", xml);
				fputsxml(dir, xml);
				fputs("' FILE='", xml);
				fputsxml(pngname, xml);
				fprintf(xml, "' WIDTH='%u' HEIGHT='%u' CHANNELS='%d' COLORTYPE='%d' COLORTYPENAME='%s' DEPTH='%d'",
						width, height, channels, color_type, pngtype, h->depth);
			}
			UNQUIET("# writing PNG \"%s\"\n", pngname);
			VERBOSE("#             %3ux%3u, depth=%d, channels=%d, type=%d(%s)\n",
					width, height, h->depth, channels, color_type, pngtype);

			if( !(info_ptr = png_create_info_struct(png_ptr)) || setjmp(png_jmpbuf(png_ptr)) )
			{ /* If we get here, libpng had a problem */
				alwayswarn("### pngsetupwrite: Fatal error in libpng\n");
				fclose(f);
				png_destroy_write_struct(&png_ptr, &info_ptr);
				return NULL;
			}

			png_init_io(png_ptr, f);

			png_set_IHDR(png_ptr, info_ptr, width, height, h->depth, color_type,
						 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

			if(h->mode == ModeBitmap) 
				png_set_invert_mono(png_ptr);
			else if(h->mode == ModeIndexedColor){
				// go get the colour palette
				savepos = ftello(psd);
				fseeko(psd, h->colormodepos, SEEK_SET);
				n = get4B(psd)/3;
				if(n > 256){ // sanity check...
					warn_msg("# more than 256 entries in colour palette! (%d)\n", n);
					n = 256;
				}
				pngpal = checkmalloc(sizeof(png_color)*n);
				for(i = 0; i < n; ++i) pngpal[i].red   = fgetc(psd);
				for(i = 0; i < n; ++i) pngpal[i].green = fgetc(psd);
				for(i = 0; i < n; ++i) pngpal[i].blue  = fgetc(psd);
				fseeko(psd, savepos, SEEK_SET);
				png_set_PLTE(png_ptr, info_ptr, pngpal, n);
				free(pngpal);
			}

			png_write_info(png_ptr, info_ptr);
			
			png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);

		}else alwayswarn("### can't open \"%s\" for writing\n", pngname);

	}else VERBOSE("### won't write empty PNG, skipping\n");

	return f;
}

void pngwriteimage(
		FILE *png,
		psd_file_t psd,
		struct layer_info *li,
		struct channel_info *chan,
		int chancount,
		struct psd_header *h)
{
	psd_pixels_t i, j;
	uint16_t *q;
	unsigned char *rowbuf, *inrows[4], *rledata, *p;
	int ch, map[4];
	
	if(xml)
		fprintf(xml, " CHINDEX='%d' />\n", chan->id);

	// buffer used to construct a row interleaving all channels (if required)
	rowbuf  = checkmalloc(chan->rowbytes*chancount);

	// a buffer for RLE decompression (if required), we pass this to readunpackrow()
	rledata = checkmalloc(chan->rowbytes*2);

	// row buffers per channel, for reading non-interleaved rows
	for(ch = 0; ch < chancount; ++ch){
		inrows[ch] = checkmalloc(chan->rowbytes);
		// build mapping so that png channel 0 --> channel with id 0, etc
		// and png alpha --> channel with id -1
		map[ch] = li && chancount > 1 ? li->chindex[ch] : ch;
	}
	
	// find the alpha channel, if needed
	if(li && (chancount == 2 || chancount == 4)){
		if(li->chindex[-1] == -1)
			alwayswarn("### did not locate alpha channel??\n");
		else
			map[chancount-1] = li->chindex[-1];
	}
	
	//for( ch = 0 ; ch < chancount ; ++ch )
	//	alwayswarn("# channel map[%d] -> %d\n", ch, map[ch]);

	if( setjmp(png_jmpbuf(png_ptr)) )
	{ /* If we get here, libpng had a problem writing the file */
		alwayswarn("### pngwriteimage: Fatal error in libpng\n");
		goto err;
	}

	for(j = 0; j < chan->rows; ++j){
		for(ch = 0; ch < chancount; ++ch){
			/* get row data */
			if(map[ch] < 0 || map[ch] >= chancount){
				warn_msg("bad map[%d]=%d, skipping a channel", ch, map[ch]);
				memset(inrows[ch], 0, chan->rowbytes); // zero out the row
			}else
				readunpackrow(psd, chan + map[ch], j, inrows[ch], rledata);
		}

		if(chancount > 1){ /* interleave channels */
			if(h->depth == 8)
				for(i = 0, p = rowbuf; i < chan->rowbytes; ++i)
					for(ch = 0; ch < chancount; ++ch)
						*p++ = inrows[ch][i];
			else
				for(i = 0, q = (uint16_t*)rowbuf; i < chan->rowbytes/2; ++i)
					for(ch = 0; ch < chancount; ++ch)
						*q++ = ((uint16_t*)inrows[ch])[i];

			png_write_row(png_ptr, rowbuf);
		}else
			png_write_row(png_ptr, inrows[0]);
	}
	
	png_write_end(png_ptr, NULL /*info_ptr*/);

err:
	fclose(png);

	free(rowbuf);
	free(rledata);
	for(ch = 0; ch < chancount; ++ch)
		free(inrows[ch]);

	png_destroy_write_struct(&png_ptr, &info_ptr);
}
