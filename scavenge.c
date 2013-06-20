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

#ifdef HAVE_ZLIB_H
	#include "zlib.h"
#endif

#include "psdparse.h"

extern struct dictentry bmdict[];

extern int scavenge, scavenge_rle;

// Search for layer signatures which appear to be followed by valid
// layer metadata.
// If li is not NULL, update this array with the layer metadata.
// Return a count of all possible layers found.

unsigned scan(unsigned char *addr, size_t len, int psd_version, struct layer_info *li)
{
	unsigned char *p = addr, *q;
	size_t i;
	int j, k;
	unsigned n = 0, ps_ptr_bytes = 2 << psd_version;
	struct dictentry *de;

	for(i = 0; i < len;)
	{
		if(KEYMATCH((char*)p+i, "8BIM")){
			i += 4;

			// found possible layer signature
			// check next 4 bytes for a known blend mode
			for(de = bmdict; de->key; ++de)
				if(!memcmp(de->key, (char*)p+i, 4)){
					// found a possible layer blendmode signature
					// try to guess number of channels
					for(j = 1; j < 64; ++j){
						q = p + i - 4 - j*(ps_ptr_bytes + 2) - 2;
						if(peek2B(q) == j){
							long t = peek4B(q-16), l = peek4B(q-12), b = peek4B(q-8), r = peek4B(q-4);
							// sanity check bounding box
							if(b >= t && r >= l){
								// sanity check channel ids
								for(k = 0; k < j; ++k){
									int chid = peek2B(q + 2 + k*(ps_ptr_bytes + 2));
									if(chid < -2 || chid >= j)
										break; // smells bad, give up
								}
								if(k == j){
									// channel ids were ok. could still be a valid guess...
									++n;
									if(li){
										li->filepos = q - p - 16;
										++li;
									}
									else
										VERBOSE("scavenge @ %8td : key: %c%c%c%c  could be %d channel layer: t = %ld, l = %ld, b = %ld, r = %ld\n",
											   q - p - 16,
											   de->key[0], de->key[1], de->key[2], de->key[3],
											   j,
											   t, l, b, r);
									break;
								}
							}
						}
					}

					break;
				}
		}else
			++i;
	}
	return n;
}

int is_resource(unsigned char *addr, size_t len, size_t offset)
{
	int namelen;
	long size;

	if(KEYMATCH(addr + offset, "8BIM")){
		offset += 4; // type
		offset += 2; // id
		namelen = addr[offset];
		offset += PAD2(1+namelen);
		size = peek4B(addr+offset);
		offset += 4;
		offset += PAD2(size); // skip resource block data
		return offset; // should be offset of next resource
	}
	return 0;
}

// if no valid layer signature was found, then look for an empty layer/mask info block
// which would imply only merged data is in the file (no layers)

void scan_merged(unsigned char *addr, size_t len, struct psd_header *h)
{
	size_t i, j = 0;
	unsigned ps_ptr_bytes = 2 << h->version;

	h->lmistart = h->lmilen = 0;

	// first search for a valid image resource block (these precede layer/mask info)
	for(i = 0; i < len;)
	{
		j = is_resource(addr, len, i);
		if(j && j < len-4){
			VERBOSE("scavenge: possible resource id=%d @ %lu\n", peek2B(addr+i+4), (unsigned long)i);
			i = j; // found an apparently valid resource; skip over it

			// is it followed by another resource?
			if(memcmp("8BIM", addr+j, 4))
				break; // no - stop looking
		}else
			++i;
	}

	if(!j)
		alwayswarn("Did not find any plausible image resources; probably cannot locate merged image data.\n");

	for(i = j; i < len; ++i)
	{
		psd_bytes_t lmilen = h->version == 2 ? peek8B(addr+i) : peek4B(addr+i),
				  layerlen = h->version == 2 ? peek8B(addr+i+ps_ptr_bytes) : peek4B(addr+i+ps_ptr_bytes);
		if(lmilen > 0 && (i+lmilen) < len && layerlen == 0)
		{
			// sanity check compression type
			int comptype = peek2Bu(addr + i + lmilen);
			if(comptype == 0 || comptype == 1)
			{
				h->lmistart = i+ps_ptr_bytes;
				h->lmilen = lmilen;
				VERBOSE("scavenge: possible empty LMI @ %lld\n", h->lmistart);
				UNQUIET(
"May be able to recover merged image if you can provide correct values\n\
for --mergedrows, --mergedcols, --mergedchan, --depth and --mode.\n");
				break; // take first one
			}
		}
	}
}

// Given a starting pointer, determine if it can be inflate'd as ZIP data
// into a buffer of the expected uncompressed size.
// Return the compressed byte count if the data inflates without errors,
// otherwise return NULL.

