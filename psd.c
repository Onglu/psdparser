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

#include "psdparse.h"
#include "cwprefix.h"
#include "psdparser.h"

extern PSD_LAYERS_INFO g_info;
extern int g_lid;
char dirsep[] = {DIRSEP,0};
FILE *listfile = NULL, *xml = NULL;

void skipblock(psd_file_t f, char *desc){
	extern void ir_dump(psd_file_t f, int level, int len, struct dictentry *parent);
	psd_bytes_t n = get4B(f);
	if(n){
		if(verbose > 1){
			VERBOSE("%s:\n", desc);
			ir_dump(f, 0, n, NULL);
		}
		else{
			fseeko(f, n, SEEK_CUR);
			VERBOSE("  ...skipped %s (" LL_L("%lld","%ld") " bytes)\n", desc, n);
		}
	}else
		VERBOSE("  (%s is empty)\n", desc);
}

/**
 * Process the "layer and mask information" section. The file must be
 * positioned at the beginning of this section when dolayermaskinfo()
 * is called.
 *
 * The function sets h->nlayers to the number of layers in the PSD,
 * and also populates the h->linfo[] array.
 */

void readlayerinfo(psd_file_t f, struct psd_header *h, int i)
{
	psd_bytes_t extralen, extrastart;
	int j, chid, namelen;
	char *chidstr, tmp[10];
	struct layer_info *li = h->linfo + i;

	// process layer record
	li->top = get4B(f);
	li->left = get4B(f);
	li->bottom = get4B(f);
	li->right = get4B(f);
	li->channels = get2Bu(f);

	VERBOSE("\n");
	UNQUIET("  layer %d: (%4d,%4d,%4d,%4d), %d channels (%4d rows x %4d cols)\n",
			i, li->top, li->left, li->bottom, li->right, li->channels,
			li->bottom-li->top, li->right-li->left);

	if( li->bottom < li->top || li->right < li->left
	 || li->channels > 64 ) // sanity check
	{
		alwayswarn("### something's not right about that, trying to skip layer.\n");
		fseeko(f, 6*li->channels+12, SEEK_CUR);
		skipblock(f, "layer info: extra data");
		li->chan = NULL;
		li->chindex = NULL;
		li->nameno = li->name = li->unicode_name = NULL;
	}
	else
	{
		li->chan = checkmalloc(li->channels*sizeof(struct channel_info));
		li->chindex = checkmalloc((li->channels+3)*sizeof(int));
		li->chindex += 3; // so we can index array from [-3] (hackish)

		for(j = -3; j < li->channels; ++j)
			li->chindex[j] = -1;

		// fetch info on each of the layer's channels

		for(j = 0; j < li->channels; ++j){
			li->chan[j].id = chid = get2B(f);
			li->chan[j].length = GETPSDBYTES(f);
			li->chan[j].rawpos = 0;
			li->chan[j].rowpos = NULL;
			li->chan[j].unzipdata = NULL;

			if(chid >= -3 && chid < li->channels)
				li->chindex[chid] = j;
			else
				warn_msg("unexpected channel id %d", chid);

			switch(chid){
			case UMASK_CHAN_ID: chidstr = " (user layer mask)"; break;
			case LMASK_CHAN_ID: chidstr = " (layer mask)"; break;
			case TRANS_CHAN_ID: chidstr = " (transparency mask)"; break;
			default:
				if(h->mode != SCAVENGE_MODE && chid < (int)strlen(channelsuffixes[h->mode]))
					sprintf(chidstr = tmp, " (%c)", channelsuffixes[h->mode][chid]); // it's a mode-ish channel
				else
					chidstr = ""; // don't know
			}
			VERBOSE("    channel %2d: " LL_L("%7lld","%7ld") " bytes, id=%2d %s\n",
					j, li->chan[j].length, chid, chidstr);
		}

		fread(li->blend.sig, 1, 4, f);
		fread(li->blend.key, 1, 4, f);
		li->blend.opacity = fgetc(f);
		li->blend.clipping = fgetc(f);
		li->blend.flags = fgetc(f);
		fgetc(f); // padding

		// process layer's 'extra data' section

		extralen = get4B(f);
		extrastart = (psd_bytes_t)ftello(f);
		VERBOSE("  (extra data: " LL_L("%lld","%ld") " bytes @ "
				LL_L("%lld","%ld") ")\n", extralen, extrastart);

		// fetch layer mask data
		li->mask.size = get4B(f);
		if(li->mask.size >= 20){
			off_t skip = li->mask.size;
			VERBOSE("  (has layer mask)\n");
			li->mask.top = get4B(f);
			li->mask.left = get4B(f);
			li->mask.bottom = get4B(f);
			li->mask.right = get4B(f);
			li->mask.default_colour = fgetc(f);
			li->mask.flags = fgetc(f);
			skip -= 18;
			if(li->mask.size >= 36){
				VERBOSE("  (has user layer mask)\n");
				li->mask.real_flags = fgetc(f);
				li->mask.real_default_colour = fgetc(f);
				li->mask.real_top = get4B(f);
				li->mask.real_left = get4B(f);
				li->mask.real_bottom = get4B(f);
				li->mask.real_right = get4B(f);
				skip -= 18;
			}
			fseeko(f, skip, SEEK_CUR); // skip remainder
		}else
			VERBOSE("  (no layer mask)\n");

		skipblock(f, "layer blending ranges");

		// layer name
		li->nameno = checkmalloc(16);
		sprintf(li->nameno, "layer%d", i+1);
		namelen = fgetc(f);
		li->name = checkmalloc(PAD4(namelen+1));
		fread(li->name, 1, PAD4(namelen+1)-1, f);
		li->name[namelen] = 0;
		if(namelen)
			UNQUIET("    name: \"%s\"\n", li->name);

		// process layer's 'additional info'

		li->additionalpos = (psd_bytes_t)ftello(f);
		li->additionallen = extrastart + extralen - li->additionalpos;

		// leave file positioned after extra data
		fseeko(f, extrastart + extralen, SEEK_SET);
	}
}

