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

/* quick hack. please don't take this too seriously */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <sys/errno.h>

#include "png.h"

#ifdef HAVE_ZLIB_H
	#include "zlib.h"
#endif

#define HEADER_BYTES 8
#define MB(x) ((x) << 20)
#define OVERSAMPLE_RATE 2.0

// This was very helpful in understanding Lanczos kernel and convolution:
// http://stackoverflow.com/questions/943781/where-can-i-find-a-good-read-about-bicubic-interpolation-and-lanczos-resampling/946943#946943

int clamp(int vv){
	if(vv < 0)
		return 0;
	else if(vv > 255)
		return 255;
	return vv;
}

double sinc(double x){ return sin(x)/x; }

// build 1-D kernel
// conv: array of 2n+1 floats (-n .. 0 .. +n)
void lanczos_1d(double *conv, unsigned n, double fac){
	unsigned i;

	// fac is the rate of kernel taps.
	// at 1.0, there is a single non-zero coefficient.
	conv[n] = 1.;
	for(i = 1; i <= n; ++i){
		double x = i*M_PI / fac;
		conv[n+i] = conv[n-i] = sinc(x)*sinc(x/n);
	}
	/*for(i = 0; i <= 2*n; ++i)
		printf("%.2g  ", conv[i]);
	putchar('\n');*/
}

void lanczos_decim( png_bytep *row_ptrs, // input image: array of row pointers
					int in_planes,		 // bytes separating input planes;
										 // allows for interleaved input
					int plane_index,
					int in_w,			 // image columns
					int in_h,			 // image rows
					unsigned char *out,	 // pointer to output buffer (always one plane)
					int out_w,			 // output columns
					int out_h,			 // output rows
					double scale )		 // ratio of input to output (> 1.0)
{
	double *conv, *knl, sum, weight;
	float *t_buf, *t_row, *t_col;
	int x, y, i, j, k, KERNEL_SIZE = floor(2*scale*OVERSAMPLE_RATE);
	unsigned char *in_row, *out_row;
	
	conv = malloc((2*KERNEL_SIZE+1)*sizeof(double));

	lanczos_1d(conv, KERNEL_SIZE, scale*OVERSAMPLE_RATE);
	knl = conv + KERNEL_SIZE; // zero index of kernel

	// apply horizontal convolution, interpolating input to output
	// store this in a temporary buffer, with same row count as input

	t_buf = malloc(sizeof(*t_buf) * in_h * out_w);

	for(j = 0, t_row = t_buf;  j < in_h;  ++j, t_row += out_w){
		in_row = row_ptrs[j] + plane_index;
		for(i = 0; i < out_w; ++i){
			// find involved input pixels. multiply each by corresponding
			// kernel coefficient.

			sum = weight = 0.;
			// step over all covered input samples
			for(k = -KERNEL_SIZE; k <= KERNEL_SIZE; ++k){
				x = floor((i+.5)*scale + k/OVERSAMPLE_RATE);
				if(x >= 0 && x < in_w){
					sum += knl[k]*in_row[x*in_planes];
					weight += knl[k];
				}
			}
			t_row[i] = sum/weight;
		}
	}

	// interpolate vertically
	for(i = 0; i < out_w; ++i){
		t_col = t_buf + i;
		out_row = out + i;
		for(j = 0; j < out_h; ++j){
			// find involved input pixels. multiply each by corresponding
			// kernel coefficient.

			sum = weight = 0.;
			// step over all covered input samples
			for(k = -KERNEL_SIZE; k <= KERNEL_SIZE; ++k){
				y = floor((j+.5)*scale + k/OVERSAMPLE_RATE);
				if(y >= 0 && y < in_h){
					sum += knl[k]*t_col[y*out_w];
					weight += knl[k];
				}
			}
			out_row[0] = clamp(sum/weight + 0.5);
			out_row += out_w; // step down one row
		}
	}

	free(t_buf);
	free(conv);
}

