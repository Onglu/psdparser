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
#include "psdparser.h"

extern PSD_LAYERS_INFO g_info;
extern int g_lid;

#ifdef HAVE_ICONV_H
	extern iconv_t ic;
#endif

/* 'Extra data' handling. *Work in progress*
 *
 * There's guesswork and trial-and-error in here,
 * due to many errors and omissions in Adobe's documentation.
 */

static struct psd_header *psd_header = NULL;

#define BITSTR(f) ((f) ? "(1)" : "(0)")

void entertag(psd_file_t f, int level, int len, struct dictentry *parent,
			  struct dictentry *d, int resetpos)
{
	psd_bytes_t savepos = (psd_bytes_t)ftello(f);
	int oneline = d->tag[0] == '-';
	char *tagname = d->tag + oneline;

	if(xml){
		// check parent's one-line-ness, because what precedes our <TAG>
		// belongs to our parent.
		fprintf(xml, "%s<%s>", parent->tag[0] == '-' ? " " : tabs(level), tagname);
		if(!oneline)
			fputc('\n', xml);
	}

	d->func(f, level+1, len, d); // parse contents of this datum

	if(xml){
		fprintf(xml, "%s</%s>", oneline ? "" : tabs(level), tagname);
		// if parent's not one-line, then we can safely newline after our tag.
		fputc(parent->tag[0] == '-' ? ' ' : '\n', xml);
	}

	if(resetpos)
		fseeko(f, savepos, SEEK_SET);
}

// This uses a dumb linear search. But it's efficient enough in practice.

struct dictentry *findbykey(psd_file_t f, int level, struct dictentry *parent,
							char *key, int len, int resetpos)
{
	struct dictentry *d;

	for(d = parent; d->key; ++d)
		if(KEYMATCH(key, d->key)){
			char *tagname = d->tag + (d->tag[0] == '-');
			//fprintf(stderr, "matched tag %s\n", d->tag);
			if(d->func)
				entertag(f, level, len, parent, d, resetpos);
			else{
				// there is no function to parse this block.
				// because tag is empty in this case, we only need to consider
				// parent's one-line-ness.
				if(xml){
					if(parent->tag[0] == '-')
						fprintf(xml, " <%s /> <!-- not parsed --> ", tagname);
					else
						fprintf(xml, "%s<%s /> <!-- not parsed -->\n", tabs(level), tagname);
				}
			}
			return d;
		}
	return NULL;
}

/* not 'static'; these are referenced by scavenge.c */
struct dictentry bmdict[] = {
	{0, "norm", "NORMAL", "normal", NULL},
	{0, "dark", "DARKEN", "darken", NULL},
	{0, "lite", "LIGHTEN", "lighten", NULL},
	{0, "hue ", "HUE", "hue", NULL},
	{0, "sat ", "SATURATION", "saturation", NULL},
	{0, "colr", "COLOR", "color", NULL},
	{0, "lum ", "LUMINOSITY", "luminosity", NULL},
	{0, "mul ", "MULTIPLY", "multiply", NULL},
	{0, "scrn", "SCREEN", "screen", NULL},
	{0, "diss", "DISSOLVE", "dissolve", NULL},
	{0, "over", "OVERLAY", "overlay", NULL},
	{0, "hLit", "HARDLIGHT", "hard light", NULL},
	{0, "sLit", "SOFTLIGHT", "soft light", NULL},
	{0, "diff", "DIFFERENCE", "difference", NULL},
	{0, "smud", "EXCLUSION", "exclusion", NULL},
	{0, "div ", "COLORDODGE", "color dodge", NULL},
	{0, "idiv", "COLORBURN", "color burn", NULL},
	// CS
	{0, "lbrn", "LINEARBURN", "linear burn", NULL},
	{0, "lddg", "LINEARDODGE", "linear dodge", NULL},
	{0, "vLit", "VIVIDLIGHT", "vivid light", NULL},
	{0, "lLit", "LINEARLIGHT", "linear light", NULL},
	{0, "pLit", "PINLIGHT", "pin light", NULL},
	{0, "hMix", "HARDMIX", "hard mix", NULL},
	// from July 2010 doc
	{0, "pass", "PASSTHROUGH", "passthrough", NULL},
	{0, "dkCl", "DARKERCOLOR", "darker color", NULL},
	{0, "lgCl", "LIGHTERCOLOR", "lighter color", NULL},
	{0, "fsub", "SUBTRACT", "subtract", NULL},
	{0, "fdiv", "DIVIDE", "divide", NULL},
	{0, NULL, NULL, NULL, NULL}
};

void layerblendmode(psd_file_t f, int level, int len, struct blend_mode_info *bm){
	struct dictentry *d;
	const char *indent = tabs(level);

	if(xml && KEYMATCH(bm->sig, "8BIM")){
		fprintf(xml, "%s<BLENDMODE OPACITY='%g' CLIPPING='%d'>\n",
				indent, bm->opacity/2.55, bm->clipping);
		findbykey(f, level+1, bmdict, bm->key, len, 1);
		if(bm->flags & 1) fprintf(xml, "%s\t<TRANSPARENCYPROTECTED />\n", indent);
		if(bm->flags & 2) fprintf(xml, "%s\t<HIDDEN />\n", indent);
		if((bm->flags & (8|16)) == (8|16))  // both bits set
			fprintf(xml, "%s\t<PIXELDATAIRRELEVANT />\n", indent);
		fprintf(xml, "%s</BLENDMODE>\n", indent);
	}
	if(!xml){
		d = findbykey(f, level+1, bmdict, bm->key, len, 1);
		VERBOSE("  blending mode: sig='%c%c%c%c' key='%c%c%c%c'(%s) opacity=%d(%d%%) clipping=%d(%s)\n\
    flags=%#x(transp_prot%s visible%s bit4valid%s pixel_data_irrelevant%s)\n",
				bm->sig[0],bm->sig[1],bm->sig[2],bm->sig[3],
				bm->key[0],bm->key[1],bm->key[2],bm->key[3],
				d ? d->desc : "???",
				bm->opacity, (bm->opacity*100+127)/255,
				bm->clipping, bm->clipping ? "non-base" : "base",
				bm->flags, BITSTR(bm->flags&1), BITSTR(bm->flags&2), BITSTR(bm->flags&8), BITSTR(bm->flags&16) );
	}
}

