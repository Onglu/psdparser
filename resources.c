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

#include "psdparse.h"

static void ir_resolution(psd_file_t f, int level, int len, struct dictentry *parent){
	double hresd, vresd;
	extern uint32_t hres, vres; // fixed point values

	hresd = FIXEDPT(hres = get4B(f));
	get2B(f);
	get2B(f);
	vresd = FIXEDPT(vres = get4B(f));
	if(xml) fprintf(xml, " <H>%g</H> <V>%g</V> ", hresd, vresd);
	UNQUIET("    Resolution %g x %g pixels per inch\n", hresd, vresd);
}

#define BYTESPERLINE 16

void dumphex(unsigned char *data, long size){
	unsigned char *p;
	long j, n;

	for(p = data; size; p += BYTESPERLINE, size -= n){
		fputs("    ", stdout);
		n = size > BYTESPERLINE ? BYTESPERLINE : size;
		for(j = 0; j < n; ++j)
			printf("%02x ", p[j]);
		for(; j < BYTESPERLINE+1; ++j)
			fputs("   ", stdout);
		for(j = 0; j < n; ++j)
			printf("%c", isprint(p[j]) ? p[j] : '.');
		putchar('\n');
	}
}

void ir_dump(psd_file_t f, int level, int len, struct dictentry *parent){
	unsigned char row[BYTESPERLINE];
	int n;

	if(verbose)
		for(; len; len -= n){
			n = len < BYTESPERLINE ? len : BYTESPERLINE;
			fread(row, 1, n, f);
			dumphex(row, n);
		}
	else
		fseeko(f, len, SEEK_CUR);
}

void ir_string(psd_file_t f, int level, int len, struct dictentry *parent){
	if(xml)
		while(len--)
			fputcxml(fgetc(f), xml);
}

// this should be used if the content is known to be valid XML.
// (does not check for invalid characters)
void ir_cdata(psd_file_t f, int level, int len, struct dictentry *parent){
	if(xml){
		fputs("<![CDATA[", xml);
		while(len--)
			fputc(fgetc(f), xml);
		fputs("]]>\n", xml);
	}
}

static void ir_pstring(psd_file_t f, int level, int len, struct dictentry *parent){
	if(xml)
		fputsxml(getpstr(f), xml);
}

static void ir_pstrings(psd_file_t f, int level, int len, struct dictentry *parent){
	char *s;

	for(; len > 1; len -= strlen(s)+1){
		// this loop will do the wrong thing
		// if any string contains NUL byte
		s = getpstr(f);
		if(xml){
			fprintf(xml, "%s<NAME>", tabs(level));
			fputsxml(s, xml);
			fputs("</NAME>\n", xml);
		}
		VERBOSE("    %s\n", s);
	}
}

static void ir_1byte(psd_file_t f, int level, int len, struct dictentry *parent){
	if(xml) fprintf(xml, "%d", fgetc(f));
}

static void ir_2byte(psd_file_t f, int level, int len, struct dictentry *parent){
	if(xml) fprintf(xml, "%d", get2B(f));
}

static void ir_4byte(psd_file_t f, int level, int len, struct dictentry *parent){
	if(xml) fprintf(xml, "%d", get4B(f));
}

static void ir_alphaids(psd_file_t f, int level, int len, struct dictentry *parent){
	if(xml){
		fprintf(xml, "%s<LENGTH>%d</LENGTH>\n", tabs(level), get4B(f));
		len -= 4;
		for(; len >= 4; len -= 4)
			fprintf(xml, "%s<ID>%d</ID>\n", tabs(level), get4B(f));
	}
}

static void ir_digest(psd_file_t f, int level, int len, struct dictentry *parent){
	if(xml)
		while(len--)
			fprintf(xml, "%02x", fgetc(f));
}

static void ir_pixelaspect(psd_file_t f, int level, int len, struct dictentry *parent){
	int v = get4B(f);
	double ratio = getdoubleB(f);
	if(xml) fprintf(xml, " <VERSION>%d</VERSION> <RATIO>%g</RATIO> ", v, ratio);
	UNQUIET("    (Version = %d, Ratio = %g)\n", v, ratio);
}

