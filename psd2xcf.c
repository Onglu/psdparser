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

#include <stdio.h>

#include "psdparse.h"
#include "xcf.h"
#include "version.h"

/* This program outputs a layered GIMP XCF file converted from PSD data.
 *
 * build:
 *     make psd2xcf -f Makefile.unix
 *
 * This file is the main program and driver, which calls upon routines
 * in psd.c (and other parts of psdparse) to interpret PSD input,
 * and then writes the XCF using routines in xcf.c
 */

int verbose = 0, quiet = 0, rsrc = 1, print_rsrc = 0, resdump = 0, extra = 0,
	makedirs = 0, numbered = 0, help = 0, split = 0, xmlout = 0,
	writepng = 0, writelist = 0, writexml = 0, unicode_filenames = 1,
	use_merged = 0, merged_only = 0, extra_chan, rebuild = 0;
long hres, vres; // set by doresources()
char *pngdir;
off_t xcf_merged_pos, *xcf_chan_pos; // updated by doimage() if merged image is processed

static FILE *xcf;
static int xcf_compr = 1; // RLE

void usage(char *prog, int status){
	fprintf(stderr, "usage: %s [options] psdfile...\n\
  -h, --help         show this help\n\
  -V, --version      show version\n\
  -v, --verbose      print more information\n\
  -q, --quiet        work silently\n\
  -c, --rle          RLE compression (default)\n\
  -u, --raw          no compression\n\
  -m, --merged       include merged (flattened) image as top layer, if available,\n\
                     and all additional non-layer channels\n\
      --merged-only  process merged image & extra channels, but omit all layers\n", prog);
	exit(status);
}