// CS doc
static void blendmode(psd_file_t f, int level, int len, struct dictentry *parent){
	char sig[4], key[4];

	fread(sig, 1, 4, f);
	fread(key, 1, 4, f);
	if(xml && KEYMATCH(sig, "8BIM")){
		fprintf(xml, "%s<BLENDMODE>\n", tabs(level));
		findbykey(f, level+1, bmdict, key, len, 1);
		fprintf(xml, "%s</BLENDMODE>\n", tabs(level));
	}
}

struct colour_space *find_colour_space(int space){
	struct colour_space *sp;

	// look for our description of this colour space
	for(sp = colour_spaces; sp->name && sp->id != space; ++sp)
		;
	return sp->name ? sp : NULL;
}

// Print XML for an 8-byte colour description, passed as raw bytes,
// given the colour space id in parameter 'space'.

void colorspace(int level, int space, unsigned char data[]){
	const char *indent = tabs(level);
	int i;
	unsigned char str[9];
	struct colour_space *sp = find_colour_space(space);

	if(xml){
		memcpy(str, data, 8);
		str[8] = 0;

		fprintf(xml, "%s<COLOR>\n", indent);
		if(!sp){ // did not find the matching colour space id
			// There's not much point in parsing this, but spit out
			// the component values and a possible string anyway.
			fprintf(xml, "\t%s<UNKNOWNCOLORSPACE>\n", indent);
			fprintf(xml, "\t\t%s<ID>%d</ID>\n", indent, space);
			fprintf(xml, "\t\t%s<STRING>%s</STRING>\n", indent, str);
			for(i = 0; i < 4; ++i)
				fprintf(xml, "\t\t%s<COMPONENT>%u</COMPONENT>\n",
						indent, peek2Bu(data+i*2));
			fprintf(xml, "\t%s</UNKNOWNCOLORSPACE>\n", indent);
		}
		else if(sp->components && sp->components[0] == '*'){
			// In some spaces, this is a readable string value
			// (though this isn't endorsed by documentation).
			fprintf(xml, "\t%s<%s>", indent, sp->name);
			fputsxml((char*)str, xml);
			fprintf(xml, "</%s>\n", sp->name);
		}
		else if(sp->components){
			// This is a recognised colour space, so we can label
			// the component elements.
			int n = strlen(sp->components);
			fprintf(xml, "\t%s<%s>", indent, sp->name);
			for(i = 0; i < 4; ++i){
				unsigned value = peek2Bu(data+i*2);
				// use initials for each component's element
				if(i < n)
					fprintf(xml, " <%c>%u</%c>",
							sp->components[i], value, sp->components[i]);
			}
			fprintf(xml, " </%s>\n", sp->name);
		}
		else{
			fprintf(xml, "\t%s<%s/>\n", indent, sp->name);
		}
		fprintf(xml, "%s</COLOR>\n", indent);
	}
}

// Read and print XML for an 8-byte colour description, given a colour space id
// in parameter 'space'. (Sometimes colour values appear separately from
// the colour space definition - for example in gradient data.)

static void color(psd_file_t f, int level, int space){
	unsigned char data[8];

	fread(data, 1, 8, f);
	colorspace(level, space, data);
}

// Read and print XML for a 10-byte colour description that begins with its
// colour space id.

void ed_colorspace(psd_file_t f, int level, int len, struct dictentry *parent){
	color(f, level, get2B(f));
}

// Print XML for colour description in current mode.

void ed_color(psd_file_t f, int level, int len, struct dictentry *parent){
	color(f, level, mode_colour_space[psd_header->mode]);
}

void conv_unicodestyles(psd_file_t f, long count, const char *indent){
	unsigned short *utf16 = checkmalloc(2*count);
	short *style = checkmalloc(2*count);
	int i;

	#ifdef HAVE_ICONV_H
	size_t inb, outb;
	char *inbuf, *outbuf, *utf8;
	#endif

	if(utf16 && style){
		for(i = 0; i < count; i++){
			utf16[i] = get2Bu(f); // the UTF-16 char
			style[i] = get2B(f);  // and its corresponding style code
		}

#ifdef HAVE_ICONV_H
		iconv(ic, NULL, &inb, NULL, &outb); // reset iconv state

		outb = 6*count; // sloppy overestimate of buffer (FIXME)
		if( (utf8 = checkmalloc(outb)) ){
			inbuf = (char*)utf16;
			inb = 2*count;
			outbuf = utf8;
			if(ic != (iconv_t)-1){
				if(iconv(ic, &inbuf, &inb, &outbuf, &outb) != (size_t)-1){
					if(xml){
						fprintf(xml, "%s<UNICODE>", indent);
						fwritexml(utf8, outbuf-utf8, xml);
						fputs("</UNICODE>\n", xml);
					}

					// copy to terminal
					if(verbose)
						fwrite(utf8, 1, outbuf-utf8, stdout);
				}else
					alwayswarn("iconv() failed, errno=%u\n", errno);
			}
			free(utf8);
		}
#endif
		if(xml)
			for(i = 0; i < count; ++i)
				fprintf(xml, "%s<S>%d</S>\n", indent, style[i]);
	}else
		fatal("conv_unicodestyle(): can't get memory");

	free(utf16);
	free(style);
}