static void ir_unicodestr(psd_file_t f, int level, int len, struct dictentry *parent){
	if(xml) xml_unicodestr(f, get4B(f));
}

static void ir_unicodestrings(psd_file_t f, int level, int len, struct dictentry *parent){
	if(xml){
		int count;
		for(; len >= 4; len -= 4 + 2*count){
			count = get4B(f);
			fprintf(xml, "%s<NAME>", tabs(level));
			xml_unicodestr(f, count);
			fputs("</NAME>\n", xml);
		}
	}
}

static void ir_gridguides(psd_file_t f, int level, int len, struct dictentry *parent){
	const char *indent = tabs(level);
	long v = get4B(f), gv = get4B(f), gh = get4B(f), i, n = get4B(f);

	if(xml){
		fprintf(xml, "%s<VERSION>%ld</VERSION>\n", indent, v);
		// Note that these quantities are "document coordinates".
		// This is not documented, but appears to mean fixed point with 5 fraction bits,
		// so we divide by 32 to obtain pixel position.
		fprintf(xml, "%s<GRIDCYCLE> <V>%g</V> <H>%g</H> </GRIDCYCLE>\n",
				indent, gv/32., gh/32.);
		fprintf(xml, "%s<GUIDES>\n", indent);
		for(i = n; i--;){
			long ord = get4B(f);
			char c = fgetc(f) ? 'H' : 'V';
			fprintf(xml, "%s\t<%cGUIDE>%g</%cGUIDE>\n", indent, c, ord/32., c);
		}
		fprintf(xml, "%s</GUIDES>\n", indent);
	}
}

static void ir_layersgroup(psd_file_t f, int level, int len, struct dictentry *parent){
	for(; len >= 2; len -= 2)
	{
		if(xml)
		{
			fprintf(xml, "%s<GROUPID>%d</GROUPID>\n", tabs(level), get2B(f));
		}

		UNQUIET("%s(%d)<GROUPID>%d</GROUPID>\n", tabs(level), level, get2B(f));
	}
}

static void ir_layerselectionids(psd_file_t f, int level, int len, struct dictentry *parent){
	int count = get2B(f);
	if(xml)
		while(count--)
			fprintf(xml, "%s<ID>%d</ID>\n", tabs(level), get4B(f));
}

static void ir_printflags(psd_file_t f, int level, int len, struct dictentry *parent){
	const char *indent = tabs(level), **p, *flags[] = {
		"LABELS", "CROPMARKS", "COLORBARS", "REGMARKS", "NEGATIVE",
		"FLIP", "INTERPOLATE", "CAPTION", "PRINTFLAGS", NULL
	};
	if(xml){
		for(p = flags; *p; ++p)
			fprintf(xml, "%s<%s>%d</%s>\n", indent, *p, fgetc(f), *p);
	}
}

static void ir_printflags10k(psd_file_t f, int level, int len, struct dictentry *parent){
	const char *indent = tabs(level);
	if(xml){
		fprintf(xml, "%s<VERSION>%d</VERSION>\n", indent, get2B(f));
		fprintf(xml, "%s<CENTERCROPMARKS>%d</CENTERCROPMARKS>\n", indent, fgetc(f));
		fgetc(f);
		fprintf(xml, "%s<BLEEDWIDTH>%d</BLEEDWIDTH>\n", indent, get4B(f));
		fprintf(xml, "%s<BLEEDWIDTHSCALE>%d</BLEEDWIDTHSCALE>\n", indent, get2B(f));
	}
}

static void ir_colorsamplers(psd_file_t f, int level, int len, struct dictentry *parent){
	const char *indent = tabs(level);
	long v = get4B(f), n = get4B(f);
	union { float f; int32_t i; } x, y;
	int space;
	struct colour_space *sp;

	if(xml){
		fprintf(xml, "%s<VERSION>%ld</VERSION>\n", indent, v);
		while(n--){
			fprintf(xml, "%s<SAMPLER>\n", indent);
			if(v >= 3)
				fprintf(xml, "\t%s<VERSION>%d</VERSION>\n", indent, get2B(f)); // doc incorrectly says 4 byte
			y.i = get4B(f);
			x.i = get4B(f);
			if(v == 1)
				fprintf(xml, "\t%s<X>%g</X> <Y>%g</Y>\n", indent, x.i/32., y.i/32.); // undocumented fixed point factor
			else
				fprintf(xml, "\t%s<X>%g</X> <Y>%g</Y>\n", indent, x.f, y.f);
			space = get2B(f);
			if( (sp = find_colour_space(space)) )
				fprintf(xml, "\t%s<COLORSPACE> <%s/> </COLORSPACE>\n", indent, sp->name);
			else
				fprintf(xml, "\t%s<COLORSPACE>%d</COLORSPACE>\n", indent, space);
			if(v >= 2)
				fprintf(xml, "\t%s<DEPTH>%d</DEPTH>\n", indent, get2B(f));
			fprintf(xml, "%s</SAMPLER>\n", indent);
		}
	}
}

