#ifndef PSDPARSER_H_
#define PSDPARSER_H_

#ifdef PSDPARSER_EXPORTS
#define PSDPARSER __declspec(dllexport)
#else
#define PSDPARSER __declspec(dllimport)
#endif

#define MAX_UUID_LEN	36

enum layer_type{LT_Background, LT_Photo, LT_Decoration, LT_Mask};

typedef struct PSD_LAYER_INFO_Tag
{
	unsigned char canvas;
	enum layer_type type;
	char name[96], lid[MAX_UUID_LEN + 1]/* Filled by caller at external */, mid[MAX_UUID_LEN + 1]/* Mask layer id, filled at internal */;
	int top, left, bottom, right;
	short channels;
    float opacity;
    double angle;

	struct size
	{
		unsigned int width, height;
	} bound, actual;

	struct point
	{
		double x, y;
	} ref, center;
} PSD_LAYER_INFO;

typedef struct PSD_LAYERS_INFO_Tag
{
	int count;
	char color_mode[16];
	char file[260];
	PSD_LAYER_INFO *layers;
} PSD_LAYERS_INFO;

#if defined(__cplusplus)
extern "C"
{
#endif

    PSDPARSER int psd_parse(PSD_LAYERS_INFO *info);

	PSDPARSER int psd_to_png(PSD_LAYERS_INFO *info);	// Doesn't chnage the png file name if info is null

	PSDPARSER void psd_release(PSD_LAYERS_INFO *info);

#if defined(__cplusplus)
}
#endif
#endif	// PSDPARSER_H_