static void ed_typetool(psd_file_t f, int level, int len, struct dictentry *parent){
	int i, j, v = get2B(f), mark, type, script, facemark,
		autokern, charcount, selstart, selend, linecount, orient, align;
	double size, tracking, kerning, leading, baseshift, scaling, hplace, vplace;
	static const char *coeff[] = {"XX","XY","YX","YY","TX","TY"}; // from CS doc
	const char *indent = tabs(level);

	if (g_info.layers && g_lid < g_info.count)
	{
		g_info.layers[g_lid].type = LT_Decoration;
	}

	if(xml){
		fprintf(xml, "%s<VERSION>%d</VERSION>\n", indent, v);

		// read transform (6 doubles)
		fprintf(xml, "%s<TRANSFORM>", indent);
		for(i = 0; i < 6; ++i)
			fprintf(xml, " <%s>%g</%s>", coeff[i], getdoubleB(f), coeff[i]);
		fputs(" </TRANSFORM>\n", xml);

		// read font information
		v = get2B(f);
		fprintf(xml, "%s<FONTINFOVERSION>%d</FONTINFOVERSION>\n", indent, v);

		if(v <= 6){
			for(i = get2B(f); i--;){
				mark = get2B(f);
				type = get4B(f);
				fprintf(xml, "%s<FACE MARK='%d' TYPE='%d' FONTNAME='%s'", indent, mark, type, getpstr(f));
				fprintf(xml, " FONTFAMILY='%s'", getpstr(f));
				fprintf(xml, " FONTSTYLE='%s'", getpstr(f));
				script = get2B(f);
				fprintf(xml, " SCRIPT='%d'>\n", script);

				// doc is unclear, but this may work:
				fprintf(xml, "%s\t<DESIGNVECTOR>", indent);
				for(j = get4B(f); j--;)
					fprintf(xml, " <AXIS>%d</AXIS>", get4B(f));
				fputs(" </DESIGNVECTOR>\n", xml);

				fprintf(xml, "%s</FACE>\n", indent);
			}

			j = get2B(f); // count of styles
			for(; j--;){
				mark = get2B(f);
				facemark = get2B(f);
				size = FIXEDPT(get4B(f)); // of course, which fields are fixed point is undocumented
				tracking = FIXEDPT(get4B(f));  // so I'm
				kerning = FIXEDPT(get4B(f));   // taking
				leading = FIXEDPT(get4B(f));   // a punt
				baseshift = FIXEDPT(get4B(f)); // on these
				autokern = fgetc(f);
				fprintf(xml, "%s<STYLE MARK='%d' FACEMARK='%d' SIZE='%g' TRACKING='%g' KERNING='%g' LEADING='%g' BASESHIFT='%g' AUTOKERN='%d'",
						indent, mark, facemark, size, tracking, kerning, leading, baseshift, autokern);
				if(v <= 5)
					fprintf(xml, " EXTRA='%d'", fgetc(f));
				fprintf(xml, " ROTATE='%d' />\n", fgetc(f));
			}

			type = get2B(f);
			scaling = FIXEDPT(get4B(f));
			charcount = get4B(f);
			hplace = FIXEDPT(get4B(f));
			vplace = FIXEDPT(get4B(f));
			selstart = get4B(f);
			selend = get4B(f);
			linecount = get2B(f);
			fprintf(xml, "%s<TEXT TYPE='%d' SCALING='%g' CHARCOUNT='%d' HPLACEMENT='%g' VPLACEMENT='%g' SELSTART='%d' SELEND='%d'>\n",
					indent, type, scaling, charcount, hplace, vplace, selstart, selend);
			for(i = linecount; i--;){
				charcount = get4B(f);
				orient = get2B(f);
				align = get2B(f);
				fprintf(xml, "%s\t<LINE ORIENTATION='%d' ALIGNMENT='%d'>\n", indent, orient, align);
				conv_unicodestyles(f, charcount, indent-2);
				fprintf(xml, "%s\t</LINE>\n", indent);
			}
			ed_colorspace(f, level+1, len, parent);
			fprintf(xml, "%s\t<ANTIALIAS>%d</ANTIALIAS>\n", indent, fgetc(f));

			fprintf(xml, "%s</TEXT>\n", indent);
		}else if(v == 50){
			ed_versdesc(f, level, len, parent); // text

			fprintf(xml, "%s<WARPVERSION>%d</WARPVERSION>\n", indent, get2B(f));
			ed_versdesc(f, level, len, parent); // warp

			fprintf(xml, "%s<LEFT>%d</LEFT>\n", indent, get4B(f));
			fprintf(xml, "%s<TOP>%d</TOP>\n", indent, get4B(f));
			fprintf(xml, "%s<RIGHT>%d</RIGHT>\n", indent, get4B(f));
			fprintf(xml, "%s<BOTTOM>%d</BOTTOM>\n", indent, get4B(f));
		}else
			fprintf(xml, "%s<!-- don't know how to parse version %d -->\n", indent, v);
	}else
		UNQUIET("    (%s, version = %d)\n", parent->desc, v);
}

// Stores last layer name encountered; pointer to UTF-8 string.
char *last_layer_name = NULL;

static void ed_unicodename(psd_file_t f, int level, int len, struct dictentry *parent){
	unsigned long length = get4B(f); // character count, not byte count
	char *buf = last_layer_name = conv_unicodestr(f, length);

	if(buf){
		if(xml)
			fputsxml(buf, xml);
		UNQUIET("    (Unicode name = '%s')\n", buf);
		//free(buf); // caller may use it via global
	}
}

static void ed_long(psd_file_t f, int level, int len, struct dictentry *parent){
	unsigned long id = get4B(f);
	if(xml)
		fprintf(xml, "%lu", id);
	else
		UNQUIET("    (%s = %lu)\n", parent->desc, id);
}

static void ed_key(psd_file_t f, int level, int len, struct dictentry *parent){
	char *key = getkey(f);
	if(xml)
		fprintf(xml, "%s", key);
	else
		UNQUIET("    (%s = '%s')\n", parent->desc, key);
}

static void ed_sectiondivider(psd_file_t f, int level, int len, struct dictentry *parent){
	static const char *type[] = {"OTHER", "OPENFOLDER", "CLOSEDFOLDER", "BOUNDING"};
	if(xml){
		int t = get4B(f);
		if(t >= 0 && t <= 3)
			fprintf(xml, "%s<%s/>\n", tabs(level), type[t]);
		else
			fprintf(xml, "%s<TYPE>%d</TYPE>\n", tabs(level), t);
		if(len >= 12)
			blendmode(f, level, len, parent);
	}
}

static void ed_blendingrestrictions(psd_file_t f, int level, int len, struct dictentry *parent){
	int i = len/4;
	if(len && xml)
		while(i--)
			fprintf(xml, "%s<CHANNEL>%d</CHANNEL>\n", tabs(level), get4B(f));
}