int main(int argc, char *argv[]){
	static png_uint_32 new_width, new_height;
	static int coltype[] = {0, PNG_COLOR_TYPE_GRAY, PNG_COLOR_TYPE_GRAY_ALPHA,
							PNG_COLOR_TYPE_RGB, PNG_COLOR_TYPE_RGB_ALPHA};
	FILE *in_file, *out_file;
	unsigned char header[HEADER_BYTES];
	char out_name[0x100];
	png_structp png_ptr, wpng_ptr;
	png_infop info_ptr, end_info, winfo_ptr;
	png_bytep *row_ptrs, out_plane[4], row;
	png_uint_32 width, height, max_pixels, i, j;
	int bit_depth, color_type, interlace, compression,
		filter, channels, rowbytes, larger_dimension, k;
	
	if(argc <= 3){
		fprintf(stderr,
"usage:	 %s source_filename dest_filename max_pixels\n\
         Writes a new PNG with height and width scaled to max_pixels.\n\
         If image is already no larger than max_pixels, makes a hard link\n\
         to original file.\n", argv[0]);
		return 2;
	}
	
	in_file = fopen(argv[1], "rb");
	if (!in_file){
		fputs("# cannot open input file\n", stderr);
		return EXIT_FAILURE;
	}

	if (fread(header, 1, HEADER_BYTES, in_file) < HEADER_BYTES
		|| png_sig_cmp(header, 0, HEADER_BYTES))
	{
		fputs("# not a PNG file\n", stderr);
		return EXIT_FAILURE;
	}

	if (!(png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))
	 || !(info_ptr = png_create_info_struct(png_ptr))
	 || !(end_info = png_create_info_struct(png_ptr)))
	{
		fputs("# png_create_read_struct or png_create_info_struct failed\n", stderr);
		return EXIT_FAILURE;
	}
	
	if (setjmp(png_jmpbuf(png_ptr))){
		fputs("# libpng failed to read the file\n", stderr);
		return EXIT_FAILURE;
	}

	png_init_io(png_ptr, in_file);
	png_set_sig_bytes(png_ptr, HEADER_BYTES);

	png_read_info(png_ptr, info_ptr);
	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth,
				 &color_type, &interlace, &compression, &filter);

	larger_dimension = width > height ? width : height;
	max_pixels = atoi(argv[3]);
	new_width = max_pixels*width/larger_dimension;
	new_height = max_pixels*height/larger_dimension;
	printf("new width: %u  new height: %u\n", new_width, new_height);

	printf("width: %u  height: %u  bit_depth: %d  color_type: %d\n",
		   width, height, bit_depth, color_type);
	if(width <= max_pixels && height <= max_pixels){
		new_width = width;
		new_height = height;
		sprintf(out_name, "%s_%u_%u.png", argv[2], new_width, new_height);
		if(link(argv[1], out_name) == 0){
			printf("hard link created\n");
			return EXIT_SUCCESS;
		}else if(errno != EXDEV){
			fprintf(stderr, "# failed to create hard link (%d)\n", errno);
			return EXIT_FAILURE;
		}
	}else{
		// otherwise write resized file
		sprintf(out_name, "%s_%u_%u.png", argv[2], new_width, new_height);
	}

	if(interlace != PNG_INTERLACE_NONE){
		fputs("# interlace not supported\n", stderr);
		return EXIT_FAILURE;
	}

	printf("file channels: %u  file rowbytes: %zu\n",
		   png_get_channels(png_ptr, info_ptr),
		   png_get_rowbytes(png_ptr, info_ptr));

	// define transformations
	if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png_ptr);
	else if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_expand_gray_1_2_4_to_8(png_ptr);
	if(bit_depth == 16)
		png_set_strip_16(png_ptr);
	png_read_update_info(png_ptr, info_ptr);

	channels = png_get_channels(png_ptr, info_ptr);
	rowbytes = png_get_rowbytes(png_ptr, info_ptr);
	printf("transformed channels: %d  rowbytes: %d\n", channels, rowbytes);
		
// -------------- read source image --------------
	row_ptrs = calloc(height, sizeof(png_bytep));
	for(i = 0; i < height; ++i)
		row_ptrs[i] = malloc(width*channels);
	png_read_image(png_ptr, row_ptrs);

// -------------- filter image --------------
	for(k = 0; k < channels; ++k){
		out_plane[k] = malloc(new_width*new_height);
		lanczos_decim(row_ptrs, channels, k /* plane index */, width, height,
					  out_plane[k], new_width, new_height,
					  (double)larger_dimension/max_pixels);
	}
	
// -------------- write output image --------------
	if (!(out_file = fopen(out_name, "wb")))
	{
		fputs("# could not open output file\n", stderr);
		return EXIT_FAILURE;
	}
	if(!(wpng_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))
	|| !(winfo_ptr = png_create_info_struct(wpng_ptr)))
	{
		fputs("# png_create_write_struct or png_create_info_struct failed\n", stderr);
		return EXIT_FAILURE;
	}

	if (setjmp(png_jmpbuf(wpng_ptr))){
		fputs("# libpng failed to write the file\n", stderr);
		return EXIT_FAILURE;
	}
	
	png_init_io(wpng_ptr, out_file);
	png_set_IHDR(wpng_ptr, winfo_ptr, new_width, new_height, 8, coltype[channels],
				 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	png_write_info(wpng_ptr, winfo_ptr);
	
	row = malloc(channels*new_width);
	for(j = 0; j < new_height; ++j){
		// interleave channels from the separate planes
		for(i = 0; i < new_width; ++i)
			for(k = 0; k < channels; ++k)
				row[i*channels + k] = out_plane[k][j*new_width + i];
		// write one interleaved row
		png_write_row(wpng_ptr, row);
	}
	
	png_write_end(wpng_ptr, winfo_ptr);

	return EXIT_SUCCESS;
}