// Print XML for a DisplayInfo structure (see Photoshop API doc, Appendix A).

static void displayinfo(int level, unsigned char data[]){
	static const char *kind[] = {"ALPHACOLORSELECTED", "ALPHACOLORMASKED", "SPOTCHANNEL"};
	const char *indent = tabs(level);

	if(xml){
		fprintf(xml, "%s<%s>\n", indent, kind[data[12]]);
		colorspace(level+1, peek2B(data), data+2);
		fprintf(xml, "\t%s<OPACITY>%d</OPACITY>\n", indent, peek2B(data+10));
		// kind values seem to be:
		// 0 = alpha channel, colour indicates selected areas
		// 1 = alpha channel, colour indicates masked areas
		// 2 = spot colour channel
		//fprintf(xml, "\t%s<KIND>%d</KIND>\n", indent, data[12]);
		fprintf(xml, "%s</%s>\n", indent, kind[data[12]]);
	}
}

static void ir_displayinfo(psd_file_t f, int level, int len, struct dictentry *parent){
	if(xml){
		for(; len >= 14; len -= 14){
			unsigned char data[14];
			fread(data, 1, 14, f);
			displayinfo(level, data);
		}
	}
}

static void ir_displayinfocs3(psd_file_t f, int level, int len, struct dictentry *parent){
	if(xml){
		fprintf(xml, "%s<VERSION>%d</VERSION>\n", tabs(level), get4B(f));
		len -= 4;
		for(; len >= 13; len -= 13){
			unsigned char data[13];
			// it seems as if this resource doesn't use DisplayInfo padding
			// (this divergence from API description isn't mentioned in File Format doc)
			fread(data, 1, 13, f);
			displayinfo(level, data);
		}
	}
}

static void ir_altduotonecolors(psd_file_t f, int level, int len, struct dictentry *parent){
	const char *indent = tabs(level);
	unsigned i;

	if(xml){
		fprintf(xml, "%s<VERSION>%d</VERSION>\n", indent, get2B(f));
		for(i = get2B(f); i--;)
			ed_colorspace(f, level, len, parent);
		for(i = get2B(f); i--;){
			int L = fgetc(f), a = fgetc(f), b = fgetc(f);
			fprintf(xml, "%s<kLabSpace> <L>%d</L> <a>%d</a> <b>%d</b> </kLabSpace>\n",
					indent, L, a, b);
		}
	}
}

static void ir_altspotcolors(psd_file_t f, int level, int len, struct dictentry *parent){
	const char *indent = tabs(level);
	unsigned i;

	if(xml){
		fprintf(xml, "%s<VERSION>%d</VERSION>\n", indent, get2B(f));
		for(i = get2B(f); i--;){
			fprintf(xml, "%s<CHANNEL>\n", indent);
			fprintf(xml, "\t%s<ID>%d</ID>\n", indent, get4B(f));
			ed_colorspace(f, level+1, len, parent);
			fprintf(xml, "%s</CHANNEL>\n", indent);
		}
	}
}