static void ed_gradient(psd_file_t f, int level, int len, struct dictentry *parent){
	int stops, expcount, length, space;
	const char *indent = tabs(level);

	if(xml){
		fprintf(xml, "%s<VERSION>%d</VERSION>\n", indent, get2B(f));
		fprintf(xml, "%s<REVERSED>%d</REVERSED>\n", indent, fgetc(f));
		fprintf(xml, "%s<DITHERED>%d</DITHERED>\n", indent, fgetc(f));
		fprintf(xml, "%s<NAME>", indent);
		xml_unicodestr(f, get4B(f));
		fputs("</NAME>\n", xml);
		for(stops = get2Bu(f); stops--;){
			fprintf(xml, "%s<COLORSTOP>\n", indent);
			fprintf(xml, "\t%s<LOCATION>%g</LOCATION>\n", indent, FIXEDPT(get4B(f)));
			fprintf(xml, "\t%s<MIDPOINT>%g</MIDPOINT>\n", indent, FIXEDPT(get4B(f)));
			fprintf(xml, "\t%s<MODE>%d</MODE>\n", indent, get2B(f));
			color(f, level+1, -2 /* FIXME: What colour space *is* meant?? */ );
			fprintf(xml, "%s</COLORSTOP>\n", indent);
		}
		for(stops = get2Bu(f); stops--;){
			fprintf(xml, "%s<TRANSPARENCYSTOP>\n", indent);
			fprintf(xml, "\t%s<LOCATION>%g</LOCATION>\n", indent, FIXEDPT(get4B(f)));
			fprintf(xml, "\t%s<MIDPOINT>%g</MIDPOINT>\n", indent, FIXEDPT(get4B(f)));
			fprintf(xml, "\t%s<OPACITY>%d</OPACITY>\n", indent, get2B(f));
			fprintf(xml, "%s</TRANSPARENCYSTOP>\n", indent);
		}
		fprintf(xml, "%s<EXPANSIONCOUNT>%d</EXPANSIONCOUNT>\n", indent, expcount = fgetc(f));
		if(expcount){
			fprintf(xml, "%s<INTERPOLATION>%d</INTERPOLATION>\n", indent, fgetc(f));
			length = get2B(f);
			if(length >= 32){
				fprintf(xml, "%s<MODE>%d</MODE>\n", indent, get2B(f));
				fprintf(xml, "%s<RANDOMSEED>%u</RANDOMSEED>\n", indent, get4B(f));
				fprintf(xml, "%s<SHOWTRANSPARENCY>%d</SHOWTRANSPARENCY>\n", indent, get2B(f));
				fprintf(xml, "%s<VECTORCOLOR>%d</VECTORCOLOR>\n", indent, get2B(f));
				fprintf(xml, "%s<ROUGHNESS>%d</ROUGHNESS>\n", indent, get4B(f));
				fprintf(xml, "%s<COLORSPACE>%d</COLORSPACE>\n", indent, space = get2B(f));
				fprintf(xml, "%s<MINIMUM>\n", indent);
				color(f, level+1, space);
				fprintf(xml, "%s</MINIMUM>\n", indent);
				fprintf(xml, "%s<MAXIMUM>\n", indent);
				color(f, level+1, space);
				fprintf(xml, "%s</MAXIMUM>\n", indent);
			}
		}
	}
}

static void ed_annotation(psd_file_t f, int level, int len, struct dictentry *parent){
	int i, j, major = get2B(f), minor = get2B(f), length, open, flags, optblocks;
	char type[4], key[4];
	const char *indent = tabs(level);
	long datalen, len2, rects[8];

	if(xml){
		fprintf(xml, "%s<VERSION MAJOR='%d' MINOR='%d' />\n", indent, major, minor);
		for(i = get4B(f); i--;){
			length = get4B(f);
			fread(type, 1, 4, f);
			open = fgetc(f);
			flags = fgetc(f);
			optblocks = get2B(f);
			// read two rectangles - icon and popup
			for(j = 0; j < 8;)
				rects[j++] = get4B(f);
			ed_colorspace(f, level, len, parent);

			if(KEYMATCH(type, "txtA"))
				fprintf(xml, "%s<TEXT", indent);
			else if(KEYMATCH(type, "sndA"))
				fprintf(xml, "%s<SOUND", indent);
			else
				fprintf(xml, "%s<UNKNOWN", indent);
			fprintf(xml, " OPEN='%d' FLAGS='%d' AUTHOR='", open, flags);
			fputsxml(getpstr2(f), xml);
			fputs("' NAME='", xml);
			fputsxml(getpstr2(f), xml);
			fputs("' MODDATE='", xml);
			fputsxml(getpstr2(f), xml);
			fprintf(xml, "' ICONT='%ld' ICONL='%ld' ICONB='%ld' ICONR='%ld'", rects[0],rects[1],rects[2],rects[3]);
			fprintf(xml, " POPUPT='%ld' POPUPL='%ld' POPUPB='%ld' POPUPR='%ld'", rects[4],rects[5],rects[6],rects[7]);

			len2 = get4B(f)-12; // remaining bytes in annotation
			fread(key, 1, 4, f);
			datalen = get4B(f);
			//printf(" optblocks=%d key=%c%c%c%c len2=%ld datalen=%ld\n", optblocks, key[0],key[1],key[2],key[3],len2,datalen);
			if(KEYMATCH(key, "txtC")){
				unsigned char bom[2];

				fputc('>', xml);
				// use the same BOM test as PDF strings
				fread(bom, 1, 2, f);
				len2 -= datalen; // we consumed this much from the file
				datalen -= 2;
				if(bom[0] == 0xfe && bom[1] == 0xff)
					xml_unicodestr(f, datalen/2);
				else{
					fputcxml(bom[0], xml);
					fputcxml(bom[1], xml);
					while(datalen--)
						fputcxml(fgetc(f), xml);
				}
				fputs("</TEXT>\n", xml);
			}else if(KEYMATCH(key, "sndM")){
				// TODO: Check PDF doc for format of sound annotation
				fprintf(xml, " RATE='%ld' BYTES='%ld' />\n", datalen, len2);
			}else
				fputs(" /> <!-- don't know -->\n", xml);

			fseeko(f, len2, SEEK_CUR); // skip whatever's left of this annotation's data
		}
	}else{
		UNQUIET("    (%s, version = %d.%d)\n", parent->desc, major, minor);
	}
}

static void ed_byte(psd_file_t f, int level, int len, struct dictentry *parent){
	int k = fgetc(f);
	if(xml)
		fprintf(xml, "%d", k);
}

static void ed_referencepoint(psd_file_t f, int level, int len, struct dictentry *parent){
	double x,y;

	g_info.layers[g_lid].name;

	x = getdoubleB(f);
	y = getdoubleB(f);

	if(xml)
		fprintf(xml, " <X>%g</X> <Y>%g</Y> ", x, y);
	else
		UNQUIET("    (%s X=%g Y=%g)\n", parent->desc, x, y);

//#ifndef VER_NO_1_0_0
	if (g_info.layers && g_lid < g_info.count)
	{
		g_info.layers[g_lid].ref.x = x;
		g_info.layers[g_lid].ref.y = y;
	}
//#endif
}

