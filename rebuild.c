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

#include <stdio.h>
#include <stdlib.h>

#include "psdparse.h"

extern FILE *rebuilt_psd;

void writeheader(psd_file_t out_psd, int version, struct psd_header *h){
	fwrite("8BPS", 1, 4, out_psd);
	put2B(out_psd, version);
	put4B(out_psd, PAD_BYTE);
	put2B(out_psd, PAD_BYTE);
	put2B(out_psd, h->channels);
	put4B(out_psd, h->rows);
	put4B(out_psd, h->cols);
	put2B(out_psd, h->depth);
	put2B(out_psd, h->mode);
}

psd_bytes_t writepsdchannels(
		FILE *out_psd,
		int version,
		psd_file_t psd,
		int chindex,
		struct channel_info *ch,
		int chancount,
		struct psd_header *h)
{
	psd_pixels_t j, k, total_rows = chancount * ch->rows;
	psd_bytes_t *rowcounts;
	unsigned char *compbuf, *inrow, *rlebuf, *p;
	int i, comp;
	psd_bytes_t chansize, compsize;
	extern const char *comptype[];

	rlebuf    = checkmalloc(ch->rowbytes*2);
	inrow     = checkmalloc(ch->rowbytes);

	// compress channel(s) to decide if RLE is a saving

	compbuf   = checkmalloc(PACKBITSWORST(ch->rowbytes)*total_rows);
	rowcounts = checkmalloc(sizeof(psd_bytes_t)*total_rows);

	p = compbuf;
	compsize = 0;
	for(i = k = 0; i < chancount; ++i){
		for(j = 0; j < ch[i].rows; ++j, ++k){
			readunpackrow(psd, ch+i, j, inrow, rlebuf);
			rowcounts[k] = packbits(inrow, p, ch[i].rowbytes);
			compsize += rowcounts[k];
			p += rowcounts[k];
		}
	}
	// allow for row counts:
	chansize = (total_rows << version) + compsize;

	if(chansize < total_rows*ch->rowbytes){
		// RLE was shorter, so use compressed data.

		put2B(out_psd, comp = RLECOMP);
		for(j = 0; j < total_rows; ++j){
			if(version == 1){
				if(rowcounts[j] > UINT16_MAX)
					fatal("## row count out of range for PSD (v1) format. Try without --rebuildpsd.\n");
				put2B(out_psd, rowcounts[j]);
			}else{
				put4B(out_psd, rowcounts[j]);
			}
		}

		if((psd_pixels_t)fwrite(compbuf, 1, compsize, out_psd) != compsize){
			alwayswarn("# error writing psd channel (RLE), aborting\n");
			return 0;
		}

		free(compbuf);
		free(rowcounts);
	}else{
		// There was no saving using RLE, so don't compress.

		put2B(out_psd, comp = RAWDATA);
		chansize = total_rows*ch->rowbytes;
		for(i = 0; i < chancount; ++i){
			for(j = 0; j < ch[i].rows; ++j){
				/* get row data */
				readunpackrow(psd, ch+i, j, inrow, rlebuf);

				/* write an uncompressed row */
				if((psd_pixels_t)fwrite(inrow, 1, ch[i].rowbytes, out_psd) != ch[i].rowbytes){
					alwayswarn("# error writing psd channel (raw), aborting\n");
					return 0;
				}
			}
		}
	}

	chansize += 2; // allow for compression type field

	if(chancount > 1){
		VERBOSE("#   %d channels: %6u bytes (%s)\n", chancount, (unsigned)chansize, comptype[comp]);
	}else{
		VERBOSE("#   channel %d: %6u bytes (%s)\n", chindex, (unsigned)chansize, comptype[comp]);
	}

	free(rlebuf);
	free(inrow);

	return chansize;
}

psd_bytes_t writedummymerged(
		FILE *out_psd,
		int version,
		struct psd_header *h)
{
	unsigned i;
	psd_pixels_t j;
	psd_bytes_t rowbytes = (h->cols*h->depth + 7)/8;
	char *inrow = calloc(rowbytes, 1);

	put2B(out_psd, RAWDATA); // uncompressed data

