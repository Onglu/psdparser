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

const char *mode_names[] = {
	"Bitmap", "GrayScale", "IndexedColor", "RGBColor",
	"CMYKColor", "HSLColor", "HSBColor", "Multichannel",
	"Duotone", "LabColor", "Gray16", "RGB48",
	"Lab48", "CMYK64", "DeepMultichannel", "Duotone16"
};

const char *channelsuffixes[] = {
	"", "", "", "RGB",
	"CMYK", "HSL", "HSB", "",
	"", "Lab", "", "RGB",
	"Lab", "CMYK", "", ""
};

// The number of channels in each mode that contain image data.
const int mode_channel_count[] = {
	1, 1, 1, 3, 4, 3, 3, 0, 1, 3, 1, 3, 3, 4, 0, 1
};

const int mode_colour_space[] = {
	8, 8, 0, 0, 2, -1, -1, 8, 14, 7, 8, 0, 7, 2, 8, 14
};

/* This colour space numbering is used by various metadata, including
 * image resources. For example, the colour space used by a colour sampler
 * is from this set, as are spot/alpha channel ink/color descriptions.
 */
struct colour_space colour_spaces[] = {
	{-1, "Dummy", NULL /* Use NULL if colour components should not be parsed at all */ },
	{ 0, "RGB", "RGB"},
	{ 1, "HSB", "HSB"},
	{ 2, "CMYK", "CMYK"},
	{ 3, "Pantone", "*" /* Use "*" if it's probably a readable string */ },
	{ 4, "Focoltone", "*"},
	{ 5, "Trumatch", "*"},
	{ 6, "Toyo", "*"},
	{ 7, "Lab", "Lab"},
	{ 8, "Gray", "G"},
	{ 9, "WideCMYK", "CMYK"},
	{10, "HKS", "*"},
	{11, "DIC", "*"},
	{12, "TotalInk", "T"},
	{13, "MonitorRGB", "RGB"},
	{14, "Duotone", "D"},
	{15, "Opacity", "O"},
	// following are spaces used by Photoshop's default Colour Libraries
	{3000, "ANPAColor", "*"},
	{3031, "PantoneColorBridgeCMYKEC", "*"},
	{3030, "PantoneColorBridgeCMYKPC", "*"},
	{3045, "PantoneColorBridgeCMYKUP", "*"},
	{3022, "PantoneMetallicCoated", "*"},
	{3020, "PantonePastelCoated", "*"},
	{3021, "PantonePastelUncoated", "*"},
	{3029, "PantoneProcessUncoated", "*"},
	{3013, "PantoneSolidMatte", "*"},
	{3028, "PantoneSolidToProcessCoatedEuro", "*"},
	{3032, "ToyoColorFinder", "*"},
	// following are from Color Books list (File Formats doc)
	{3001, "Focoltone", "*"},
	{3002, "PantoneCoated", "*"},
	{3003, "PantoneProcess", "*"},
	{3004, "PantoneProSlim", "*"},
	{3005, "PantoneUncoated", "*"},
	{3006, "Toyo", "*"},
	{3007, "Trumatch", "*"},
	{3008, "HKSE", "*"},
	{3009, "HKSK", "*"},
	{3010, "HKSN", "*"},
	{3011, "HKSZ", "*"},
	{3012, "DIC", "*"},
	{0, NULL, NULL}
};