// CS doc
void ed_versdesc(psd_file_t f, int level, int len, struct dictentry *parent){
	long v = get4B(f);
	if(xml){
		fprintf(xml, "%s<DESCRIPTORVERSION>%ld</DESCRIPTORVERSION>\n", tabs(level), v);
		fprintf(xml, "%s<DESCRIPTOR>\n", tabs(level));
		descriptor(f, level+1, len, parent);
		fprintf(xml, "%s</DESCRIPTOR>\n", tabs(level));
	}
}

// CS doc
static void ed_objecteffects(psd_file_t f, int level, int len, struct dictentry *parent){
	if(xml){
		fprintf(xml, "%s<VERSION>%d</VERSION>\n", tabs(level), get4B(f));
		ed_versdesc(f, level, len, parent);
	}
}

// CS doc
static void ed_vectormask(psd_file_t f, int level, int len, struct dictentry *parent){
	extern void ir_path(psd_file_t f, int level, int len, struct dictentry *parent);
	const char *indent = tabs(level);
	int flags;

	if(xml){
		fprintf(xml, "%s<VERSION>%d</VERSION>\n", indent, get4B(f));
		flags = get4B(f);
		if(flags & 2) fprintf(xml, "%s<INVERT/>\n", indent);
		if(flags & 4) fprintf(xml, "%s<NOTLINK/>\n", indent);
		if(flags & 8) fprintf(xml, "%s<DISABLE/>\n", indent);
		fprintf(xml, "%s<PATH>\n", indent);
		ir_path(f, level+1, len-8, parent);
		fprintf(xml, "%s</PATH>\n", indent);
	}
}

static int sigkeyblock(psd_file_t f, struct psd_header *h, int level, int len, struct dictentry *dict){
	char sig[4], key[4];
	long length;
	struct dictentry *d;
	int is_photoshop;

	fread(sig, 1, 4, f);
	is_photoshop = KEYMATCH(sig, "8BIM") || KEYMATCH(sig, "8B64");
	fread(key, 1, 4, f);
	length = is_photoshop
			 && ( KEYMATCH(key, "LMsk") || KEYMATCH(key, "Lr16") || KEYMATCH(key, "Lr32")
			   || KEYMATCH(key, "Layr") || KEYMATCH(key, "Mt16") || KEYMATCH(key, "Mt32")
			   || KEYMATCH(key, "Mtrn") || KEYMATCH(key, "Alph") || KEYMATCH(key, "FMsk")
			   || KEYMATCH(key, "Ink2") || KEYMATCH(key, "FEid") || KEYMATCH(key, "FXid")
			   || KEYMATCH(key, "PxSD") )
			  ? GETPSDBYTES(f) : get4B(f);
	if(!xml)
		VERBOSE("    data block: sig='%c%c%c%c' key='%c%c%c%c' length=%7ld\n",
				sig[0],sig[1],sig[2],sig[3], key[0],key[1],key[2],key[3], length);
	if(is_photoshop && dict && (d = findbykey(f, level, dict, key, length, 1))
	   && !d->func && !xml)
	{
		// there is no function to parse this block
		UNQUIET("    (data: %s)\n", d->desc);
		if(verbose){
			psd_bytes_t pos = (psd_bytes_t)ftello(f);
			int n = length > 32 ? 32 : length;
			printf("    ");
			while(n--)
				printf("%02x ", fgetc(f));
			printf(length > 32 ? "...\n" : "\n");
			fseeko(f, pos, SEEK_SET);
		}
	}
	fseeko(f, length, SEEK_CUR);
	return length + 12; // return number of bytes consumed
}

static void dumpblock(psd_file_t f, int level, int len, struct dictentry *dict){
	// FIXME: this can over-run the actual block; need to pass block length into the function
	if(verbose){
		int n = 32;
		printf("%s: ", dict->desc);
		while(n--)
			printf("%02x ", fgetc(f));
		putchar('\n');
	}
}

// CS doc
static void fx_commonstate(psd_file_t f, int level, int len, struct dictentry *parent){
	if(xml){
		fprintf(xml, "%s<VERSION>%d</VERSION>\n", tabs(level), get4B(f));
		fprintf(xml, "%s<VISIBLE>%d</VISIBLE>\n", tabs(level), fgetc(f));
	}
}

// CS doc
static void fx_shadow(psd_file_t f, int level, int len, struct dictentry *parent){
	const char *indent = tabs(level);

	if(xml){
		fprintf(xml, "%s<VERSION>%d</VERSION>\n", indent, get4B(f));
		fprintf(xml, "%s<BLUR>%g</BLUR>\n", indent, FIXEDPT(get4B(f))); // this is fixed point, but you wouldn't know it from the doc
		fprintf(xml, "%s<INTENSITY>%g</INTENSITY>\n", indent, FIXEDPT(get4B(f))); // they're trying to make it more interesting for
		fprintf(xml, "%s<ANGLE>%g</ANGLE>\n", indent, FIXEDPT(get4B(f)));         // implementors, I guess, by setting little puzzles
		fprintf(xml, "%s<DISTANCE>%g</DISTANCE>\n", indent, FIXEDPT(get4B(f)));   // "pit yourself against our documentation!"
		ed_colorspace(f, level, len, parent);
		blendmode(f, level, len, parent);
		fprintf(xml, "%s<ENABLED>%d</ENABLED>\n", indent, fgetc(f));
		fprintf(xml, "%s<USEANGLE>%d</USEANGLE>\n", indent, fgetc(f));
		fprintf(xml, "%s<OPACITY>%g</OPACITY>\n", indent, fgetc(f)/2.55); // doc implies this is a percentage; it's not, it's 0-255 as usual
		ed_colorspace(f, level, len, parent);
	}
}

// CS doc
static void fx_outerglow(psd_file_t f, int level, int len, struct dictentry *parent){
	const char *indent = tabs(level);

	if(xml){
		fprintf(xml, "%s<VERSION>%d</VERSION>\n", indent, get4B(f));
		fprintf(xml, "%s<BLUR>%g</BLUR>\n", indent, FIXEDPT(get4B(f)));
		fprintf(xml, "%s<INTENSITY>%g</INTENSITY>\n", indent, FIXEDPT(get4B(f)));
		ed_colorspace(f, level, len, parent);
		blendmode(f, level, len, parent);
		fprintf(xml, "%s<ENABLED>%d</ENABLED>\n", indent, fgetc(f));
		fprintf(xml, "%s<OPACITY>%g</OPACITY>\n", indent, fgetc(f)/2.55);
		ed_colorspace(f, level, len, parent);
	}
}

