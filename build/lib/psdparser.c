#include <stdio.h>
#include <string.h>
#include <math.h>
#include "psdparser.h"
#include "psdparse.h"
#include "png.h"
#include "zlib.h"

#if defined(WIN32) | defined(Q_WS_WIN)
//#include <direct.h>
//#define mkdir(s, m)	_mkdir(s)
#define DIRSEP	'\\'
#else
//#define mkdir(s, S_IRWXU)
#define DIRSEP	'/'
#endif

//#define ALWAYS_WRITE_PNG
#define DEFAULT_VERBOSE 0

#ifndef max
#define max(a, b)  (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a, b)  (((a) <= (b)) ? (a) : (b))
#endif

extern int nwarns;
extern char indir[];

char *pngdir = indir;
int verbose = DEFAULT_VERBOSE, quiet = 0, rsrc = 0, print_rsrc = 0, resdump = 0, extra = 1,
	scavenge = 0, scavenge_psb = 0, scavenge_depth = 8, scavenge_mode = -1,
	scavenge_rows = 0, scavenge_cols = 0, scavenge_chan = 3, scavenge_rle = 0,
	makedirs = 0, numbered = 0, help = 0, split = 0, xmlout = 0,
	unicode_filenames = 0, rebuild = 0, rebuild_v1 = 0, merged_only = 0;

uint32_t hres, vres; // we don't use these, but they're set within doresources()

#ifdef ALWAYS_WRITE_PNG
// for the Windows console app, we want to be able to drag and drop a PSD
// giving us no way to specify a destination directory, so use a default
int writepng = 1, writelist = 1, writexml = 1;
#else
int writepng = 0, writelist = 0, writexml = 0;
#endif

int g_lid;
PSD_LAYERS_INFO g_info;
static char g_base[PATH_MAX] = {0};

static void close_parser(FILE *fp, struct psd_header *hdr)
{
	if (hdr->linfo)
	{
		free(hdr->linfo);
		hdr->linfo = NULL;
	}

	if (fp)
	{
		fclose(fp);
		fp = NULL;
	}
}

int is_photo(const char *name)
{
	char buf[7] = {0};
	strncpy(buf, &name[strlen(name) - 6], 6);
	if (!stricmp(buf, "@photo"))
	{
		return 1;
	}
	return 0;
}

double calculate_angle(void *li)
{
	int refX, refY, sign = 0, delta = 0;
	double sx, si, s1 = 0, s2 = 0, b = 0, c = -1, r, angle = 0, X = 0, x1, x2;
	const double degree = 180 / 3.1415926535, arc = 3.1415926535 / 180, a = -1;
	PSD_LAYER_INFO *layer = (PSD_LAYER_INFO *)li;

	if (!layer)
	{
		return 0;
	}

	refX = 0 < layer->ref.x ? layer->ref.x + 0.5 : layer->ref.x - 0.5;
	refY = 0 < layer->ref.y ? layer->ref.y + 0.5 : layer->ref.y - 0.5;

	do
	{
		if (layer->left == refX - delta)	// Anticlockwise rotation(0бу~ 90бу)
		{
			sign = -1;
			s1 = layer->ref.y - layer->top;
			s2 = layer->bound.height - s1;
			b = layer->bound.width;
			break;
		}

		if (layer->bottom == refY + delta)	// Anticlockwise rotation(90бу~ 180бу)
		{
			sign = -1;
			s1 = layer->ref.x - layer->left;
			s2 = layer->bound.width - s1;
			b = layer->bound.height;
			break;
		}

		if (layer->top == refY - delta)	// Clockwise rotation(0бу~ 90бу)
		{
			sign = 1;
			s1 = layer->ref.x - layer->left;
			s2 = layer->bound.width - s1;
			b = layer->bound.height;
			break;
		}

		if (layer->right == refX + delta)	// Clockwise rotation(90бу~ 180бу)
		{
			sign = 1;
			s1 = layer->ref.y - layer->top;
			s2 = layer->bound.height - s1;
			b = layer->bound.width;
			break;
		}

		delta++;
	} while (3 >= delta);

	if ((!layer->top && !layer->left) || (!layer->ref.x && !layer->ref.y) || (!s1 && !s2) || 0 > s1 || 0 > s2)
	{
		goto end;
	}

	c *= s1 * s2;
	r = b * b - 4 * a * c;

	if (0 > r)
	{
		goto end;
	}
	else if (0 == r)
	{
		x1 = x2 = -b / (2 * a);
	}
	else
	{
		r = sqrt(r);
		x1 = (-b + r) / (2 * a);
		x2 = (-b - r) / (2 * a);
	}

	sx = max(s1, s2);
	si = min(s1, s2);

	if (0 > sign)
	{
		if (s1 > s2)
		{
			X = x2;
		}
		else
		{
			X = 0 > x1 ? -1 * x1 : x1;
		}

		if (b < X + delta)
		{
			goto end;
		}

		if (layer->bound.height != b)
		{
			angle = atan2(X, s2) * degree;
		}
		else
		{
			angle = atan2(s1, X) * degree;
		}

		layer->actual.width = (unsigned int)(si / sin(angle * arc));
		layer->actual.height = (unsigned int)(sx / cos(angle * arc));

		if (layer->bound.height == b)
		{
			angle = 180 - angle;
		}
	}
	else
	{
		if (s1 > s2)
		{
			X = 0 > x1 ? -1 * x1 : x1;
		}
		else
		{
			X = x2;
		}

		if (b < X + delta)
		{
			goto end;
		}

		if (layer->bound.width != b)
		{
			angle = atan2(s1, X) * degree;
			layer->actual.width = (unsigned int)(sx / cos(angle * arc));
			layer->actual.height = (unsigned int)(si / sin(angle * arc));
		}
		else
		{
			angle = atan2(X, s1) * degree;
			layer->actual.width = (unsigned int)(sx / sin(angle * arc));
			layer->actual.height = (unsigned int)(si / cos(angle * arc));
			angle = 180 - angle;
		}
	}

end:
	if (!layer->actual.width)
	{
		layer->actual.width = layer->bound.width;
	}

	if (!layer->actual.height)
	{
		layer->actual.height = layer->bound.height;
	}

	return (angle ? angle * sign : angle);
}