static void ir_slices(psd_file_t f, int level, int len, struct dictentry *parent){
	const char *indent = tabs(level);
	unsigned i, origin, version;

	if(xml){
		fprintf(xml, "%s<VERSION>%d</VERSION>\n", indent, version = get4B(f));

		if(version == 6){
			// TODO: Not tested, needs PS6...
			fprintf(xml, "%s<BOUNDS>\n", indent);
			fprintf(xml, "\t%s<TOP>%d</TOP>\n", indent, get4B(f));
			fprintf(xml, "\t%s<LEFT>%d</LEFT>\n", indent, get4B(f));
			fprintf(xml, "\t%s<BOTTOM>%d</BOTTOM>\n", indent, get4B(f));
			fprintf(xml, "\t%s<RIGHT>%d</RIGHT>\n", indent, get4B(f));
			fprintf(xml, "%s</BOUNDS>\n", indent);

			fprintf(xml, "%s<NAME>", indent);
			xml_unicodestr(f, get4B(f));
			fputs("</NAME>\n", xml);

			for(i = get4B(f); i--;){
				fprintf(xml, "%s<SLICE>\n", indent);
				fprintf(xml, "\t%s<ID>%d</ID>\n", indent, get4B(f));
				fprintf(xml, "\t%s<GROUPID>%d</GROUPID>\n", indent, get4B(f));
				fprintf(xml, "\t%s<ORIGIN>%d</ORIGIN>\n", indent, origin = get4B(f));
				if(origin == 1)
					fprintf(xml, "\t%s<ASSOCLAYERID>%d</ASSOCLAYERID>\n", indent, get4B(f));
				fprintf(xml, "\t%s<NAME>", indent);
				xml_unicodestr(f, get4B(f));
				fputs("</NAME>\n", xml);
				fprintf(xml, "\t%s<TYPE>%d</TYPE>\n", indent, get4B(f));
				fprintf(xml, "\t%s<LEFT>%d</LEFT>\n", indent, get4B(f));
				fprintf(xml, "\t%s<TOP>%d</TOP>\n", indent, get4B(f));
				fprintf(xml, "\t%s<RIGHT>%d</RIGHT>\n", indent, get4B(f));
				fprintf(xml, "\t%s<BOTTOM>%d</BOTTOM>\n", indent, get4B(f));
				fprintf(xml, "\t%s<URL>", indent);
				xml_unicodestr(f, get4B(f));
				fputs("</URL>\n", xml);
				fprintf(xml, "\t%s<TARGET>", indent);
				xml_unicodestr(f, get4B(f));
				fputs("</TARGET>\n", xml);
				fprintf(xml, "\t%s<MESSAGE>", indent);
				xml_unicodestr(f, get4B(f));
				fputs("</MESSAGE>\n", xml);
				fprintf(xml, "\t%s<ALT>", indent);
				xml_unicodestr(f, get4B(f));
				fputs("</ALT>\n", xml);
				fprintf(xml, "\t%s<CELLTEXTISHTML>%d</CELLTEXTISHTML>\n", indent, fgetc(f));
				fprintf(xml, "\t%s<CELLTEXT>", indent);
				xml_unicodestr(f, get4B(f));
				fputs("</CELLTEXT>\n", xml);
				fprintf(xml, "\t%s<HALIGN>%d</HALIGN>\n", indent, get4B(f));
				fprintf(xml, "\t%s<VALIGN>%d</VALIGN>\n", indent, get4B(f));
				fprintf(xml, "\t%s<ALPHACOLOR>%d</ALPHACOLOR>\n", indent, fgetc(f));
				fprintf(xml, "\t%s<RED>%d</RED>\n", indent, fgetc(f));
				fprintf(xml, "\t%s<GREEN>%d</GREEN>\n", indent, fgetc(f));
				fprintf(xml, "\t%s<BLUE>%d</BLUE>\n", indent, fgetc(f));
				fprintf(xml, "%s</SLICE>\n", indent);
			}
		}
		else{
			// at least version 8 looks like a Descriptor
			ed_versdesc(f, level, len-4, parent);
		}
	}
}

#define PATHFIX(f) ((double)(f)/0x1000000)

