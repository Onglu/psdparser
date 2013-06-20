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

#include "psdparse.h"

/* This program is an example of using psdparse as a library.
 * It accepts one filename as the first parameter, opens this PSD file,
 * and prints some information about its contents.
 *
 * build:
 *     make example -f Makefile.unix
 */

// These flags control psdparse behaviour.
// You WILL get text output unless you set quiet = 1 !
int verbose = 0, quiet = 1, rsrc = 0, print_rsrc = 0, resdump = 0, extra = 0,
	makedirs = 0, numbered = 0, help = 0, split = 0, xmlout = 0,
	writepng = 0, writelist = 0, writexml = 0, unicode_filenames = 1,
	rebuild = 0;
long hres, vres; // we don't use these, but they're set within doresources()
char *pngdir;

int main(int argc, char *argv[]){
	FILE *f;
	struct psd_header h;

	if(argc == 2 && (f = fopen(argv[1], "rb"))){
		h.version = h.nlayers = 0;
		h.layerdatapos = 0;

		if(dopsd(f, argv[1], &h)){
			/* The following members of psd_header struct h are initialised:
			 * sig, version, channels, rows, cols, depth, mode */
			printf("PS%c file, %u rows x %u cols, %u channels, %u bit depth, %u layers\n",
				   h.version == 1 ? 'D' : 'B',
				   h.rows, h.cols, h.channels, h.depth, h.nlayers);

			// process the layers in 'image data' section.
			// this will, in turn, call doimage() for each layer.
			processlayers(f, &h);

			// position file after 'layer & mask info', i.e. at the
			// beginning of the merged image data.
			fseeko(f, h.lmistart + h.lmilen, SEEK_SET);

			// process merged (composite) image data
			doimage(f, NULL, NULL, &h);

			return EXIT_SUCCESS;
		}else{
			fprintf(stderr, "Not a PSD or PSB file.\n");
		}

		fclose(f);
	}else{
		fprintf(stderr, "Could not open: %s\n", argv[1]);
	}

	return EXIT_FAILURE;
}

/* This function must be supplied as a callback. It assumes that the
 * current file position points to the beginning of the image data
 * for the layer, or merged (flattened) image data. */

void doimage(psd_file_t f, struct layer_info *li, char *name, struct psd_header *h)
{
	int ch;

	/* li points to layer information. If it is NULL, then
	 * the merged image is being being processed, not a layer. */

	/* You probably want to treat the layer/image differently
	 * according to its mode. */
	switch(h->mode){
	case ModeBitmap:
	case ModeGrayScale:
	case ModeGray16:
	case ModeDuotone:
	case ModeDuotone16:
		break;
	case ModeIndexedColor:
		break;
	case ModeRGBColor:
	case ModeRGB48:
		break;
	default: // multichannel, cmyk, lab etc
		;
	}

	if(li){
		// Process layer, described by struct layer_info pointed to by li

		// The following members of this struct may be useful:
		//   top, left, bottom, right       - position/size of layer in document
		//     (the layer may lie partly or wholly outside the document bounds,
		//      as defined by PSD header)
		//   channels                       - channel count
		//   struct channel_info *chan      - array of channel_info
		//   struct blend_mode_info blend   - blending information
		//   struct layer_mask_info mask    - layer mask info
		//   char *name                     - layer name

		printf("layer \"%s\"\n", li->name);
		for(ch = 0; ch < li->channels; ++ch){
			// dochannel() initialises the li->chan[ch] struct, including:
			//   id                    - channel id
			//   comptype              - channel's compression type
			//   rows, cols, rowbytes  - set by dochannel()
			//   length                - channel byte count in file
			// how to find image data, depending on compression type:
			//   rawpos                - file offset of RAW channel data (AFTER compression type)
			//   rowpos                - row data file positions (RLE ONLY)
			//   unzipdata             - uncompressed data (ZIP ONLY)

			dochannel(f, li, li->chan + ch, 1, h);
			printf("  channel %d  id=%2d  %4u rows x %4u cols  %6ld bytes\n",
				   ch, li->chan[ch].id, li->chan[ch].rows, li->chan[ch].cols,
				   (long)li->chan[ch].length);
		}
	}else{
		// The merged image has the size, mode, depth, and channel count
		// given by the main PSD header (h).

		struct channel_info *merged_chans = checkmalloc(h->channels*sizeof(struct channel_info));

		// The 'merged' or 'composite' image is where the flattened image is stored
		// when 'Maximise Compatibility' is used.
		// It consists of:
		// - the alpha channel for merged image (if mergedalpha is TRUE)
		// - the merged image (1 or 3 channels)
		// - any remaining alpha or spot channels.

		printf("\nmerged channels:\n");
		dochannel(f, NULL, merged_chans, h->channels, h);
		for(ch = 0; ch < h->channels; ++ch){
			printf("  channel %d  id=%2d  %4u rows x %4u cols\n",
				   ch, merged_chans[ch].id, merged_chans[ch].rows, merged_chans[ch].cols);
		}

		free(merged_chans);
	}
}
