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
#include <stddef.h>

#include "psdparse.h"

#define PROP_COLORMAP     1
#define PROP_COMPRESSION 17
#define PROP_GUIDES      18
#define PROP_RESOLUTION  19
#define PROP_PATHS       23
#define PROP_VECTORS     25
#define PROP_END          0
#define PROP_OPACITY      6
#define PROP_VISIBLE      8
#define PROP_MODE         7
#define PROP_OFFSETS     15
#define PROP_PRESERVE_TRANSPARENCY 10
#define PROP_APPLY_MASK  11

FILE *xcf_open(char *psd_name, struct psd_header *h);

size_t put4xcf(FILE *f, uint32_t v);
size_t putfxcf(FILE *f, float v);
size_t putsxcf(FILE *f, char *s);

void xcf_prop_colormap(FILE *xcf, FILE *psd, struct psd_header *h);
void xcf_prop_compression(FILE *xcf, int compr);
void xcf_prop_resolution(FILE *xcf, float x_per_cm, float y_per_cm);
void xcf_prop_mode(FILE *xcf, int m);
void xcf_prop_offsets(FILE *xcf, int dx, int dy);
void xcf_prop_opacity(FILE *xcf, int op);
void xcf_prop_end(FILE *xcf);

size_t xcf_rle(FILE *xcf, unsigned char *input, size_t n);
off_t xcf_level(FILE *xcf, FILE *psd, int w, int h, int channel_cnt,
				struct channel_info *xcf_chan[], int compr);
off_t xcf_hierarchy(FILE *xcf, FILE *psd, int w, int h, int channel_cnt,
					struct channel_info *xcf_chan[], int compr);
off_t xcf_channel(FILE *xcf, FILE *psd, int w, int h, char *name, int visible,
				  struct channel_info *chan, int compr);
off_t xcf_layer(FILE *xcf, FILE *psd, struct layer_info *li, int compr);