	for(i = 0; i < h->channels; ++i){
		for(j = 0; j < h->rows; ++j){
			/* write an uncompressed row */
			if((psd_pixels_t)fwrite(inrow, 1, rowbytes, out_psd) != rowbytes){
				alwayswarn("# error writing merged channel, aborting\n");
				return 0;
			}
		}
	}
	free(inrow);
	return 2 + h->channels * h->rows * rowbytes;
}

static int32_t bounds_top, bounds_left, bounds_bottom, bounds_right;

psd_bytes_t writelayerinfo(psd_file_t psd, psd_file_t out_psd,
						   int version, struct psd_header *h,
						   psd_pixels_t h_offset, psd_pixels_t v_offset)
{
	int i, j, namelen, mask_size, extralen;
	psd_bytes_t size;
	struct layer_info *li;

	put2B(out_psd, h->mergedalpha ? -h->nlayers : h->nlayers);
	size = 2;
	for(i = 0, li = h->linfo; i < h->nlayers; ++i, ++li){
		// accumulate this layer's bounding rectangle into global bounds
		if(bounds_top > li->top)
			bounds_top = li->top;
		if(bounds_left > li->left)
			bounds_left = li->left;
		if(bounds_bottom < li->bottom)
			bounds_bottom = li->bottom;
		if(bounds_right < li->right)
			bounds_right = li->right;

		put4B(out_psd, li->top + v_offset);
		put4B(out_psd, li->left + h_offset);
		put4B(out_psd, li->bottom + v_offset);
		put4B(out_psd, li->right + h_offset);
		put2B(out_psd, li->channels);
		size += 18;
		for(j = 0; j < li->channels; ++j){
			put2B(out_psd, li->chan[j].id);
			putpsdbytes(out_psd, version, li->chan[j].length_rebuild);
			size += 2 + PSDBSIZE(version);
		}
		fwrite(li->blend.sig, 1, 4, out_psd);
		fwrite(li->blend.key, 1, 4, out_psd);
		fputc(li->blend.opacity, out_psd);
		fputc(li->blend.clipping, out_psd);
		fputc(li->blend.flags, out_psd);
		fputc(PAD_BYTE, out_psd);
		size += 12;

		// layer's 'extra data' section ================================

		// TODO: Flag damaged layers in the name and optionally hide them automatically
		namelen = strlen(li->name);
		mask_size = li->mask.size >= 36 ? 36 : (li->mask.size >= 20 ? 20 : 0);

		extralen = 4 + mask_size + 4 + PAD4(namelen+1);
		put4B(out_psd, extralen);
		size += 4 + extralen;

		// layer mask data ---------------------------------------------
		put4B(out_psd, mask_size);
		if(mask_size >= 20){
			put4B(out_psd, li->mask.top + v_offset);
			put4B(out_psd, li->mask.left + h_offset);
			put4B(out_psd, li->mask.bottom + v_offset);
			put4B(out_psd, li->mask.right + h_offset);
			fputc(li->mask.default_colour, out_psd);
			fputc(li->mask.flags, out_psd);
			mask_size -= 18;
			if(mask_size >= 36){
				fputc(li->mask.real_flags, out_psd);
				fputc(li->mask.real_default_colour, out_psd);
				put4B(out_psd, li->mask.real_top);
				put4B(out_psd, li->mask.real_left);
				put4B(out_psd, li->mask.real_bottom);
				put4B(out_psd, li->mask.real_right);
				mask_size -= 18;
			}
			while(mask_size--)
				fputc(PAD_BYTE, out_psd);
		}

		// layer blending ranges ---------------------------------------
		put4B(out_psd, 0); // empty

		// layer name --------------------------------------------------
		fputc(namelen, out_psd);
		fwrite(li->name, 1, PAD4(namelen+1)-1, out_psd);

		// additional layer information --------------------------------
		// currently empty, but if non-empty, is accounted for in size

		// End of layer records section ================================
	}

	return size;
}

psd_bytes_t copy_block(psd_file_t psd, psd_file_t out_psd, psd_bytes_t pos){
	char *tempbuf;
	psd_bytes_t n, cnt;

	fseeko(psd, pos, SEEK_SET);
	n = get4B(psd); // TODO: sanity check this byte count
	tempbuf = malloc(n);
	fread(tempbuf, 1, n, psd);
	put4B(out_psd, n);
	cnt = fwrite(tempbuf, 1, n, out_psd);
	free(tempbuf);
	if(cnt != n)
		alwayswarn("# copy_block(): only wrote %d of %d bytes\n", cnt, n);
	return 4 + cnt;
}