void dolayerinfo(psd_file_t f, struct psd_header *h){
	int i;

	// layers structure
	h->nlayers = get2B(f);
	h->mergedalpha = h->nlayers < 0;
	if(h->mergedalpha){
		h->nlayers = - h->nlayers;
		VERBOSE("  (first alpha is transparency for merged image)\n");
	}
	UNQUIET("\n%d layers:\n", h->nlayers);

	//if( h->nlayers*(18+6*h->channels) > layerlen ){ // sanity check
	//	alwayswarn("### unlikely number of layers, giving up.\n");
	//	return;
	//}

	h->linfo = checkmalloc(h->nlayers*sizeof(struct layer_info));

	// load linfo[] array with each layer's info

	for(i = 0; i < h->nlayers; ++i)
		readlayerinfo(f, h, i);
}

void dolayermaskinfo(psd_file_t f, struct psd_header *h){
	psd_bytes_t layerlen;

	h->nlayers = 0;
	h->lmilen = GETPSDBYTES(f);
	h->lmistart = (psd_bytes_t)ftello(f);
	if(h->lmilen){
		// process layer info section
		layerlen = GETPSDBYTES(f);
		if(layerlen){
			dolayerinfo(f, h);
      		// after processing all layers, file should now positioned at image data
		}else VERBOSE("  (layer info section is empty)\n");
	}else VERBOSE("  (layer & mask info section is empty)\n");
}

psd_bytes_t globallayermaskinfo(psd_file_t f, struct psd_header *h){
	psd_bytes_t n;
	int kind;

	h->global_lmi_pos = (psd_bytes_t)ftello(f);
	n = h->global_lmi_len = get4B(f);
	if(n){
		VERBOSE("  (global layer mask info section: %u bytes)\n", (unsigned)n);
		if(xml){
			if(n >= 13){
				fputs("\t<GLOBALLAYERMASK>\n", xml);
				ed_colorspace(f, 2, 0, NULL);
				fprintf(xml, "\t\t<OPACITY>%d</OPACITY>\n", get2B(f));
				kind = fgetc(f);
				switch(kind){
				case 0:   fputs("\t\t<COLORSELECTED/>\n", xml); break;
				case 1:   fputs("\t\t<COLORPROTECTED/>\n", xml); break;
				case 128: fputs("\t\t<PERLAYER/>\n", xml); break;
				default:  fprintf(xml, "\t\t<KIND>%d</KIND>\n", kind);
				}
				fputs("\t</GLOBALLAYERMASK>\n", xml);
				n -= 13;
			}
			else{
				alwayswarn("# global layer mask info only %d bytes, expected at least 13\n", n);
			}
		}
		fseeko(f, n, SEEK_CUR);
	}else VERBOSE("  (global layer mask info section is empty)\n");

	return n;
}