void ir_path(psd_file_t f, int level, int len, struct dictentry *parent){
	const char *indent = tabs(level);
	int subpath_count = 0;

	if(xml)
		for(; len >= 26; len -= 26){
			int i, sel = get2B(f), skip = 24;
			double p[6];

			switch (sel){
			case 0: // closed subpath length record
			case 3: // open subpath length record
				if(!subpath_count){
					subpath_count = get2B(f);
					skip -= 2;
					fprintf(xml, "%s<SUBPATH>\n", indent);
					fprintf(xml, "%s\t<%s/>\n", indent, sel ? "OPEN" : "CLOSED");
				}else
					warn_msg("path resource: unexpected subpath record");
				break;
			case 1: // closed subpath Bezier knot, linked
			case 2: //    "      "       "     "   unlinked
			case 4: // open subpath Bezier knot, linked
			case 5: //  "      "       "     "   unlinked
				if(subpath_count){
					for(i = 0; i < 6; ++i)
						p[i] = PATHFIX(get4B(f));
					skip -= 24;
					fprintf(xml, "%s\t<KNOT>\n", indent);
					fprintf(xml, "%s\t\t<%sLINKED/>\n", indent,
							sel == 1 || sel == 4 ? "" : "UN");
					fprintf(xml, "%s\t\t<IN><V>%.9f</V><H>%.9f</H></IN>\n",
							indent, p[0], p[1]);
					fprintf(xml, "%s\t\t<ANCHOR><V>%.9f</V><H>%.9f</H></ANCHOR>\n",
							indent, p[2], p[3]);
					fprintf(xml, "%s\t\t<OUT><V>%.9f</V><H>%.9f</H></OUT>\n",
							indent, p[4], p[5]);
					fprintf(xml, "%s\t</KNOT>\n", indent);
					--subpath_count;
					if(!subpath_count)
						fprintf(xml, "%s</SUBPATH>\n", indent);
				}else
					warn_msg("path resource: unexpected knot record");
				break;
			case 6: // path fill rule record
				fprintf(xml, "%s<PATHFILLRULE/>\n", indent);
				break;
			case 7: // clipboard record
				fprintf(xml, "%s<CLIPBOARD>\n", indent);
				fprintf(xml, "%s\t<BOUNDS>\n", indent);
				fprintf(xml, "%s\t\t<TOP>%.9f</TOP>\n", indent, PATHFIX(get4B(f)));
				fprintf(xml, "%s\t\t<LEFT>%.9f</LEFT>\n", indent, PATHFIX(get4B(f)));
				fprintf(xml, "%s\t\t<BOTTOM>%.9f</BOTTOM>\n", indent, PATHFIX(get4B(f)));
				fprintf(xml, "%s\t\t<RIGHT>%.9f</RIGHT>\n", indent, PATHFIX(get4B(f)));
				fprintf(xml, "%s\t</BOUNDS>\n", indent);
				fprintf(xml, "%s\t<RESOLUTION>%.9f</RESOLUTION>\n", indent, PATHFIX(get4B(f)));
				fprintf(xml, "%s</CLIPBOARD>\n", indent);
				skip -= 20;
				break;
			case 8: // initial fill rule record
				fprintf(xml, "%s<INITIALFILL>%d</INITIALFILL>\n", indent, get2B(f));
				skip -= 2;
				break;
			default:
				warn_msg("path resource: unexpected record selector");
			}
			if(skip)
				fseeko(f, skip, SEEK_CUR);
		}
}