size_t try_inflate(unsigned char *src_buf, size_t src_len,
				   unsigned char *dst_buf, size_t dst_len)
{
#ifdef HAVE_ZLIB_H
	z_stream stream;
	int state;

	memset(&stream, 0, sizeof(z_stream));
	stream.data_type = Z_BINARY;

	stream.next_in = src_buf;
	stream.avail_in = src_len;
	stream.next_out = dst_buf;
	stream.avail_out = dst_len;

	if(inflateInit(&stream) == Z_OK) {
		do {
			state = inflate(&stream, Z_FINISH);
			//fprintf(stderr,"inflate: state=%d next_in=%p avail_in=%d next_out=%p avail_out=%d\n",
			//		state, stream.next_in, stream.avail_in, stream.next_out, stream.avail_out);
			if(state == Z_STREAM_END)
				return stream.next_in - src_buf;
		}  while (state == Z_OK && stream.avail_out > 0 && stream.avail_in > 0);
	}
#endif
	return 0;
}

// For each layer we know about, search for possible channel data
// based on pixel dimensions (also using compression type).
// If a complete set of channels is found, store chpos to indicate this.

void scan_channels(unsigned char *addr, size_t len, struct psd_header *h)
{
	int i, j, c, rows, cols, comp, nextcomp, countbytes = 1 << h->version;
	struct layer_info *li = h->linfo;
	size_t n, lastpos = h->layerdatapos, pos, p, count, uncompsize;
	unsigned char *buf;

	UNQUIET("scan_channels(): starting @ %lu\n", (unsigned long)lastpos);

	for(i = 0; i < h->nlayers; ++i)
	{
		UNQUIET("scan_channels(): layer %d, channels: %d\n", i, li[i].channels);

		li[i].chpos = 0;
		rows = li[i].bottom - li[i].top;
		cols = li[i].right  - li[i].left;
		if(rows && cols)
		{
			// scan forward for compression type value
			for(pos = lastpos; pos < len-2; ++pos)
			{
				p = pos;
				for(c = 0; c < li[i].channels && p < len-2; ++c)
				{
					//fprintf(stderr,"scavenge_channels(): layer=%d ch=%d p=%lu id=%d length=%lld r%ld x c%ld rb=%ld\n",
					//		i,c,p,li[i].chan[c].id,li[i].chan[c].length,li[i].chan[c].rows,li[i].chan[c].cols,li[i].chan[c].rowbytes);

					comp = peek2Bu(addr+p);
					p += 2;
					uncompsize = li[i].chan[c].rows*li[i].chan[c].rowbytes;
					switch(comp)
					{
					case RAWDATA:
						if(p > len-uncompsize-2)
							goto mismatch;
						nextcomp = peek2Bu(addr + p + uncompsize);
						if(nextcomp >= RAWDATA && nextcomp <= ZIPPREDICT){
							count = uncompsize;
							//VERBOSE("layer %d channel %d: possible RAW data @ %lu\n", i, c, p);
						}else
							goto mismatch;
						break;

					case RLECOMP:
						count = 0;
						for(j = li[i].chan[c].rows; j--; p += countbytes){ // assume PSD for now
							if(p > len-countbytes)
								goto mismatch;
							n = h->version == 1 ? peek2Bu(addr+p) : (size_t)peek4B(addr+p);
							if(n >= 2 && n <= li[i].chan[c].rowbytes*2){
								count += n;
								//VERBOSE("layer %d channel %d: possible RLE data @ %lu\n", i, c, p);
							}else
								goto mismatch; // bad RLE count
						}
						break;

					case ZIPNOPREDICT:
					case ZIPPREDICT:
						//fprintf(stderr,"ZIP comp type seen... rows=%d rb=%d\n",rows,rb);
						buf = checkmalloc(uncompsize);
						count = try_inflate(addr+p, len-p, buf, uncompsize);
						free(buf);
						if(count)
							break;

					default:
						goto mismatch;
					}

					// Likely channel data for this layer was found.
					p += count;
				}

				if(c == li[i].channels)
				{
					// All channels found. Store location in linfo[]
					// and allocate array for channel data.

					UNQUIET("scan_channels(): layer %d may be @ %7lu\n", i, (unsigned long)pos);
					li[i].chpos = pos;
					lastpos = p; // step past it

					goto next_layer;
				}
mismatch:
				;
			}
		}
next_layer:
		;
	}
}

unsigned scavenge_psd(void *addr, size_t st_size, struct psd_header *h)
{
	// Pass 1: count possible layers
	h->nlayers = scan(addr, st_size, h->version, NULL);
	if(h->nlayers){
		// Pass 2: store layer information in linfo array.
		if( (h->linfo = checkmalloc(h->nlayers*sizeof(struct layer_info))) )
			scan(addr, st_size, h->version, h->linfo);
	}
	else
		scan_merged(addr, st_size, h);

	if(h->nlayers){
		UNQUIET("scavenge: possible layers (PS%c): %d\n", h->version == 2 ? 'B' : 'D', h->nlayers);
	}else
		alwayswarn("Did not find any plausible layer signatures (flattened file?)\n");

	return h->nlayers;
}
