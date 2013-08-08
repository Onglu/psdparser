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

#include "psdparse.h"
#include "png.h"
#include "psdparser.h"

extern PSD_LAYERS_INFO g_info;
extern int g_lid;

static void writeimage(psd_file_t psd, char *dir, char *name,
					   struct layer_info *li,
					   struct channel_info *chan,
					   int channels, long rows, long cols,
					   struct psd_header *h, int color_type)
{
	FILE *outfile;

	if(writepng){
		if(h->depth == 32){
			if((outfile = rawsetupwrite(psd, dir, name, cols, rows, channels, color_type, li, h)))
				rawwriteimage(outfile, psd, li, chan, channels, h);
		}else{
			if((outfile = pngsetupwrite(psd, dir, name, cols, rows, channels, color_type, li, h)))
				pngwriteimage(outfile, psd, li, chan, channels, h);
		}
	}
}

static void writechannels(psd_file_t f, char *dir, char *name,
						  struct layer_info *li,
						  struct channel_info *chan,
						  int channels, struct psd_header *h)
{
	char pngname[FILENAME_MAX] = {0};
	const char num[12] = "0123456789a";
	int ch, i, j;
	PSD_LAYER_INFO *layers;

	for(ch = 0; ch < channels; ++ch){
		// build PNG file name
		strcpy(pngname, name);

		if(chan[ch].id == LMASK_CHAN_ID){
					if(xml){
						fprintf(xml, "\t\t<LAYERMASK TOP='%d' LEFT='%d' BOTTOM='%d' RIGHT='%d' ROWS='%d' COLUMNS='%d' DEFAULTCOLOR='%d'>\n",
								li->mask.top, li->mask.left, li->mask.bottom, li->mask.right,
								li->mask.bottom - li->mask.top, li->mask.right - li->mask.left,
								li->mask.default_colour);
						if(li->mask.flags & 1) fputs("\t\t\t<POSITIONRELATIVE />\n", xml);
						if(li->mask.flags & 2) fputs("\t\t\t<DISABLED />\n", xml);
						if(li->mask.flags & 4) fputs("\t\t\t<INVERT />\n", xml);
					}
					else
					{
						UNQUIET("LAYERMASK TOP='%d' LEFT='%d' BOTTOM='%d' RIGHT='%d' ROWS='%d' COLUMNS='%d' DEFAULTCOLOR='%d'\n",
							li->mask.top, li->mask.left, li->mask.bottom, li->mask.right,
							li->mask.bottom - li->mask.top, li->mask.right - li->mask.left,
							li->mask.default_colour);
					}

					if (LT_Photo == g_info.layers[g_lid].type)
					{
						for (i = 0; i < MAX_UUID_LEN; i++)
						{
							switch (pngname[i])
							{
							case 'a':pngname[i] = 'b';break;
							case 'b':pngname[i] = 'c';break;
							case 'c':pngname[i] = 'd';break;
							case 'd':pngname[i] = 'e';break;
							case 'e':pngname[i] = 'f';break;
							case 'f':
							case '-':
								continue;
							default:
								j = pngname[i] & 0x0f;
								pngname[i] = num[j + 1];
								break;
							}
						}

						layers = g_info.layers;
						j = g_info.count++;
						g_info.layers = (PSD_LAYER_INFO *)realloc(layers, g_info.count * sizeof(PSD_LAYER_INFO));
						if (!g_info.layers)
						{
							return;
						}

						memset(&g_info.layers[j], 0, sizeof(PSD_LAYER_INFO));
						g_info.layers[j].type = LT_Mask;

						strcpy(g_info.layers[j].lid, pngname);
						strcpy(g_info.layers[g_lid].mid, pngname);
						sprintf(g_info.layers[j].name, "%s(lmask)", g_info.layers[g_lid].name);

						g_info.layers[j].top = g_info.layers[g_lid].top;
						g_info.layers[j].left = g_info.layers[g_lid].left;
						g_info.layers[j].bottom = g_info.layers[g_lid].bottom;
						g_info.layers[j].right = g_info.layers[g_lid].right;
						g_info.layers[j].bound.width = g_info.layers[j].actual.width = li->mask.right - li->mask.left;
						g_info.layers[j].bound.height = g_info.layers[j].actual.height = li->mask.bottom - li->mask.top;
						g_info.layers[j].center.x = (int)g_info.layers[j].bound.width / 2 + li->mask.left;
						g_info.layers[j].center.y = (int)g_info.layers[j].bound.height / 2 + li->mask.top;
						g_info.layers[j].opacity = 1;
					}
					else
					{
						strcat(pngname, ".lmask");
					}
					
		}else if(chan[ch].id == UMASK_CHAN_ID){
			if(xml){
				fprintf(xml, "\t\t<USERLAYERMASK TOP='%d' LEFT='%d' BOTTOM='%d' RIGHT='%d' ROWS='%d' COLUMNS='%d' DEFAULTCOLOR='%d'>\n",
						li->mask.real_top, li->mask.real_left, li->mask.real_bottom, li->mask.real_right,
						li->mask.real_bottom - li->mask.real_top, li->mask.real_right - li->mask.real_left,
						li->mask.real_default_colour);
				if(li->mask.real_flags & 1) fputs("\t\t\t<POSITIONRELATIVE />\n", xml);
				if(li->mask.real_flags & 2) fputs("\t\t\t<DISABLED />\n", xml);
				if(li->mask.real_flags & 4) fputs("\t\t\t<INVERT />\n", xml);
			}

			strcat(pngname, ".umask");
		}else if(chan[ch].id == TRANS_CHAN_ID){
			if(xml) fputs("\t\t<TRANSPARENCY>\n", xml);
			strcat(pngname, li ? ".trans" : ".alpha");
		}else{
			if(xml)
				fprintf(xml, "\t\t<CHANNEL ID='%d'>\n", chan[ch].id);
			if(chan[ch].id < (int)strlen(channelsuffixes[h->mode])) // can identify channel by letter
				sprintf(pngname+strlen(pngname), ".%c", channelsuffixes[h->mode][chan[ch].id]);
			else // give up and use a number
				sprintf(pngname+strlen(pngname), ".%d", chan[ch].id);
		}

		if(chan[ch].comptype == -1)
			alwayswarn("## not writing \"%s\", bad channel compression type\n", pngname);
		else
			writeimage(f, dir, pngname, li, chan + ch, 1,
					   chan[ch].rows, chan[ch].cols, h, PNG_COLOR_TYPE_GRAY);

		if(chan[ch].id == LMASK_CHAN_ID){
			if(xml) fputs("\t\t</LAYERMASK>\n", xml);
		}else if(chan[ch].id == TRANS_CHAN_ID){
			if(xml) fputs("\t\t</TRANSPARENCY>\n", xml);
		}else{
			if(xml) fputs("\t\t</CHANNEL>\n", xml);
		}
	}
}