// CS doc
static void fx_innerglow(psd_file_t f, int level, int len, struct dictentry *parent){
	const char *indent = tabs(level);
	long version;

	if(xml){
		fprintf(xml, "%s<VERSION>%ld</VERSION>\n", indent, version = get4B(f));
		fprintf(xml, "%s<BLUR>%g</BLUR>\n", indent, FIXEDPT(get4B(f)));
		fprintf(xml, "%s<INTENSITY>%g</INTENSITY>\n", indent, FIXEDPT(get4B(f)));
		ed_colorspace(f, level, len, parent);
		blendmode(f, level, len, parent);
		fprintf(xml, "%s<ENABLED>%d</ENABLED>\n", indent, fgetc(f));
		fprintf(xml, "%s<OPACITY>%g</OPACITY>\n", indent, fgetc(f)/2.55);
		if(version==2)
			fprintf(xml, "%s<INVERT>%d</INVERT>\n", indent, fgetc(f));
		ed_colorspace(f, level, len, parent);
	}
}

// CS doc
static void fx_bevel(psd_file_t f, int level, int len, struct dictentry *parent){
	const char *indent = tabs(level);
	long version;

	if(xml){
		fprintf(xml, "%s<VERSION>%ld</VERSION>\n", indent, version = get4B(f));
		fprintf(xml, "%s<ANGLE>%g</ANGLE>\n", indent, FIXEDPT(get4B(f)));
		fprintf(xml, "%s<STRENGTH>%g</STRENGTH>\n", indent, FIXEDPT(get4B(f)));
		fprintf(xml, "%s<BLUR>%g</BLUR>\n", indent, FIXEDPT(get4B(f)));
		blendmode(f, level, len, parent);
		blendmode(f, level, len, parent);
		ed_colorspace(f, level, len, parent);
		ed_colorspace(f, level, len, parent);
		fprintf(xml, "%s<STYLE>%d</STYLE>\n", indent, fgetc(f));
		fprintf(xml, "%s<HIGHLIGHTOPACITY>%g</HIGHLIGHTOPACITY>\n", indent, fgetc(f)/2.55);
		fprintf(xml, "%s<SHADOWOPACITY>%g</SHADOWOPACITY>\n", indent, fgetc(f)/2.55);
		fprintf(xml, "%s<ENABLED>%d</ENABLED>\n", indent, fgetc(f));
		fprintf(xml, "%s<USEANGLE>%d</USEANGLE>\n", indent, fgetc(f));
		fprintf(xml, "%s<UPDOWN>%d</UPDOWN>\n", indent, fgetc(f)); // heh, interpretation is undocumented
		if(version==2){
			ed_colorspace(f, level, len, parent);
			ed_colorspace(f, level, len, parent);
		}
	}
}

// CS doc
static void fx_solidfill(psd_file_t f, int level, int len, struct dictentry *parent){
	const char *indent = tabs(level);
	long version;

	if(xml){
		fprintf(xml, "%s<VERSION>%ld</VERSION>\n", indent, version = get4B(f));
		// blendmode is the usual 8 bytes; doc only mentions 4
		blendmode(f, level, len, parent);
		ed_colorspace(f, level, len, parent);
		fprintf(xml, "%s<OPACITY>%g</OPACITY>\n", indent, fgetc(f)/2.55);
		fprintf(xml, "%s<ENABLED>%d</ENABLED>\n", indent, fgetc(f));
		ed_colorspace(f, level, len, parent);
	}
}

// CS doc
static void ed_layereffects(psd_file_t f, int level, int len, struct dictentry *parent){
	static struct dictentry fxdict[] = {
		{0, "cmnS", "COMMONSTATE", "common state", fx_commonstate},
		{0, "dsdw", "DROPSHADOW", "drop shadow", fx_shadow},
		{0, "isdw", "INNERSHADOW", "inner shadow", fx_shadow},
		{0, "oglw", "OUTERGLOW", "outer glow", fx_outerglow},
		{0, "iglw", "INNERGLOW", "inner glow", fx_innerglow},
		{0, "bevl", "BEVEL", "bevel", fx_bevel},
		{0, "sofi", "SOLIDFILL", "solid fill", fx_solidfill}, // Photoshop 7.0
		{0, NULL, NULL, NULL, NULL}
	};
	int count;

	if(xml){
		fprintf(xml, "%s<VERSION>%d</VERSION>\n", tabs(level), get2B(f));
		for(count = get2B(f); count--;)
			if(!sigkeyblock(f, NULL/*FIXME*/, level, len, fxdict))
				break; // got bad signature
	}
}

static void mdblock(psd_file_t f, int level, int len){
	char sig[4], key[4];
	long length;
	int copy;
	const char *indent = tabs(level);

	fread(sig, 1, 4, f);
	fread(key, 1, 4, f);
	copy = fgetc(f);
	fseeko(f, 3, SEEK_CUR); // padding
	length = get4B(f);
	if(xml){
		fprintf(xml, "%s<METADATA SIG='", indent);
		fwritexml(sig, 4, xml);
		fputs("' KEY='", xml);
		fwritexml(key, 4, xml);
		fputs("'>\n", xml);
		fprintf(xml, "\t%s<COPY>%d</COPY>\n", indent, copy);
		fprintf(xml, "\t%s<!-- %ld bytes of undocumented data -->\n", indent, length); // the documentation tells us that it's undocumented
		fprintf(xml, "%s</METADATA>\n", indent);
	}else
		UNQUIET("    (Metadata: sig='%c%c%c%c' key='%c%c%c%c' %ld bytes)\n",
				sig[0],sig[1],sig[2],sig[3], key[0],key[1],key[2],key[3], length);
}

// v6 doc
static void ed_metadata(psd_file_t f, int level, int len, struct dictentry *parent){
	long count;

	for(count = get4B(f); count--;)
		mdblock(f, level, len);
}

// see libpsd
static void ed_layer16(psd_file_t f, int level, int len, struct dictentry *parent){
	// This block is an entire layer info section (i.e. it begins with
	// a layer count and is followed by layer info just like the ordinary section).
	// Update the main header struct with the layer count, info pointers, etc.
	dolayerinfo(f, psd_header);
	processlayers(f, psd_header);
}

