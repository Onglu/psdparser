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

// Read one row's data from the PSD file, according to the parameters:
//   chan   - points to the channel info struct
//   row    - row index
//   inrow  - destination for uncompressed row data (at least rowbytes in size)
//   rlebuf - temporary buffer for RLE decompression (at least 2*rowbytes in size)

void readunpackrow(psd_file_t psd,        // input file handle
				   struct channel_info *chan, // channel info
				   psd_pixels_t row,      // row index
				   unsigned char *inrow,  // dest buffer for the uncompressed row (rb bytes)
				   unsigned char *rlebuf) // temporary buffer for compressed data, 2 x rb in size
{
	psd_pixels_t n = 0, rlebytes;
	psd_bytes_t pos;
	int seekres = 0;

	switch(chan->comptype){
	case RAWDATA: /* uncompressed */
		if(chan->rawpos){
			pos = chan->rawpos + chan->rowbytes*row;
			seekres = fseeko(psd, pos, SEEK_SET);
			if(seekres != -1)
				n = fread(inrow, 1, chan->rowbytes, psd);
		}else{
			warn_msg("# readunpackrow() called for raw data, but rawpos is zero");
		}
		break;
	case RLECOMP:
		if(chan->rowpos){
			pos = chan->rowpos[row];
			seekres = fseeko(psd, pos, SEEK_SET);
			if(seekres != -1){
				rlebytes = fread(rlebuf, 1, chan->rowpos[row+1] - pos, psd);
				n = unpackbits(inrow, rlebuf, chan->rowbytes, rlebytes);
			}
		}else{
			warn_msg("# readunpackrow() called for RLE data, but rowpos is NULL");
		}
		break;
	case ZIPNOPREDICT:
	case ZIPPREDICT:
		if(chan->unzipdata)
			memcpy(inrow, chan->unzipdata + chan->rowbytes*row, chan->rowbytes);
		else
			warn_msg("# readunpackrow() called for ZIP data, but unzipdata is NULL");
		return;
	}
	// if we don't recognise the compression type, skip the row
	// FIXME: or would it be better to use the last valid type seen?

	if(seekres == -1)
		alwayswarn("# can't seek to " LL_L("%lld\n","%ld\n"), pos);

	if(n < chan->rowbytes){
		warn_msg("row data short (wanted %d, got %d bytes)", chan->rowbytes, n);
		// zero out unwritten part of row
		memset(inrow + n, 0xff, chan->rowbytes - n);
	}
}

// Read channel metadata and populate the chan[] struct
// in preparation for later reading/decompression of image data.
// Called individually for layer channels (channels always == 1), and
// once for entire merged data (channels == PSD header channel count).

const char *comptype[] = {"raw", "RLE", "ZIP without prediction", "ZIP with prediction"};

void dochannel(psd_file_t f,
			   struct layer_info *li,
			   struct channel_info *chan, // array of channel info
			   int channels, // how many channels are to be processed (>1 only for merged data)
			   struct psd_header *h)
{
	int compr, ch;
	psd_bytes_t chpos, pos;
	unsigned char *zipdata;
	psd_pixels_t count, last, j, rb;

	chpos = (psd_bytes_t)ftello(f);

	if(li){
		VERBOSE(">>> channel id = %2d @ " LL_L("%7lld, %lld","%7ld, %ld") " bytes\n",
				chan->id, chpos, chan->length);

		// If this is a layer mask, the pixel size is a special case
		if(chan->id == LMASK_CHAN_ID){
			chan->rows = li->mask.bottom - li->mask.top;
			chan->cols = li->mask.right - li->mask.left;
			VERBOSE("# layer mask (%4d,%4d,%4d,%4d) (%4u rows x %4u cols)\n",
					li->mask.top, li->mask.left,
					li->mask.bottom, li->mask.right, chan->rows, chan->cols);
		}else if(chan->id == UMASK_CHAN_ID){
			chan->rows = li->mask.real_bottom - li->mask.real_top;
			chan->cols = li->mask.real_right - li->mask.real_left;
			VERBOSE("# user layer mask (%4d,%4d,%4d,%4d) (%4u rows x %4u cols)\n",
					li->mask.real_top, li->mask.real_left,
					li->mask.real_bottom, li->mask.real_right,
					chan->rows, chan->cols);
		}else{
			// channel has dimensions of the layer
			chan->rows = li->bottom - li->top;
			chan->cols = li->right - li->left;
		}
	}else{
		// merged image, has dimensions of PSD
		VERBOSE(">>> merged image data @ " LL_L("%7lld\n","%7ld\n"), chpos);
		chan->rows = h->rows;
		chan->cols = h->cols;
	}