int main(int argc, char *argv[]){
	static struct option longopts[] = {
		{"help",       no_argument, &help, 1},
		{"version",    no_argument, NULL, 'V'},
		{"verbose",    no_argument, &verbose, 1},
		{"quiet",      no_argument, &quiet, 1},
		{"rle",        no_argument, &xcf_compr, 1},
		{"raw",        no_argument, &xcf_compr, 0},
		{"merged",     no_argument, &use_merged, 1},
		{"merged-only",no_argument, &merged_only, 1},
		{NULL,0,NULL,0}
	};
	FILE *f;
	struct psd_header h;
	int arg, i, indexptr, opt;
	off_t xcf_layers_pos, xcf_channels_pos;

	while( (opt = getopt_long(argc, argv, "hVvqcum", longopts, &indexptr)) != -1 )
		switch(opt){
		case 0: break; // long option
		case 'h': help = 1; break;
		case 'V':
			printf("psd2xcf version " VERSION_STR
				   "\nCopyright (C) 2004-2010 Toby Thain <toby@telegraphics.com.au>\n");
			return EXIT_SUCCESS;
		case 'v': verbose = 1; break;
		case 'q': quiet = 1; break;
		case 'c': xcf_compr = 1; break;
		case 'u': xcf_compr = 0; break;
		case 'm': use_merged = 1; break;
		default:  usage(argv[0], EXIT_FAILURE);
		}

	if(optind >= argc)
		usage(argv[0], EXIT_FAILURE);
	else if(help)
		usage(argv[0], EXIT_SUCCESS);

	for(arg = optind; arg < argc; ++arg){
		if( (f = fopen(argv[arg], "rb")) ){
			h.version = h.nlayers = h.mergedalpha = 0;
			h.layerdatapos = 0;

			if(dopsd(f, argv[arg], &h)){
				if(h.depth != 8){
					alwayswarn("# input file must be 8 bits/channel; skipping %s\n", argv[arg]);
					continue;
				}
				else if(h.mode != ModeGrayScale
					 && h.mode != ModeIndexedColor
					 && h.mode != ModeRGBColor)
				{
					alwayswarn("# input file must be Grey Scale, Indexed or RGB mode; skipping %s\n", argv[arg]);
					continue;
				}

				if(h.nlayers == 0 && !use_merged){
					alwayswarn("# %s has no layers. Using merged image.\n", argv[arg]);
					use_merged = 1;
				}

				if( (xcf = xcf_open(argv[arg], &h)) ){
					// xcf_open() has written the XCF header.

					// -------------- Image properties --------------
					xcf_prop_compression(xcf, xcf_compr);
					// image resolution in pixels per cm
					xcf_prop_resolution(xcf, FIXEDPT(hres)/2.54, FIXEDPT(vres)/2.54);
					if(h.mode == ModeIndexedColor) // copy palette from psd to xcf
						xcf_prop_colormap(xcf, f, &h);
					xcf_prop_end(xcf); // end image properties

					// -------------- Layer pointers --------------
					// write dummies now, fixup later.
					xcf_layers_pos = ftello(xcf);

					if(use_merged || merged_only)
						put4xcf(xcf, 0); // slot for merged image layer

					if(!merged_only){
						// Ignore zero-sized layers.
						for(i = h.nlayers; i--;)
							if(h.linfo[i].right > h.linfo[i].left
							&& h.linfo[i].bottom > h.linfo[i].top)
								put4xcf(xcf, 0); // placeholder only
					}
					put4xcf(xcf, 0); // end of layer pointers

					// -------------- Channel pointers --------------
					// Only process these if merged image has been requested.
					extra_chan = 0;
					if(use_merged || merged_only){
						xcf_channels_pos = ftello(xcf);
						// count how many channels exist in the merged data
						// beyond the image channels and any alpha channel
						extra_chan = h.channels - mode_channel_count[h.mode] - h.mergedalpha;
						if(extra_chan < 0){
							alwayswarn("# unexpected psd channel count: %d  (expected mode channels: %d + merged alpha: %d)\n",
									   h.channels, mode_channel_count[h.mode], h.mergedalpha);
							extra_chan = 0;
						}
						for(i = extra_chan; i--;)
							put4xcf(xcf, 0); // placeholder only
					}
					put4xcf(xcf, 0); // end of channel pointers

					// -------------- Layers --------------
					if(!merged_only){
						// process the layers in 'image data' section.
						// this will, in turn, call doimage() for each layer.
						fseeko(f, h.layerdatapos, SEEK_SET);
						processlayers(f, &h);
					}

					// -------------- Merged image --------------
					if(use_merged || merged_only){
						// position file after 'layer & mask info', i.e. at the
						// beginning of the merged image data.
						fseeko(f, h.lmistart + h.lmilen, SEEK_SET);

						// process merged (composite) image data
						xcf_merged_pos = 0;
						doimage(f, NULL, NULL, &h);
					}

					// -------------- Fixup layer pointers --------------
					// In reverse of the PSD order, since XCF stores
					// layers top to bottom.
					fseeko(xcf, xcf_layers_pos, SEEK_SET);

					if(use_merged || merged_only)
						put4xcf(xcf, xcf_merged_pos);

					if(!merged_only && h.nlayers){
						VERBOSE("xcf layer offset fixup (top to bottom):\n");
						for(i = h.nlayers-1; i >= 0; --i){
							if(h.linfo[i].right > h.linfo[i].left
							   && h.linfo[i].bottom > h.linfo[i].top)
							{
								VERBOSE("  layer %3d xcf @ %7ld  \"%s\"\n",
										i,
										(long)h.linfo[i].xcf_pos,
										h.linfo[i].unicode_name ? h.linfo[i].unicode_name : h.linfo[i].name);
								put4xcf(xcf, h.linfo[i].xcf_pos);
							}
							else{
								VERBOSE("  layer %3d       skipped  \"%s\"\n",
										i,
										h.linfo[i].unicode_name ? h.linfo[i].unicode_name : h.linfo[i].name);
							}
						}
					}
					put4xcf(xcf, 0); // end of layer pointers

					// -------------- Fixup channel pointers --------------
					for(i = 0; i < extra_chan; ++i)
						put4xcf(xcf, xcf_chan_pos[i]);
					//put4xcf(xcf, 0); // end of channel pointers

					// -------------- XCF file is complete --------------
					fclose(xcf);

					UNQUIET("Done: %s\n\n", argv[arg]);
				}
				else{
					fatal("could not open xcf file for writing\n");
				}
			}else{
				fprintf(stderr, "Not a PSD or PSB file.\n");
			}

			fclose(f);
		}else{
			fprintf(stderr, "Could not open: %s\n", argv[arg]);
		}
	}

	return EXIT_SUCCESS;
}