void rebuild_psd(psd_file_t psd, int version, struct psd_header *h){
	psd_bytes_t lmipos, lmilen, layerlen, checklen;
	int32_t h_offset = 0, v_offset = 0;
	int i, j;
	struct layer_info *li;

	if(merged_only)
		h->nlayers = 0;

	// File header =====================================================
	writeheader(rebuilt_psd, version, h);

	// copy color mode data --------------------------------------------
	copy_block(psd, rebuilt_psd, h->colormodepos);

	// TODO: image resources -------------------------------------------
	put4B(rebuilt_psd, 0); // empty for now

	// Layer and mask information ======================================
	lmipos = (psd_bytes_t)ftello(rebuilt_psd);
	putpsdbytes(rebuilt_psd, version, 0); // dummy lmi length
	lmilen = 0;

	if(h->nlayers){
		// Layer info --------------------------------------------------
		putpsdbytes(rebuilt_psd, version, 0); // dummy layer info length
		lmilen += PSDBSIZE(version); // account for layer info length field
		bounds_top = bounds_left = bounds_bottom = bounds_right = 0;
		layerlen = checklen = writelayerinfo(psd, rebuilt_psd, version, h, 0, 0);

		VERBOSE("# rebuilt layer info: %u bytes\n", (unsigned)layerlen);

		// Image data --------------------------------------------------
		for(i = 0, li = h->linfo; i < h->nlayers; ++i, ++li){
			UNQUIET("# rebuilding layer %d: %s\n", i, li->name);

			for(j = 0; j < li->channels; ++j)
				layerlen += li->chan[j].length_rebuild =
						writepsdchannels(rebuilt_psd, version, psd, j, li->chan + j, 1, h);
		}

		// Even alignment ----------------------------------------------
		if(layerlen & 1){
			++layerlen;
			fputc(PAD_BYTE, rebuilt_psd);
		}

		// Global layer mask info --------------------------------------
		put4B(rebuilt_psd, 0); // empty for now
		layerlen += 4;
	}

	// Merged image data ===============================================
	if(h->merged_chans){
		UNQUIET("# rebuilding merged image\n");
		writepsdchannels(rebuilt_psd, version, psd, 0, h->merged_chans, h->channels, h);
	}else{
		// For some reason, we have no information about the merged image,
		// (scavenging?) so write a dummy image.

		UNQUIET("# unable to recover merged image; using blank image\n");

		if(!h->rows && !h->cols){
			// If we are scavenging, and the size of the (merged) document
			// wasn't given, then fake the dimensions based on combined bounds
			// of all layers. Then shift all layers into this area.
			h->rows = bounds_bottom - bounds_top;
			v_offset = -bounds_top;
			h->cols = bounds_right - bounds_left;
			h_offset = -bounds_left;
			UNQUIET("# sized document to %d rows x %d columns to fit all layers\n", h->rows, h->cols);
		}

		if(h->mode == -1){
			// mode wasn't specified (scavenging); try to guess
			if(h->depth == 1)
				h->mode = ModeBitmap;
			else if(h->channels <= 2)
				h->mode = ModeGrayScale;
			else if(h->channels <= 4)
				h->mode = ModeRGBColor;
			else
				h->mode = ModeMultichannel;
			UNQUIET("# tried to guess image mode: %s  (if this is incorrect, try --mode N with --scavenge)\n",
					mode_names[h->mode]);
		}

		writedummymerged(rebuilt_psd, version, h);

		// fixup header
		fseeko(rebuilt_psd, 0, SEEK_SET);
		writeheader(rebuilt_psd, version, h);
	}

	// File complete ===================================================

	// now do fixups for layer info/layer mask info and image data sizes

	if(h->nlayers){
		// overwrite layer & mask information with fixed-up sizes
		fseeko(rebuilt_psd, lmipos, SEEK_SET);
		putpsdbytes(rebuilt_psd, version, lmilen + layerlen); // do fixup
		putpsdbytes(rebuilt_psd, version, layerlen); // do fixup
		if(writelayerinfo(psd, rebuilt_psd, version, h, h_offset, v_offset) != checklen)
			fatal("# oops! rewritten layer info different size from first pass");
	}

	VERBOSE("# rebuild done.\n");
}
