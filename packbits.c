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

#include <string.h>

#include "psdparse.h"

// Assuming compressor logic is maximally efficient,
// worst case input with no duplicate runs of 3 or more bytes
// will be compressed into a series of verbatim runs no longer
// than 128 bytes, each preceded by length byte.
// i.e. worst case output length is not more than 129*ceil(n/128)
// or slightly tighter, 129*floor(n/128) + 1 + (n%128)

psd_pixels_t packbits(unsigned char *src, unsigned char *dst, psd_pixels_t n){
	unsigned char *p, *q, *run, *dataend;
	int count, maxrun;

	dataend = src + n;
	for( run = src, q = dst; n > 0; run = p, n -= count ){
		// A run cannot be longer than 128 bytes.
		maxrun = n < 128 ? n : 128;
		if(run <= (dataend-3) && run[1] == run[0] && run[2] == run[0]){
			// 'run' points to at least three duplicated values.
			// Step forward until run length limit, end of input,
			// or a non matching byte:
			for( p = run+3; p < (run+maxrun) && *p == run[0]; )
				++p;
			count = p - run;
			// replace this run in output with two bytes:
			*q++ = 1+256-count; /* flag byte, which encodes count (129..254) */
			*q++ = run[0];      /* byte value that is duplicated */
		}else{
			// If the input doesn't begin with at least 3 duplicated values,
			// then copy the input block, up to the run length limit,
			// end of input, or until we see three duplicated values:
			for( p = run; p < (run+maxrun); )
				if(p <= (dataend-3) && p[1] == p[0] && p[2] == p[0])
					break; // 3 bytes repeated end verbatim run
				else
					++p;
			count = p - run;
			*q++ = count-1;        /* flag byte, which encodes count (0..127) */
			memcpy(q, run, count); /* followed by the bytes in the run */
			q += count;
		}
	}
	return q - dst;
}
