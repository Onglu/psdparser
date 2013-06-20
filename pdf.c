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

/* This parser is just a hack to process the subset of PDF found
 * in text descriptors. It's not written for elegance or generality!
 */

#include "psdparse.h"

#define ASCII_LF 012
#define ASCII_CR 015

#define MAX_NAMES 32
#define MAX_DICTS 32 // dict/array nesting limit

int is_pdf_white(char c){
	return c == '\000' || c == '\011' || c == '\012'
		|| c == '\014' || c == '\015' || c == '\040';
}

int is_pdf_delim(char c){
	return c == '(' || c == ')' || c == '<' || c == '>'
		|| c == '[' || c == ']' || c == '{' || c == '}'
		|| c == '/' || c == '%';
}

// p      : pointer to first character following opening ( of string
// outbuf : destination buffer for parsed string. pass NULL to count but not store
// n      : count of characters available in input buffer
// returns number of characters in parsed string
// updates the source pointer to the first character after the string

size_t pdf_string(char **p, char *outbuf, size_t n){
	int paren = 1;
	size_t cnt;
	char c;

	for(cnt = 0; n;){
		--n;
		switch(c = *(*p)++){
		case ASCII_CR:
			if(n && (*p)[0] == ASCII_LF){ // eat the linefeed in a cr/lf pair
				++(*p);
				--n;
			}
			c = ASCII_LF;
		case ASCII_LF:
			break;
		case '(':
			++paren;
			break;
		case ')':
			if(!(--paren))
				return cnt; // it was the closing paren
			break;
		case '\\':
			if(!n)
				return cnt; // ran out of data
			--n;
			switch(c = *(*p)++){
			case ASCII_CR: // line continuation; skip newline
				if(n && (*p)[0] == ASCII_LF){ // eat the linefeed in a cr/lf pair
					++(*p);
					--n;
				}
			case ASCII_LF:
				continue;
			case 'n': c = ASCII_LF; break;
			case 'r': c = ASCII_CR; break;
			case 't': c = 011; break; // horizontal tab
			case 'b': c = 010; break; // backspace
			case 'f': c = 014; break; // formfeed
			case '(':
			case ')':
			case '\\': c = (*p)[-1]; break;
			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7':
				// octal escape
				c = (*p)[-1] - '0';
				if(n >= 1 && isdigit((*p)[0])){
					c = (c << 3) | ((*p)[0] - '0');
					++p;
					--n;
					if(n >= 1 && isdigit((*p)[0])){
						c = (c << 3) | ((*p)[0] - '0');
						++p;
						--n;
					}
				}
				break;
			}
		}
		if(outbuf)
			*outbuf++ = c;
		++cnt;
	}
	return cnt;
}

// parameters analogous to pdf_string()'s
size_t pdf_hexstring(char **p, char *outbuf, size_t n){
	size_t cnt, flag;
	unsigned acc;

	for(cnt = acc = flag = 0; n;){
		char c = *(*p)++;
		--n;
		if(c == '>'){ // or should this be pdf_delim() ?
			// check for partial byte
			if(flag){
				if(outbuf)
					*outbuf++ = acc;
				++cnt;
			}
			break;
		}else if(!is_pdf_white(c)){ // N.B. DOES NOT CHECK for valid hex digits!
			acc |= hexdigit(c);
			if(flag){
				// both nibbles loaded; emit character
				if(outbuf)
					*outbuf++ = acc;
				++cnt;
				flag = acc = 0;
			}else{
				acc <<= 4;
				flag = 1; // high nibble loaded
			}
		}
	}
	return cnt;
}

// parameters analogous to pdf_string()'s
size_t pdf_name(char **p, char *outbuf, size_t n){
	size_t cnt;

	for(cnt = 0; n;){
		char c = *(*p);
		if(is_pdf_white(c) || is_pdf_delim(c))
			break;
		else if(c == '#' && n >= 3){
			// process #XX hex escape
			c = (hexdigit((*p)[1]) << 4) | hexdigit((*p)[2]);
			*p += 3;
			n -= 3;
		}else{
			// consume character
			++(*p);
			--n;
		}
		if(outbuf)
			*outbuf++ = c;
		++cnt;
	}
	return cnt;
}

static char *name_stack[MAX_NAMES];
static unsigned name_tos, in_array;

void push_name(char *tag){
	if(name_tos == MAX_NAMES)
		fatal("name stack overflow");
	name_stack[name_tos++] = tag;
}

void pop_name(){
	if(name_tos)
		free(name_stack[--name_tos]);
	else
		warn_msg("pop_name(): underflow");
}

// Write a string representation to XML. Either convert to UTF-8
// from the UTF-16BE if flagged as such by a BOM prefix,
// or just write the literal string bytes without transliteration.

/*
From PDF Reference 1.7, 3.8.1 Text String Type

The text string type is used for character strings that are encoded in either PDFDocEncoding
or the UTF-16BE Unicode character encoding scheme. PDFDocEncoding
can encode all of the ISO Latin 1 character set and is documented in Appendix D. ...

For text strings encoded in Unicode, the first two bytes must be 254 followed by 255.
These two bytes represent the Unicode byte order marker, U+FEFF,
indicating that the string is encoded in the UTF-16BE (big-endian) encoding scheme ...

Note: Applications that process PDF files containing Unicode text strings
should be prepared to handle supplementary characters;
that is, characters requiring more than two bytes to represent.
*/