/**
 * Loop over all layers described by layer info section,
 * spit out a line in asset list if requested, and call
 * doimage() to process its image data.
 */

void processlayers(psd_file_t f, struct psd_header *h)
{
	int i, j = 0, c = 0;
	//char buf[7];
	psd_bytes_t savepos;
	extern char *last_layer_name;

	if(listfile) fputs("assetlist = {\n", listfile);

	for(i = 0; i < h->nlayers; ++i){
		struct layer_info *li = &h->linfo[i];
		psd_pixels_t cols = li->right - li->left, rows = li->bottom - li->top;

		VERBOSE("\n  layer %d (\"%s\"):\n", i, li->name);

		if(listfile && cols && rows){
			if(numbered)
				fprintf(listfile, "\t\"%s\" = { pos={%4d,%4d}, size={%4u,%4u} }, -- %s\n",
						li->nameno, li->left, li->top, cols, rows, li->name);
			else
				fprintf(listfile, "\t\"%s\" = { pos={%4d,%4d}, size={%4u,%4u} },\n",
						li->name, li->left, li->top, cols, rows);
		}
		if(xml){
			fputs("\t<LAYER NAME='", xml);
			fputsxml(li->name, xml); // FIXME: what encoding is this in? maybe PDF Latin?
			fprintf(xml, "' TOP='%d' LEFT='%d' BOTTOM='%d' RIGHT='%d' WIDTH='%u' HEIGHT='%u'>\n",
					li->top, li->left, li->bottom, li->right, cols, rows);
		}

		layerblendmode(f, 2, 1, &li->blend);

		last_layer_name = NULL;
		if(extra || unicode_filenames){
			// Process 'additional data' (non-image layer data,
			// such as adjustments, effects, type tool).

			savepos = (psd_bytes_t)ftello(f);
			fseeko(f, li->additionalpos, SEEK_SET);

			g_lid = j;

			UNQUIET("Layer %d additional data:\n", i);
			doadditional(f, h, 2, li->additionallen);

			fseeko(f, savepos, SEEK_SET); // restore file position
		}
		li->unicode_name = last_layer_name;

		if (g_info.layers)
		{
			if (li->top || li->left || li->bottom || li->right)
			{
				if (!writepng)
				{
					strcpy(g_info.layers[j].name, li->name);
					g_info.layers[j].opacity = (float)li->blend.opacity / 255;
					g_info.layers[j].top = li->top;
					g_info.layers[j].left = li->left;
					g_info.layers[j].bottom = li->bottom;
					g_info.layers[j].right = li->right;
					g_info.layers[j].channels = li->channels;
					g_info.layers[j].bound.width = cols;
					g_info.layers[j].bound.height = rows;
					g_info.layers[j].center.x = (int)cols / 2 + li->left;
					g_info.layers[j].center.y = (int)rows / 2 + li->top;
					g_info.layers[j].angle = calculate_angle(&g_info.layers[j]);
				}
				else
				{
// 					if (li->blend.clipping && j && !h->linfo[j - 1].blend.clipping && MAX_UUID_LEN == strlen(g_info.layers[j - 1].lid))
// 					{
//  						g_info.layers[j - 1].type = LT_Mask;
// 						g_info.layers[j].type = LT_Photo;
// 						strcpy(g_info.layers[j].mid, g_info.layers[j - 1].lid);
// 					}
// 					else
//					{
// 						memset(buf, 0, 7);
// 						strncpy(buf, &li->name[strlen(li->name) - 6], 6);
						if (/*!stricmp(buf, "@photo")*/ is_photo(li->name) && MAX_UUID_LEN == strlen(g_info.layers[j - 1].lid))
						{
							g_info.layers[j - 1].type = LT_Mask;
							g_info.layers[j].type = LT_Photo;
							strcpy(g_info.layers[j].mid, g_info.layers[j - 1].lid);
						}
//					}				
				}

				j++;
			}
		}

		doimage(f, li, unicode_filenames && last_layer_name ? last_layer_name : (numbered ? li->nameno : li->name), h);

		if(xml) fputs("\t</LAYER>\n\n", xml);
	}

	for (i = 0; i < g_info.count; i++)
	{
		if (g_info.layers[g_lid].canvas)
		{
			c = 1;
			break;
		}
	}
	
	if (!c)
	{
		g_info.layers[c].canvas = 1;
	}

	VERBOSE("## end of layer image data @ %ld\n", (long)ftello(f));
}