void doimage(psd_file_t f, struct layer_info *li, char *name, struct psd_header *h)
{
	// map channel count to a suitable PNG mode (when scavenging and actual mode is not known)
	static int png_mode[] = {0, PNG_COLOR_TYPE_GRAY, PNG_COLOR_TYPE_GRAY_ALPHA,
								PNG_COLOR_TYPE_RGB,  PNG_COLOR_TYPE_RGB_ALPHA};
	int ch, pngchan = 0, color_type = 0, has_alpha = 0,
		channels = li ? li->channels : h->channels;
	psd_bytes_t image_data_end;
	char fname[PATH_MAX] = {0};

	if(h->mode == SCAVENGE_MODE){
		pngchan = channels;
		color_type = png_mode[pngchan];
	}
	else{
		has_alpha = li ? li->chindex[TRANS_CHAN_ID] != -1
					   : h->mergedalpha && channels > mode_channel_count[h->mode];
		pngchan = mode_channel_count[h->mode] + has_alpha;

		switch(h->mode){
		default: // multichannel, cmyk, lab etc
			split = 1;
		case ModeBitmap:
		case ModeGrayScale:
		case ModeGray16:
		case ModeDuotone:
		case ModeDuotone16:
			color_type = has_alpha ? PNG_COLOR_TYPE_GRAY_ALPHA : PNG_COLOR_TYPE_GRAY;
			break;
		case ModeIndexedColor:
			color_type = PNG_COLOR_TYPE_PALETTE;
			break;
		case ModeRGBColor:
		case ModeRGB48:
			color_type = has_alpha ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB;
			break;
		}
	}

	if(li){
		// Process layer

		for(ch = 0; ch < channels; ++ch){
			VERBOSE("  channel %d:\n", ch);
			dochannel(f, li, li->chan + ch, 1/*count*/, h);
		}

		image_data_end = (psd_bytes_t)ftello(f);

		if(writepng && !merged_only){
			nwarns = 0;

			if (g_lid < g_info.count && g_info.layers && MAX_UUID_LEN == strlen(g_info.layers[g_lid].lid))
			{
				strcpy(fname, g_info.layers[g_lid].lid);
			}
			else
			{
				strcpy(fname, name);
			}

			if(pngchan && !split){
				writeimage(f, pngdir, fname, li, li->chan,
						   h->depth == 32 ? channels : pngchan,
						   li->bottom - li->top, li->right - li->left,
						   h, color_type);

				if(h->depth < 32){
					// spit out any 'extra' channels (e.g. layer mask)
					for(ch = 0; ch < channels; ++ch)
						if(li->chan[ch].id < -1 || li->chan[ch].id >= pngchan)
							writechannels(f, pngdir, fname, li, li->chan + ch, 1, h);
				}
			}
			else{
				UNQUIET("# writing layer as split channels...\n");
				writechannels(f, pngdir, fname, li, li->chan, channels, h);
			}
		}
	}
	else{	// Ignore the below case
		h->merged_chans = checkmalloc(channels*sizeof(struct channel_info));

		// The 'merged' or 'composite' image is where the flattened image is stored
		// when 'Maximise Compatibility' is used.
		// It consists of:
		// - the merged image (1 or 3 channels)
		// - the alpha channel for merged image (if mergedalpha is TRUE)
		// - any remaining alpha or spot channels.
		// For an identifiable image mode (Bitmap, GreyScale, Duotone, Indexed or RGB),
		// we should ideally
		// 1) write the first 1[+alpha] or 3[+alpha] channels in appropriate PNG format
		// 2) write the remaining channels as extra GRAY PNG files.
		// (For multichannel (and maybe other?) modes, we should just write all
		// channels per step 2)

		VERBOSE("\n  merged image:\n");
		dochannel(f, NULL, h->merged_chans, channels, h);

		image_data_end = ftello(f);

		if(xml)
			fprintf(xml, "\t<COMPOSITE CHANNELS='%d' HEIGHT='%d' WIDTH='%d'>\n",
					channels, h->rows, h->cols);

		nwarns = 0;
		ch = 0;
		if(pngchan && !split){
			writeimage(f, pngdir, name, NULL, h->merged_chans,
					   h->depth == 32 ? channels : pngchan,
					   h->rows, h->cols, h, color_type);
			ch += pngchan;
		}
		if(writepng && ch < channels){
			if(split){
				UNQUIET("# writing %s image as split channels...\n", mode_names[h->mode]);
			}else{
				UNQUIET("# writing %d extra channels...\n", channels - ch);
			}

			writechannels(f, pngdir, name, NULL, h->merged_chans + ch, channels - ch, h);
		}

		if(xml) fputs("\t</COMPOSITE>\n", xml);
	}

	// caller may expect this file position
	fseeko(f, image_data_end, SEEK_SET);
}