	// Compute image row bytes
	rb = ((long)chan->cols*h->depth + 7)/8;

	// Read compression type
	compr = get2Bu(f);

	if(compr >= RAWDATA && compr <= ZIPPREDICT){
		VERBOSE("    compression = %d (%s)\n", compr, comptype[compr]);
	}
	VERBOSE("    uncompressed size %u bytes (row bytes = %u)\n",
			channels*chan->rows*rb, rb);

	// Prepare compressed data for later access:

	pos = chpos + 2;

	// skip RLE counts, leave pos pointing to first row's compressed data
	if(compr == RLECOMP)
		pos += (channels*chan->rows) << h->version;

	for(ch = 0; ch < channels; ++ch){
		if(!li){
			// if required, identify first alpha channel as merged data transparency
			chan[ch].id = h->mergedalpha && ch == mode_channel_count[h->mode]
			                  ? TRANS_CHAN_ID : ch;
		}
		chan[ch].rowbytes = rb;
		chan[ch].comptype = compr;
		chan[ch].rows = chan->rows;
		chan[ch].cols = chan->cols;
		chan[ch].rowpos = NULL;
		chan[ch].unzipdata = NULL;
		chan[ch].rawpos = 0;

		if(!chan->rows)
			continue;

		// For RLE, we read the row count array and compute file positions.
		// For ZIP, read and decompress whole channel.
		switch(compr){
		case RAWDATA:
			chan[ch].rawpos = pos;
			pos += chan->rowbytes*chan->rows;
			break;

		case RLECOMP:
			/* accumulate RLE counts, to make array of row start positions */
			chan[ch].rowpos = checkmalloc((chan[ch].rows+1)*sizeof(psd_bytes_t));
			last = chan[ch].rowbytes;
			for(j = 0; j < chan[ch].rows && !feof(f); ++j){
				count = h->version==1 ? get2Bu(f) : (psd_pixels_t)get4B(f);

				if(count < 2 || count > 2*chan[ch].rowbytes)  // this would be impossible
					count = last; // make a guess, to help recover

				last = count;
				chan[ch].rowpos[j] = pos;
				pos += count;
			}
			if(j < chan[ch].rows){
				fatal("# couldn't read RLE counts");
			}
			chan[ch].rowpos[j] = pos; /* = end of last row */
			break;

		case ZIPNOPREDICT:
		case ZIPPREDICT:
			if(li){
				pos += chan->length - 2;

				zipdata = checkmalloc(chan->length);
				count = fread(zipdata, 1, chan->length - 2, f);
				if(count < chan->length - 2)
					alwayswarn("ZIP data short: wanted %ld bytes, got %ld", chan->length, count);

				chan->unzipdata = checkmalloc(chan->rows*chan->rowbytes);
				if(compr == ZIPNOPREDICT)
					psd_unzip_without_prediction(zipdata, count, chan->unzipdata,
												 chan->rows*chan->rowbytes);
				else
					psd_unzip_with_prediction(zipdata, count, chan->unzipdata,
											  chan->rows*chan->rowbytes,
											  chan->cols, h->depth);

				free(zipdata);
			}else{
				alwayswarn("## can't process ZIP outside layer\n");
			}
			break;

		default:
			if(li)
				alwayswarn("## bad compression type: %d; skipping channel (id %2d) in layer \"%s\"\n",
						   compr, chan->id, li->name);
			else
				alwayswarn("## bad compression type: %d; skipping channel\n", compr);

			if(li)
				fseeko(f, chan->length - 2, SEEK_CUR);
			break;
		}
	}

	if(li && pos != chpos + chan->length)
		alwayswarn("# channel data is %lu bytes, but length = %lu\n",
				   (unsigned long)(pos - chpos), (unsigned long)chan->length);

	fseeko(f, pos, SEEK_SET);
}
