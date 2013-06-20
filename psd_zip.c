/**
 * libpsd - Photoshop file formats (*.psd) decode library
 * Copyright (C) 2004-2007 Graphest Software.
 *
 * libpsd is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: psd_zip.c, created by Patrick in 2007.02.02, libpsd@graphest.com Exp $
 */

// adapted from code in libpsd 0.9
// modifications Copyright (C) Toby Thain <toby@telegraphics.com.au>

#include "psdparse.h"

#ifdef HAVE_ZLIB_H
	#include "zlib.h"
#endif

psd_status psd_unzip_without_prediction(psd_uchar *src_buf, psd_int src_len, 
	psd_uchar *dst_buf, psd_int dst_len)
{
#ifdef HAVE_ZLIB_H
	z_stream stream;
	psd_int state;

	memset(&stream, 0, sizeof(z_stream));
	stream.data_type = Z_BINARY;

	stream.next_in = (Bytef *)src_buf;
	stream.avail_in = src_len;
	stream.next_out = (Bytef *)dst_buf;
	stream.avail_out = dst_len;

	if(inflateInit(&stream) != Z_OK)
		return 0;
	
	do {
		state = inflate(&stream, Z_PARTIAL_FLUSH);
		if(state == Z_STREAM_END)
			break;
		if(state == Z_DATA_ERROR || state != Z_OK)
			break;
	}  while (stream.avail_out > 0);

	if (state != Z_STREAM_END && state != Z_OK)
		return 0;

	return 1;
#endif
	return 0;
}

psd_status psd_unzip_with_prediction(psd_uchar *src_buf, psd_int src_len, 
	psd_uchar *dst_buf, psd_int dst_len, 
	psd_int row_size, psd_int color_depth)
{
#ifdef HAVE_ZLIB_H
	psd_status status;
	int len;
	psd_uchar * buf;

	status = psd_unzip_without_prediction(src_buf, src_len, dst_buf, dst_len);
	if(!status)
		return status;
	
	buf = dst_buf;
	do {
		len = row_size;
		if (color_depth == 16)
		{
			while(--len)
			{
				buf[2] += buf[0] + ((buf[1] + buf[3]) >> 8);
				buf[3] += buf[1];
				buf += 2;
			}
			buf += 2;
			dst_len -= row_size * 2;
		}
		else
		{
			while(--len)
			{
				*(buf + 1) += *buf;
				buf ++;
			}
			buf ++;
			dst_len -= row_size;
		}
	} while(dst_len > 0);

	return 1;
#endif
	return 0;
}
