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

#ifdef HAVE_SYS_MMAN_H

#include <sys/mman.h>

#include "psdparse.h"

void *map_file(int fd, size_t len)
{
	void *addr = mmap(NULL, len, PROT_READ, MAP_FILE|MAP_SHARED, fd, 0);
	return addr != MAP_FAILED ? addr : NULL;
}

void unmap_file(void *addr, size_t len)
{
	if(addr) munmap(addr, len);
}

#endif