/**
 * Check PSD header; if everything seems ok, create list and xml output
 * files if requested, and process the layer & mask information section
 * to collect data on layers. (During which, description text will be sent to
 * the list and XML files, if they were created.)
 *
 * These output files are left open, because caller may later choose to
 * process image data, resulting in further output (to XML).
 */

int dopsd(psd_file_t f, char *psdpath, struct psd_header *h){
	int result = 0;

	// file header
	fread(h->sig, 1, 4, f);
	h->version = get2Bu(f);
	get4B(f); get2B(f); // reserved[6];
	h->channels = get2Bu(f);
	h->rows = get4B(f);
	h->cols = get4B(f);
	h->depth = get2Bu(f);
	h->mode = get2Bu(f);

	if(!feof(f) && KEYMATCH(h->sig, "8BPS")){
		if(h->version == 1
#ifdef PSBSUPPORT
		   || h->version == 2
#endif
		){
			openfiles(psdpath, h);

			if(listfile) fprintf(listfile, "-- PSD file: %s\n", psdpath);
			if(xml){
				fputs("<PSD FILE='", xml);
				fputsxml(psdpath, xml);
				fprintf(xml, "' VERSION='%u' CHANNELS='%u' ROWS='%u' COLUMNS='%u' DEPTH='%u' MODE='%u'",
						h->version, h->channels, h->rows, h->cols, h->depth, h->mode);
				if(h->mode >= 0 && h->mode < 16)
					fprintf(xml, " MODENAME='%s'", mode_names[h->mode]);
				fputs(">\n", xml);
			}
			UNQUIET("  PS%c (version %u), %u channels, %u rows x %u cols, %u bit %s\n",
					h->version == 1 ? 'D' : 'B', h->version, h->channels, h->rows, h->cols, h->depth,
					h->mode >= 0 && h->mode < 16 ? mode_names[h->mode] : "???");

			if(h->channels <= 0 || h->channels > 64 || h->rows <= 0
			   || h->cols <= 0 || h->depth <= 0 || h->depth > 32 || h->mode < 0)
			{
				alwayswarn("### something isn't right about that header, giving up now.\n");
			}
			else{
				h->colormodepos = (psd_bytes_t)ftello(f);
				if(h->mode == ModeDuotone)
					duotone_data(f, 1);
				else
					skipblock(f, "color mode data");

				h->resourcepos = (psd_bytes_t)ftello(f);
				if(rsrc || resdump)
					doimageresources(f);
				else
					skipblock(f, "image resources");

				dolayermaskinfo(f, h);

				h->layerdatapos = (psd_bytes_t)ftello(f);
				VERBOSE("## layer data begins @ " LL_L("%lld","%ld") "\n", h->layerdatapos);

				result = 1;
			}
		}else
			alwayswarn("# \"%s\": version %d not supported\n", psdpath, h->version);
	}else
		alwayswarn("# \"%s\": couldn't read header, or is not a PSD/PSB\n", psdpath);

	if(!result)
		alwayswarn("# Try --scavenge (and related options) to see if any layer data can be found.\n");

	return result;
}