// id, key, tag, desc, func
static struct dictentry rdesc[] = {
	{1000, NULL, NULL, "PS2.0 mode data", NULL},
	{1001, NULL, NULL, "Macintosh print record", NULL},
	{1003, NULL, NULL, "PS2.0 indexed color table", NULL},
	{1005, NULL, "-RESOLUTION", "ResolutionInfo", ir_resolution},
	{1006, NULL, "ALPHANAMES", "Alpha names", ir_pstrings},
	{1007, NULL, "DISPLAYINFO", "DisplayInfo", ir_displayinfo},
	{1008, NULL, "-CAPTION", "Caption", ir_pstring},
	{1009, NULL, NULL, "Border information", NULL},
	{1010, NULL, "BGCOLOR", "Background color", ed_colorspace},
	{1011, NULL, "PRINTFLAGS", "Print flags", ir_printflags},
	{1012, NULL, NULL, "Grayscale/multichannel halftoning info", NULL},
	{1013, NULL, NULL, "Color halftoning info", NULL},
	{1014, NULL, NULL, "Duotone halftoning info", NULL},
	{1015, NULL, NULL, "Grayscale/multichannel transfer function", NULL},
	{1016, NULL, NULL, "Color transfer functions", NULL},
	{1017, NULL, NULL, "Duotone transfer functions", NULL},
	{1018, NULL, NULL, "Duotone image info", NULL},
	{1019, NULL, NULL, "B&W values for the dot range", NULL},
	{1021, NULL, NULL, "EPS options", NULL},
	{1022, NULL, NULL, "Quick Mask info", NULL},
	{1024, NULL, "-TARGETLAYER", "Layer state info", ir_2byte},
	{1025, NULL, "WORKINGPATH", "Working path", ir_path},
	{1026, NULL, "LAYERSGROUP", "Layers group info", ir_layersgroup},
	{1028, NULL, NULL, "IPTC-NAA record (File Info)", NULL},
	{1029, NULL, NULL, "Image mode for raw format files", NULL},
	{1030, NULL, NULL, "JPEG quality", NULL},
	// v4.0
	{1032, NULL, "GRIDGUIDES", "Grid and guides info", ir_gridguides},
	{1033, NULL, NULL, "Thumbnail resource", NULL},
	{1034, NULL, "-COPYRIGHTFLAG", "Copyright flag", ir_1byte},
	{1035, NULL, "-URL", "URL", ir_string}, // incorrectly documented as Pascal string
	// v5.0
	{1036, NULL, NULL, "Thumbnail resource (5.0)", NULL},
	{1037, NULL, "-GLOBALANGLE", "Global Angle", ir_4byte},
	{1038, NULL, "COLORSAMPLERS", "Color samplers resource", ir_colorsamplers},
	{1039, NULL, "ICC34", "ICC Profile", ir_icc34profile},
	{1040, NULL, "-WATERMARK", "Watermark", ir_1byte},
	{1041, NULL, "-ICCUNTAGGED", "ICC Untagged Profile", ir_1byte},
	{1042, NULL, "-EFFECTSVISIBLE", "Effects visible", ir_1byte},
	{1043, NULL, NULL, "Spot Halftone", NULL},
	{1044, NULL, "-DOCUMENTIDSEED", "Document specific IDs", ir_4byte},
	{1045, NULL, "ALPHANAMESUNICODE", "Unicode Alpha Names", ir_unicodestrings},
	// v6.0
	{1046, NULL, "-COLORTABLECOUNT", "Indexed Color Table Count", ir_2byte},
	{1047, NULL, "-TRANSPARENTINDEX", "Transparent Index", ir_2byte},
	{1049, NULL, "-GLOBALALTITUDE", "Global Altitude", ir_4byte},
	{1050, NULL, "SLICES", "Slices", ir_slices},
	{1051, NULL, "-WORKFLOWURL", "Workflow URL", ir_unicodestr},
	{1052, NULL, NULL, "Jump To XPEP", NULL},
	{1053, NULL, "ALPHAIDS", "Alpha Identifiers", ir_alphaids},
	{1054, NULL, NULL, "URL List", NULL},
	{1057, NULL, NULL, "Version Info", NULL},
	// v7.0 - from CS doc
	{1058, NULL, NULL, "EXIF data 1", NULL},
	{1059, NULL, NULL, "EXIF data 3", NULL},
	{1060, NULL, "XMP", "XMP metadata", ir_cdata},
	{1061, NULL, "-CAPTIONDIGEST", "Caption digest (RSA MD5)", ir_digest},
	{1062, NULL, NULL, "Print scale", NULL},
	// CS
	{1064, NULL, "-PIXELASPECTRATIO", "Pixel aspect ratio", ir_pixelaspect},
	{1065, NULL, "LAYERCOMPS", "Layer comps", ed_versdesc},
	{1066, NULL, "ALTDUOTONECOLORS", "Alternate duotone colors", ir_altduotonecolors},
	{1067, NULL, "ALTSPOTCOLORS", "Alternate spot colors", ir_altspotcolors},

	// CS2 - July 2010 document
	{1069, NULL, "LAYERSELECTIONIDS", "Layer Selection ID(s)", ir_layerselectionids},
	{1070, NULL, NULL, "HDR Toning information", NULL},
	{1071, NULL, NULL, "Print info", NULL},
	{1072, NULL, NULL, "Layer Group(s) Enabled ID", NULL},