/* This function is a callback from processlayers(). It needs the
 * input file positioned at the beginning of the image data
 * for the layer, or merged (flattened) image data, whichever is being
 * processed. */

void doimage(psd_file_t f, struct layer_info *li, char *name, struct psd_header *h)
{
	int ch, i;
	psd_bytes_t image_data_end;

	/* li points to layer information. If it is NULL, then
	 * the merged image is being being processed, not a layer. */

	if(li){
		// Process layer.

		UNQUIET("layer \"%s\"\n", li->name);
		for(ch = 0; ch < li->channels; ++ch){
			// dochannel() initialises the li->chan[ch] struct

			dochannel(f, li, li->chan + ch, 1, h);
			VERBOSE("  channel %d  id=%2d  %4u rows x %4u cols  %6ld bytes\n",
				    ch, li->chan[ch].id, li->chan[ch].rows, li->chan[ch].cols,
				    (long)li->chan[ch].length);
		}

		image_data_end = ftello(f);

		// xcf_layer() does alter the input PSD file position!
		li->xcf_pos = li->right > li->left && li->bottom > li->top
							? xcf_layer(xcf, f, li, xcf_compr)
							: 0;

		// caller may be assuming this position
		fseeko(f, image_data_end, SEEK_SET);
	}
	else{
		// The merged image has the size, mode, depth, and channel count
		// given by the main PSD header (h).

		struct channel_info *merged_chans
				= checkmalloc(h->channels*sizeof(struct channel_info));
		struct layer_info mli;

		// The 'merged' or 'composite' image is where the flattened
		// image is stored when 'Maximise Compatibility' is used.
		// It consists of:
		// - the merged image (1 or 3 channels)
		// - the alpha channel for merged image (if mergedalpha is TRUE)
		// - any remaining alpha or spot channels.

		UNQUIET("merged image, %4u rows x %4u cols\n", h->rows, h->cols);

		dochannel(f, NULL, merged_chans, h->channels, h);

		mli.top = mli.left = 0;
		mli.bottom = h->rows;
		mli.right = h->cols;
		mli.channels = h->channels;
		mli.chan = merged_chans;
		mli.chindex = NULL; // xcf.c doesn't use this
		memcpy(mli.blend.key, "norm", 4); // normal blend mode
		mli.blend.opacity = 255;
		mli.blend.clipping = 0;
		// if there are other layers, then hide merged image
		mli.blend.flags = !merged_only && h->nlayers ? 2 : 0;
		mli.mask.size = 0; // no layer mask
		mli.name = mli.unicode_name = "Merged image";

		xcf_merged_pos = xcf_layer(xcf, f, &mli, xcf_compr);

		if(extra_chan){
			char ch_name[16];
			xcf_chan_pos = checkmalloc(extra_chan*sizeof(off_t));

			UNQUIET("extra channels (%d)\n", extra_chan);
			for(ch = mode_channel_count[h->mode] + h->mergedalpha, i = 0;
				ch < h->channels;
				++ch, ++i)
			{
				sprintf(ch_name, "Channel %d", i+1);
				VERBOSE("  storing channel index %d as \"%s\"\n", ch, ch_name);
				xcf_chan_pos[i] = xcf_channel(xcf, f, h->cols, h->rows,
											  ch_name, 0, /* visible */
											  merged_chans+ch, xcf_compr);
			}
		}

		free(merged_chans);
	}
}