// v6 doc
static void adj_levels(psd_file_t f, int level, int len, struct dictentry *parent){
	const char *indent = tabs(level);
	int i, j, v[5], version = get2B(f);

	if(xml){
		fprintf(xml, "%s<VERSION>%d</VERSION>\n", indent, version);
		for(i = 0; i < 29; ++i){
			for(j = 0; j < 5; ++j)
				v[j] = get2B(f);

			fprintf(xml, "%s<CHANNEL>\n", indent);
			fprintf(xml, "\t%s<INPUTFLOOR>%d</INPUTFLOOR>\n",   indent, v[0]);
			fprintf(xml, "\t%s<INPUTCEIL>%d</INPUTCEIL>\n",     indent, v[1]);
			fprintf(xml, "\t%s<OUTPUTFLOOR>%d</OUTPUTFLOOR>\n", indent, v[2]);
			fprintf(xml, "\t%s<OUTPUTCEIL>%d</OUTPUTCEIL>\n",   indent, v[3]);
			fprintf(xml, "\t%s<GAMMA>%g</GAMMA>\n",             indent, v[4]/100.);
			fprintf(xml, "%s</CHANNEL>\n", indent);
		}
	}
}

// v6 doc
static void adj_curves(psd_file_t f, int level, int len, struct dictentry *parent){
	const char *indent = tabs(level);
	int i, j, version, count;

	fgetc(f); // mystery data - not mentioned in v6 doc?!
	version = get2B(f);
	get2B(f); // mystery data
	count = get2B(f);
	if(xml){
		fprintf(xml, "%s<VERSION>%d</VERSION>\n", indent, version);
		for(i = 0; i < count; ++i){
			int points = get2B(f);
			fprintf(xml, "%s<CURVE>\n", indent);
			for(j = 0; j < points; ++j){
				int output = get2B(f), input = get2B(f);
				fprintf(xml, "\t%s<POINT> <OUTPUT>%d</OUTPUT> <INPUT>%d</INPUT> </POINT>\n",
						indent, output, input);
			}
			fprintf(xml, "%s</CURVE>\n", indent);
		}
	}
}

// v6 doc
static void adj_huesat4(psd_file_t f, int level, int len, struct dictentry *parent){
	static const char *hsl[] = {"HUE", "SATURATION", "LIGHTNESS"};
	const char *indent = tabs(level);
	int i, j, version, mode;

	version = get2B(f);
	mode = fgetc(f);
	fgetc(f);

	if(xml){
		int h = get2B(f), s = get2B(f), l = get2B(f);
		fprintf(xml, "%s<VERSION>%d</VERSION>\n", indent, version);
		fprintf(xml, "%s<%s/>\n", indent, mode ? "COLORISE" : "HUEADJUST");
		fprintf(xml, "%s<H>%d</H> <S>%d</S> <L>%d</L>\n", indent, h, s, l);
		for(i = 0; i < 3; ++i){
			fprintf(xml, "%s<%s>\n", indent, hsl[i]);
			for(j = 0; j < 7; ++j)
				fprintf(xml, "\t%s<VALUE>%d</VALUE>\n", indent, get2B(f));
			fprintf(xml, "%s</%s>\n", indent, hsl[i]);
		}
	}
}

// v6 doc
static void adj_huesat5(psd_file_t f, int level, int len, struct dictentry *parent){
	const char *indent = tabs(level);
	int i, j, version, mode;

	version = get2B(f);
	mode = fgetc(f);
	fgetc(f);

	if(xml){
		int h = get2B(f), s = get2B(f), l = get2B(f);
		fprintf(xml, "%s<VERSION>%d</VERSION>\n", indent, version);
		fprintf(xml, "%s<%s/>\n", indent, mode ? "COLORISE" : "HUEADJUST");
		fprintf(xml, "%s<H>%d</H> <S>%d</S> <L>%d</L>\n", indent, h, s, l);
		for(i = 0; i < 6; ++i){
			fprintf(xml, "%s<HEXTANT>\n", indent);
			for(j = 0; j < 4; ++j)
				fprintf(xml, "\t%s<RANGE>%d</RANGE>\n", indent, get2B(f));
			for(j = 0; j < 3; ++j)
				fprintf(xml, "\t%s<SETTING>%d</SETTING>\n", indent, get2B(f));
			fprintf(xml, "%s</HEXTANT>\n", indent);
		}
	}
}

// v6 doc
static void adj_selcol(psd_file_t f, int level, int len, struct dictentry *parent){
	const char *indent = tabs(level);
	int i, version = get2B(f);
	static char *colours[] = {"RESERVED", "REDS", "YELLOWS", "GREENS", "CYANS",
							  "BLUES", "MAGENTAS", "WHITES", "NEUTRALS", "BLACKS"};
	fgetc(f);

	if(xml){
		fprintf(xml, "%s<VERSION>%d</VERSION>\n", indent, version);
		for(i = 0; i < 10; ++i){
			fprintf(xml, "%s<%s>", indent, colours[i]);
			fprintf(xml, " <C>%d</C>", get2B(f));
			fprintf(xml, " <M>%d</M>", get2B(f));
			fprintf(xml, " <Y>%d</Y>", get2B(f));
			fprintf(xml, " <K>%d</K>", get2B(f));
			fprintf(xml, " </%s>\n", colours[i]);
		}
	}
}

static void adj_brightnesscontrast(psd_file_t f, int level, int len, struct dictentry *parent){
	const char *indent = tabs(level);

	if(xml){
		fprintf(xml, "%s<BRIGHTNESS>%d</BRIGHTNESS>\n", indent, get2B(f));
		fprintf(xml, "%s<CONTRAST>%d</CONTRAST>\n", indent, get2B(f));
		fprintf(xml, "%s<MEAN>%d</MEAN>\n", indent, get2B(f));
		fprintf(xml, "%s<LABCOLORONLY>%d</LABCOLORONLY>\n", indent, fgetc(f));
	}
}