	// CS3 - July 2010 document
	{1073, NULL, "COLORSAMPLERSCS3", "Color samplers resource (CS3)", ir_colorsamplers},
	{1074, NULL, "MEASUREMENTSCALE", "Measurement Scale", ed_versdesc},
	{1075, NULL, "TIMELINE", "Timeline Information", ed_versdesc},
	{1076, NULL, "SHEETDISCLOSURE", "Sheet Disclosure", ed_versdesc},
	{1077, NULL, "DISPLAYINFOCS3", "DisplayInfo (CS3)", ir_displayinfocs3},
	{1078, NULL, "ONIONSKINS", "Onion Skins", ed_versdesc},

	// CS4 - July 2010 document
	{1080, NULL, "COUNT", "Count Information", ed_versdesc},

	// CS5 - July 2010 document
	{1082, NULL, "PRINTSETTINGS", "Print Information", ed_versdesc},
	{1083, NULL, "PRINTSTYLE", "Print Style", ed_versdesc},
	{1084, NULL, NULL, "Macintosh NSPrintInfo", NULL},
	{1085, NULL, NULL, "Windows DEVMODE", NULL},

	{7000, NULL, "IMAGEREADYVARS", "ImageReady variables", ir_cdata},
	{7001, NULL, NULL, "ImageReady data sets", NULL},

	{8000, NULL, NULL, "Lightroom workflow", NULL}, // CS3 per July 2010 document

	{2999, NULL, "-CLIPPINGPATH", "Name of clipping path", ir_pstring},
	{10000,NULL, "PRINTFLAGS10K", "Print flags info", ir_printflags10k},
	{-1, NULL, NULL, NULL, NULL}
};

static struct dictentry *findbyid(int id){
	static struct dictentry path = {0, NULL, "PATH", "Path", ir_path};
	struct dictentry *d;

	/* dumb linear lookup of description string from resource id */
	/* assumes array ends with a NULL desc pointer */
	if(id >= 2000 && id <= 2998)
		return &path; // special case
	for(d = rdesc; d->desc; ++d)
		if(d->id == id)
			return d;
	return NULL;
}

static long doirb(psd_file_t f){
	static struct dictentry resource = {0, NULL, "RESOURCE", "dummy", NULL};
	char type[4], name[0x100];
	int id, namelen;
	long size;
	size_t padded_size;
	struct dictentry *d;

	fread(type, 1, 4, f);
	id = get2B(f);
	namelen = fgetc(f);
	fread(name, 1, PAD2(1+namelen)-1, f);
	name[namelen] = 0;
	size = get4B(f);
	padded_size = PAD2(size);

	d = findbyid(id);
	if((verbose || print_rsrc || resdump) && !xmlout){
		printf("  resource '%c%c%c%c' (%5d,\"%s\"):%5ld bytes",
			   type[0],type[1],type[2],type[3], id, name, size);
		if(d)
			printf(" [%s]", d->desc);
		putchar('\n');
	}

	if(d && d->tag){
		if(xml){
			fprintf(xml, "\t<RESOURCE TYPE='%c%c%c%c' ID='%d'",
					type[0],type[1],type[2],type[3], id);
			if(namelen)
				fprintf(xml, " NAME='%s'", name);
		}
		if(d->func){
			if(xml) fputs(">\n", xml);

			entertag(f, 2, size, &resource, d, 1);

			if(xml) fputs("\t</RESOURCE>\n\n", xml);
		}
		else if(xml){
			fputs(" /> <!-- not parsed -->\n", xml);
		}
	}

	if(resdump){
		char *temp_buf = checkmalloc(padded_size);
		if(fread(temp_buf, 1, padded_size, f) < padded_size)
			fatal("did not read expected bytes in image resource\n");
		dumphex((unsigned char*)temp_buf, size);
		putchar('\n');
		free(temp_buf);
	}
	else
		fseeko(f, padded_size, SEEK_CUR); // skip resource block data

	return 4+2+PAD2(1+namelen)+4+padded_size; /* returns total bytes in block */
}

void doimageresources(psd_file_t f){
	long len = get4B(f);
	VERBOSE("\nImage resources (%ld bytes):\n", len);
	while(len > 0)
		len -= doirb(f);
	if(len != 0)
		warn_msg("image resources overran expected size by %d bytes\n", -len);
}