void stringxml(char *strbuf, size_t cnt){
	if(cnt >= 2 && (strbuf[0] & 0xff) == 0xfe && (strbuf[1] & 0xff) == 0xff){
#ifdef HAVE_ICONV_H
		size_t inb, outb;
		char *inbuf, *outbuf, *utf8;

		iconv(ic, NULL, &inb, NULL, &outb); // reset iconv state

		outb = 6*(cnt/2); // sloppy overestimate of buffer (FIXME)
		if( (utf8 = checkmalloc(outb)) ){
			// skip the meaningless BOM
			inbuf = strbuf + 2;
			inb = cnt - 2;
			outbuf = utf8;
			if(ic != (iconv_t)-1){
				if(iconv(ic, &inbuf, &inb, &outbuf, &outb) != (size_t)-1)
					fwritexml(utf8, outbuf-utf8, xml);
				else
					alwayswarn("stringxml(): iconv() failed, errno=%u\n", errno);
			}
			free(utf8);
		}
#endif
	}
	else
		fputsxml((char*)strbuf, xml); // not UTF; should be PDFDocEncoded
}

void begin_element(const char *indent){
	if(in_array)
		fprintf(xml, "%s<e>", indent);
	else if(name_tos)
		fprintf(xml, "%s<%s>", indent, name_stack[name_tos-1]);
}

void end_element(const char *indent){
	if(in_array){
		fprintf(xml, "%s</e>\n", indent);
	}
	else if(name_tos){
		fprintf(xml, "%s</%s>\n", indent, name_stack[name_tos-1]);
		pop_name();
	}
}

// Implements a "ghetto" PDF syntax parser - just the minimum needed
// to translate Photoshop's embedded type tool data into XML.

// PostScript implements a single heterogenous stack; we don't try
// to emulate proper behaviour here but rather keep a 'stack' of names
// only in order to generate correct closing tags,
// and remember whether the 'current' object is a dictionary or array.

static void pdf_data(char *buf, size_t n, int level){
	char *p, *q, *strbuf, c;
	size_t cnt;
	unsigned is_array[MAX_DICTS], dict_tos = 0;

	name_tos = in_array = 0;
	for(p = buf; n;){
		c = *p++;
		--n;
		switch(c){
		case '(':
			// String literal. Copy the string content to XML
			// as element content.

			// check parsed string length
			q = p;
			cnt = pdf_string(&q, NULL, n);

			// parse string into new buffer, and step past in source buffer
			strbuf = checkmalloc(cnt+1);
			q = p;
			pdf_string(&p, strbuf, n);
			n -= p - q;
			strbuf[cnt] = 0;

			begin_element(tabs(level));
			stringxml(strbuf, cnt);
			end_element("");

			free(strbuf);
			break;

		case '<':
			if(n && *p == '<'){ // dictionary literal
				++p;
				--n;
		case '[':
				begin_element(tabs(level));
				if(name_tos){
					fputc('\n', xml);
					++level;
				}

				if(dict_tos == MAX_DICTS)
					fatal("dict stack overflow");
				is_array[dict_tos++] = in_array = c == '[';
			}
			else{ // hex string literal
				q = p;
				cnt = pdf_hexstring(&q, NULL, n);

				strbuf = checkmalloc(cnt+1);
				q = p;
				pdf_hexstring(&p, strbuf, n);
				n -= p - q;
				strbuf[cnt] = 0;

				begin_element(tabs(level));
				stringxml(strbuf, cnt);
				end_element("");

				free(strbuf);
			}
			break;

		case '>':
			if(n && *p == '>'){
				++p;
				--n;
			}
			else{
				warn_msg("misplaced >");
				continue;
			}
		case ']':
			if(dict_tos)
				--dict_tos;
			else
				warn_msg("dict stack underflow");
			in_array = dict_tos && is_array[dict_tos-1];

			end_element(tabs(--level));
			break;

		case '/':
			// check parsed name length
			q = p;
			cnt = pdf_name(&q, NULL, n);

			// parse name into new buffer, and step past in source buffer
			strbuf = checkmalloc(cnt+1);
			q = p;
			pdf_name(&p, strbuf, n);
			strbuf[cnt] = 0;
			n -= p - q;

			// FIXME: This won't work correctly if a name is given as a
			//        value for a dictionary key (we need to track of
			//        whether name is key or value)
			if(in_array){
				begin_element(tabs(level));
				stringxml(strbuf, cnt);
				end_element("");
				free(strbuf);
			}
			else{ // it's a dictionary key
				// FIXME: we need to deal with zero-length key, and
				//        characters unsuitable for XML element name.
				push_name(strbuf);
			}
			break;

		case '%': // skip comment
			while(n && *p != ASCII_CR && *p != ASCII_LF){
				++p;
				--n;
			}
			break;

		default:
			if(!is_pdf_white(c)){
				// numeric or boolean literal, or null
				// use characters until whitespace or delimiter
				q = p-1;
				while(n && !is_pdf_white(*p) && !is_pdf_delim(*p)){
					++p;
					--n;
				}

				c = *p;
				*p = 0;
				// If a dictionary value has value null, the key
				// should not be created. (7.3.7)
				if(in_array || strcmp(q, "null")){
					begin_element(tabs(level));
					fputs(q, xml);
					end_element("");
				}
				*p = c;
			}
			break;
		}
	}

	// close any open elements (should not happen)
	while(name_tos){
		warn_msg("unclosed element %s", name_stack[name_tos-1]);
		pop_name();
	}
}

void desc_pdf(psd_file_t f, int level, int printxml, struct dictentry *parent){
	long count = get4B(f);
	char *buf = checkmalloc(count);

	if(buf){
		pdf_data(buf, fread(buf, 1, count, f), level);

		free(buf);
	}
}