void doadditional(psd_file_t f, struct psd_header *h, int level, psd_bytes_t length){
	static struct dictentry extradict[] = {
		// v4.0
		{0, "levl", "LEVELS", "Levels", adj_levels},
		{0, "curv", "CURVES", "Curves", adj_curves},
		{0, "brit", "BRIGHTNESSCONTRAST", "Brightness/contrast", adj_brightnesscontrast},
		{0, "blnc", "COLORBALANCE", "Color balance", NULL},
		{0, "hue ", "HUESATURATION4", "Old Hue/saturation, Photoshop 4.0", adj_huesat4},
		{0, "hue2", "HUESATURATION5", "New Hue/saturation, Photoshop 5.0", adj_huesat5},
		{0, "selc", "SELECTIVECOLOR", "Selective color", adj_selcol},
		{0, "thrs", "THRESHOLD", "Threshold", NULL},
		{0, "nvrt", "INVERT", "Invert", NULL},
		{0, "post", "POSTERIZE", "Posterize", NULL},
		// v5.0, 5.5
		{0, "lrFX", "EFFECT", "Effects layer", ed_layereffects},
		{0, "tySh", "TYPETOOL5", "Type tool (5.0)", ed_typetool},
		{0, "luni", "-UNICODENAME", "Unicode layer name", ed_unicodename},
		{0, "lyid", "-LAYERID", "Layer ID", ed_long}, // '-' prefix means keep tag value on one line
		// v6.0
		{0, "lfx2", "OBJECTEFFECT", "Object based effects layer", ed_objecteffects},
		{0, "Patt", "PATTERN", "Pattern", NULL},
		{0, "Pat2", "PATTERNCS", "Pattern (CS)", NULL},
		{0, "Pat3", "PATTERN3", "Pattern (3)", NULL}, // July 2007 doc
		{0, "Anno", "ANNOTATION", "Annotation", ed_annotation},
		{0, "clbl", "-BLENDCLIPPING", "Blend clipping", ed_byte},
		{0, "infx", "-BLENDINTERIOR", "Blend interior", ed_byte},
		{0, "knko", "-KNOCKOUT", "Knockout", ed_byte},
		{0, "lspf", "-PROTECTED", "Protected", ed_long},
		{0, "lclr", "SHEETCOLOR", "Sheet color", ed_color},
		{0, "fxrp", "-REFERENCEPOINT", "Reference point", ed_referencepoint},
		{0, "grdm", "GRADIENTMAP", "Gradient map", ed_gradient},
		{0, "lsct", "SECTIONDIVIDER", "Section divider", ed_sectiondivider}, // CS doc
		{0, "SoCo", "SOLIDCOLORSHEET", "Solid color sheet", ed_versdesc}, // CS doc
		{0, "PtFl", "PATTERNFILL", "Pattern fill", ed_versdesc}, // CS doc
		{0, "GdFl", "GRADIENTFILL", "Gradient fill", ed_versdesc}, // CS doc
		{0, "vmsk", "VECTORMASK", "Vector mask", ed_vectormask}, // CS doc
		{0, "TySh", "TYPETOOL6", "Type tool (6.0)", ed_typetool}, // CS doc
		{0, "ffxi", "-FOREIGNEFFECTID", "Foreign effect ID", ed_long}, // CS doc (this is probably a key too, who knows)
		{0, "lnsr", "-LAYERNAMESOURCE", "Layer name source", ed_key}, // CS doc (who knew this was a signature? docs fail again - and what do the values mean?)
		{0, "shpa", "PATTERNDATA", "Pattern data", NULL}, // CS doc
		{0, "shmd", "METADATASETTING", "Metadata setting", ed_metadata}, // CS doc
		{0, "brst", "BLENDINGRESTRICTIONS", "Channel blending restrictions", ed_blendingrestrictions}, // CS doc
		// v7.0
		{0, "lyvr", "-LAYERVERSION", "Layer version", ed_long}, // CS doc
		{0, "tsly", "-TRANSPARENCYSHAPES", "Transparency shapes layer", ed_byte}, // CS doc
		{0, "lmgm", "-LAYERMASKASGLOBALMASK", "Layer mask as global mask", ed_byte}, // CS doc
		{0, "vmgm", "-VECTORMASKASGLOBALMASK", "Vector mask as global mask", ed_byte}, // CS doc
		// CS
		{0, "mixr", "CHANNELMIXER", "Channel mixer", NULL}, // CS doc
		{0, "phfl", "PHOTOFILTER", "Photo Filter", NULL}, // CS doc
		{0, "expA", "EXPOSURE", "Exposure", NULL}, // July 2007 doc
		{0, "plLd", "PLACEDLAYER", "Placed layer", NULL}, // July 2007 doc
		{0, "lnkD", "LINKEDLAYER", "Linked layer", NULL}, // July 2007 doc
		{0, "lnk2", "LINKEDLAYER2", "Linked layer (2)", NULL}, // July 2007 doc
		{0, "lnk3", "LINKEDLAYER3", "Linked layer (3)", NULL}, // July 2007 doc
		// CS3
		{0, "blwh", "BLACKWHITE", "Black White", NULL}, // July 2007 doc
		{0, "FMsk", "FILTERMASK", "Filter mask", NULL}, // July 2007 doc
		{0, "SoLd", "PLACEDLAYERCS3", "Placed layer (CS3 SoLd)", NULL}, // July 2007 doc
		{0, "PILd", "PLACEDLAYERCS3PILD", "Placed layer (CS3 PILd)", NULL}, // July 2007 doc
		{0, "Txt2", "TEXTENGINEDATA", "Text Engine Data", NULL}, // July 2010 doc

		{0, "Mtrn", "SAVINGMERGEDTRANSPARENCY", "Saving merged transparency", NULL}, // July 2007 doc
		{0, "Mt16", "SAVINGMERGEDTRANSPARENCY16", "Saving merged transparency (16)", NULL}, // July 2007 doc
		{0, "Mt32", "SAVINGMERGEDTRANSPARENCY32", "Saving merged transparency (32)", NULL}, // July 2007 doc
		{0, "LMsk", "USERMASK", "User mask", NULL}, // July 2007 doc
		{0, "FXid", "FILTEREFFECTS", "Filter effects (FXid)", NULL}, // July 2007 doc
		{0, "FEid", "FILTEREFFECTSFEID", "Filter effects (FEid)", NULL}, // July 2007 doc
		// from libpsd
		{0, "Lr16", "LAYER16", "Layer (16)", ed_layer16},

		// CS5 - from July 2010 doc
		{0, "CgEd", "CONTENTGENEXTRADATA", "Content Generator Extra Data", NULL},
		// CS6 (?) - from July 2010 doc
		{0, "vibA", "VIBRANCE", "Vibrance", NULL},

		{0, NULL, NULL, NULL, NULL}
	};

	psd_header = h;

	while(length >= 12){
		psd_bytes_t block = sigkeyblock(f, h, level, length, extradict);
		if(!block){
			warn_msg("bad signature in layer's extra data, skipping the rest");
			break;
		}else if(block > length){
			warn_msg("bad length in sigkeyblock: %d", block);
			break;
		}
		length -= block;
	}
}