#if defined(__cplusplus)    // Mainly to support C++ specification
extern "C"
{
#endif

    PSDPARSER int psd_parse(PSD_LAYERS_INFO *info)
    {
		struct stat st;
		static struct psd_header hdr;
		static FILE *fp = NULL;
		char temp_str[PATH_MAX] = {0};
		char *base;
		int fd, map_flag, i;
		void *addr = NULL;
		psd_bytes_t n, k, offset;

		if (!info || !(fp = fopen(info->file, "rb")))
		{
			printf("can't open file \"%s\"!\n", info->file);
			return -1;
		}

		fd = fileno(fp);
		map_flag = (scavenge || scavenge_psb || scavenge_rle) && fstat(fd, &st) == 0 && (st.st_mode & S_IFMT) == S_IFREG;
		if (map_flag && !(addr = map_file(fd, st.st_size)))
		{
			fclose(fp);
			fp = NULL;
			return -1;
		}

		if (g_info.count)
		{
			if (g_info.layers)
			{
				for (i = 0; i < g_info.count; i++)
				{
					memset(&g_info.layers[i], 0, sizeof(PSD_LAYER_INFO));
				}

				free(g_info.layers);
				g_info.layers = NULL;
			}

			memset(g_info.file, 0, 260);
			memset(&g_info, 0, sizeof(PSD_LAYERS_INFO));
		}

		memset(&st, 0, sizeof(st));
		memset(&hdr, 0, sizeof(hdr));
		memset(g_base, 0, PATH_MAX);

		strcpy(g_info.file, info->file);
		base = strrchr(info->file, DIRSEP);
		strcpy(g_base, base ? base + 1 : info->file);

		if ((scavenge || scavenge_psb) && addr)
		{
			hdr.version = 1 + scavenge_psb;
			hdr.channels = scavenge_chan;
			hdr.rows = scavenge_rows;
			hdr.cols = scavenge_cols;
			hdr.depth = scavenge_depth;
			hdr.mode = scavenge_mode;
			scavenge_psd(addr, st.st_size, &hdr);

			openfiles(info->file, &hdr);

			if(xml){
				fputs("<PSD FILE='", xml);
				fputsxml(info->file, xml);
				fputs("'>\n", xml);
			}

			for (i = 0; i < hdr.nlayers; ++i)
			{
				fseeko(fp, hdr.linfo[i].filepos, SEEK_SET);
				readlayerinfo(fp, &hdr, i);
			}

			hdr.layerdatapos = (psd_bytes_t)ftello(fp);

			// Layer content starts immediately after the last layer's 'metadata'.
			// If we did not correctly locate the *last* layer, we are not going to
			// succeed in extracting data for any layer.
			processlayers(fp, &hdr);

			// if no layers found, try to locate merged data
			if(!hdr.nlayers && hdr.rows && hdr.cols && hdr.lmistart)
			{
				// position file after 'layer & mask info'
				fseeko(fp, hdr.lmistart + hdr.lmilen, SEEK_SET);
				// process merged (composite) image data
				doimage(fp, NULL, g_base, &hdr);
			}
		}
		else
		{
			if (dopsd(fp, info->file, &hdr))
			{
				VERBOSE("## layer image data begins @ " LL_L("%lld","%ld") "\n", hdr.layerdatapos);

				info->count = g_info.count = hdr.nlayers;

				if (16 > strlen(mode_names[hdr.mode]))
				{
					strcpy(g_info.color_mode, mode_names[hdr.mode]);
				}
				else
				{
					strncpy(g_info.color_mode, mode_names[hdr.mode], 15);
				}
				strcpy(info->color_mode, g_info.color_mode);

				g_info.layers = (PSD_LAYER_INFO *)calloc(hdr.nlayers, sizeof(PSD_LAYER_INFO));
				info->layers = g_info.layers;

				// process the layers in 'image data' section,
				// creating PNG/raw files if requested
				processlayers(fp, &hdr);

				// skip 1 byte of padding if we are not at an even position
				offset = (psd_bytes_t)ftello(fp);
				if (offset & 1)
					fgetc(fp);

				n = globallayermaskinfo(fp, &hdr);

				// global 'additional info' (not really documented)
				// this is found immediately after the 'image data' section
				offset = (psd_bytes_t)ftello(fp);
				k = hdr.lmistart + hdr.lmilen - offset;
				if ((extra || hdr.depth > 8) && offset < (hdr.lmistart + hdr.lmilen))
				{
					VERBOSE("## global additional info @ %ld (%ld bytes)\n", (long)offset, (long)k);
					
					if(xml)
						fputs("\t<GLOBALINFO>\n", xml);

					doadditional(fp, &hdr, 2, k); // write description to XML

					if(xml)
						fputs("\t</GLOBALINFO>\n", xml);
				}

				// position file after 'layer & mask info'
				fseeko(fp, hdr.lmistart + hdr.lmilen, SEEK_SET);

				// process merged (composite) image data
				doimage(fp, NULL, g_base, &hdr);
			}
		}

		if (scavenge_rle && hdr.nlayers && addr)
		{
			scan_channels((unsigned char *)addr, st.st_size, &hdr);

			// process scavenged layer channel data
			for (i = 0; i < hdr.nlayers; ++i)
			{
				if (hdr.linfo[i].chpos)
				{
					UNQUIET("layer %d: using scavenged pos @ %lu\n", i, (unsigned long)hdr.linfo[i].chpos);

					strcpy(temp_str, numbered ? hdr.linfo[i].nameno : hdr.linfo[i].name);
					strcat(temp_str, ".scavenged");
					fseeko(fp, hdr.linfo[i].chpos, SEEK_SET);
					doimage(fp, &hdr.linfo[i], temp_str, &hdr);
				}
			}
		}

		if (map_flag)
			unmap_file(addr, st.st_size); // needed for Windows cleanup, even if mmap() failed

		if(xml){
			fputs("</PSD>\n", xml);
			fclose(xml);
		}

		UNQUIET("  done.\n\n");

		//if (rebuild || rebuild_v1)
		//	rebuild_psd(fp, rebuild_v1 ? 1 : hdr.version, &hdr);

		close_parser(fp, &hdr);

		return 0;
    }

	PSDPARSER int psd_to_png(PSD_LAYERS_INFO *info)
	{
		struct psd_header hdr;
		FILE *fp = NULL;
		int i;

		if (!(fp = fopen(g_info.file, "rb")) || 0 >= g_info.count)
		{
			return -1;
		}

		if (info)
		{
			for (i = 0; i < info->count; i++)
			{
				strncpy(g_info.layers[i].lid, info->layers[i].lid, 36);
			}
		}

		memset(&hdr, 0, sizeof(hdr));

		if (dopsd(fp, g_info.file, &hdr))
		{
			writepng = 1;
			processlayers(fp, &hdr);
			fseeko(fp, hdr.lmistart + hdr.lmilen, SEEK_SET);
			doimage(fp, NULL, g_base, &hdr);
			info->count = g_info.count;
			info->layers = g_info.layers;
			writepng = 0;
		}

		close_parser(fp, &hdr);

		return 0;
	}

	PSDPARSER void psd_release(PSD_LAYERS_INFO *info)
	{
		if (info)
		{
			if (info->layers)
			{
				free(info->layers);
				g_info.layers = info->layers = NULL;
			}

			free(info);
			info = NULL;
		}

		g_lid = 0;
		memset(g_info.file, 0, 260);
		memset(&g_info, 0, sizeof(PSD_LAYERS_INFO));
	}

#if defined(__cplusplus)
}
#endif
