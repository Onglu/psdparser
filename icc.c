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

double s15fixed16(psd_file_t f){ return get4B(f)/65536.; }

static void icc_xyz(psd_file_t f, int level, int len, struct dictentry *parent){
	for(; len >= 12; len -= 12){
		fprintf(xml, " <X>%g</X>",  s15fixed16(f));
		fprintf(xml, " <Y>%g</Y>",  s15fixed16(f));
		fprintf(xml, " <Z>%g</Z> ", s15fixed16(f));
	}
}

static void icc_text(psd_file_t f, int level, int len, struct dictentry *parent){
	--len; // exclude terminating NUL
	while(len--)
		fputcxml(fgetc(f), xml);
}

static void icc_textdescription(psd_file_t f, int level, int len, struct dictentry *parent){
	long count = get4B(f)-1; // exclude terminating NUL
	while(count--)
		fputcxml(fgetc(f), xml);
	// ignore other fields of this tag
}

static void icc_rawsignature(psd_file_t f, int level, int len, struct dictentry *parent){
	fputsxml(getkey(f), xml);
}
static void icc_signature(psd_file_t f, int level, int len, struct dictentry *parent){
	static struct dictentry sigdict[] = {
		// technology
	    {0, "dcam", "DigitalCamera", "icSigDigitalCamera", NULL},
	    {0, "fscn", "FilmScanner", "icSigFilmScanner", NULL},
	    {0, "rscn", "ReflectiveScanner", "icSigReflectiveScanner", NULL},
	    {0, "ijet", "InkJetPrinter", "icSigInkJetPrinter", NULL},
	    {0, "twax", "ThermalWaxPrinter", "icSigThermalWaxPrinter", NULL},
	    {0, "epho", "ElectrophotographicPrinter", "icSigElectrophotographicPrinter", NULL},
	    {0, "esta", "ElectrostaticPrinter", "icSigElectrostaticPrinter", NULL},
	    {0, "dsub", "DyeSublimationPrinter", "icSigDyeSublimationPrinter", NULL},
	    {0, "rpho", "PhotographicPaperPrinter", "icSigPhotographicPaperPrinter", NULL},
	    {0, "fprn", "FilmWriter", "icSigFilmWriter", NULL},
	    {0, "vidm", "VideoMonitor", "icSigVideoMonitor", NULL},
	    {0, "vidc", "VideoCamera", "icSigVideoCamera", NULL},
	    {0, "pjtv", "ProjectionTelevision", "icSigProjectionTelevision", NULL},
	    {0, "CRT ", "CRTDisplay", "icSigCRTDisplay", NULL},
	    {0, "PMD ", "PMDisplay", "icSigPMDisplay", NULL},
	    {0, "AMD ", "AMDisplay", "icSigAMDisplay", NULL},
	    {0, "KPCD", "PhotoCD", "icSigPhotoCD", NULL},
	    {0, "imgs", "PhotoImageSetter", "icSigPhotoImageSetter", NULL},
	    {0, "grav", "Gravure", "icSigGravure", NULL},
	    {0, "offs", "OffsetLithography", "icSigOffsetLithography", NULL},
	    {0, "silk", "Silkscreen", "icSigSilkscreen", NULL},
	    {0, "flex", "Flexography", "icSigFlexography", NULL},
	    // ...add other categories as required
		{0, NULL, NULL, NULL, NULL}
	};

	findbykey(f, level, sigdict, getkey(f), len, 1);
}

static void icc_datetime(psd_file_t f, int level, int len, struct dictentry *parent){
	int y = get2B(f), mo = get2B(f), d = get2B(f), h = get2B(f), m = get2B(f), s = get2B(f);
	fprintf(xml, "%04d-%02d-%02d %02d:%02d:%02d", y, mo, d, h, m, s);
}

static void icc_tag(psd_file_t f, int level, int len, struct dictentry *parent){
	static struct dictentry typedict[] = {
	    {0, "curv", "Curve", "icSigCurveType", NULL},
	    {0, "data", "Data", "icSigDataType", NULL},
	    {0, "dtim", "-DateTime", "icSigDateTimeType", icc_datetime},
	    {0, "mft2", "Lut16", "icSigLut16Type", NULL},
	    {0, "mft1", "Lut8", "icSigLut8Type", NULL},
	    {0, "meas", "Measurement", "icSigMeasurementType", NULL},
	    {0, "ncol", "NamedColor", "icSigNamedColorType", NULL},
	    {0, "sf32", "S15Fixed16Array", "icSigS15Fixed16ArrayType", NULL},
	    {0, "scrn", "Screening", "icSigScreeningType", NULL},
	    {0, "sig ", "Signature", "icSigSignatureType", icc_signature},
	    {0, "text", "-Text", "icSigTextType", icc_text},
	    {0, "desc", "-TextDescription", "icSigTextDescriptionType", icc_textdescription},
	    {0, "uf32", "U16Fixed16Array", "icSigU16Fixed16ArrayType", NULL},
	    {0, "bfd ", "UcrBg", "icSigUcrBgType", NULL},
	    {0, "ui16", "UInt16Array", "icSigUInt16ArrayType", NULL},
	    {0, "ui32", "UInt32Array", "icSigUInt32ArrayType", NULL},
	    {0, "ui64", "UInt64Array", "icSigUInt64ArrayType", NULL},
	    {0, "ui08", "UInt8Array", "icSigUInt8ArrayType", NULL},
	    {0, "view", "ViewingConditions", "icSigViewingConditionsType", NULL},
	    {0, "XYZ ", "-XYZ", "icSigXYZType", icc_xyz},
	    //{0, "XYZ ", "-XYZArray", "icSigXYZArrayType", icc_xyz},
	    {0, "ncl2", "NamedColor2", "icSigNamedColor2Type", NULL},
	    {0, "crdi", "CrdInfo", "icSigCrdInfoType", NULL},
		{0, NULL, NULL, NULL, NULL}
	};

	char *key = getkey(f);
	get4B(f); // skip reserved field
	findbykey(f, level, typedict, key, len-8, 1);
}

void ir_icc34profile(psd_file_t f, int level, int len, struct dictentry *parent){
	// ([a-z]+)\s+[^']*\'(.*)\'.*$
	// {0, "\2", "\1", "\1", NULL},
	static struct dictentry classdict[] = {
	    {0, "scnr", "InputClass", "icSigInputClass", NULL},
	    {0, "mntr", "DisplayClass", "icSigDisplayClass", NULL},
	    {0, "prtr", "OutputClass", "icSigOutputClass", NULL},
	    {0, "link", "LinkClass", "icSigLinkClass", NULL},
	    {0, "abst", "AbstractClass", "icSigAbstractClass", NULL},
	    {0, "spac", "ColorSpaceClass", "icSigColorSpaceClass", NULL},
	    {0, "nmcl", "NamedColorClass", "icSigNamedColorClass", NULL},
		{0, NULL, NULL, NULL, NULL}
	};
	static struct dictentry platdict[] = {
		{0, "APPL", "Macintosh", "Apple Computer, Inc.", NULL},
		{0, "MSFT", "Microsoft", "Microsoft Corporation", NULL},
		{0, "SGI ", "SGI", "Silicon Graphics, Inc.", NULL},
		{0, "SUNW", "Solaris", "Sun Microsystems, Inc.", NULL},
		{0, "TGNT", "Taligent", "Taligent, Inc.", NULL},
		{0, NULL, NULL, NULL, NULL}
	};
	static struct dictentry spacedict[] = {
	    {0, "XYZ ", "XYZData", "icSigXYZData", NULL},
	    {0, "Lab ", "LabData", "icSigLabData", NULL},
	    {0, "Luv ", "LuvData", "icSigLuvData", NULL},
	    {0, "YCbr", "YCbCrData", "icSigYCbCrData", NULL},
	    {0, "Yxy ", "YxyData", "icSigYxyData", NULL},
	    {0, "RGB ", "RgbData", "icSigRgbData", NULL},
	    {0, "GRAY", "GrayData", "icSigGrayData", NULL},
	    {0, "HSV ", "HsvData", "icSigHsvData", NULL},
	    {0, "HLS ", "HlsData", "icSigHlsData", NULL},
	    {0, "CMYK", "CmykData", "icSigCmykData", NULL},
	    {0, "CMY ", "CmyData", "icSigCmyData", NULL},
	    {0, "2CLR", "icSig2colorData", "icSig2colorData", NULL},
	    {0, "3CLR", "icSig3colorData", "icSig3colorData", NULL},
	    {0, "4CLR", "icSig4colorData", "icSig4colorData", NULL},
	    {0, "5CLR", "icSig5colorData", "icSig5colorData", NULL},
	    {0, "6CLR", "icSig6colorData", "icSig6colorData", NULL},
	    {0, "7CLR", "icSig7colorData", "icSig7colorData", NULL},
	    {0, "8CLR", "icSig8colorData", "icSig8colorData", NULL},
	    {0, "9CLR", "icSig9colorData", "icSig9colorData", NULL},
	    {0, "ACLR", "icSig10colorData", "icSig10colorData", NULL},
	    {0, "BCLR", "icSig11colorData", "icSig11colorData", NULL},
	    {0, "CCLR", "icSig12colorData", "icSig12colorData", NULL},
	    {0, "DCLR", "icSig13colorData", "icSig13colorData", NULL},
	    {0, "ECLR", "icSig14colorData", "icSig14colorData", NULL},
	    {0, "FCLR", "icSig15colorData", "icSig15colorData", NULL},
		{0, NULL, NULL, NULL, NULL}
	};
	static struct dictentry tagdict[] = {
	    {0, "A2B0", "AToB0", "icSigAToB0Tag", icc_tag},
	    {0, "A2B1", "AToB1", "icSigAToB1Tag", icc_tag},
	    {0, "A2B2", "AToB2", "icSigAToB2Tag", icc_tag},
	    {0, "bXYZ", "BlueColorant", "icSigBlueColorantTag", icc_tag},
	    {0, "bTRC", "BlueTRC", "icSigBlueTRCTag", icc_tag},
	    {0, "B2A0", "BToA0", "icSigBToA0Tag", icc_tag},
	    {0, "B2A1", "BToA1", "icSigBToA1Tag", icc_tag},
	    {0, "B2A2", "BToA2", "icSigBToA2Tag", icc_tag},
	    {0, "calt", "CalibrationDateTime", "icSigCalibrationDateTimeTag", icc_tag},
	    {0, "targ", "CharTarget", "icSigCharTargetTag", icc_tag},
	    {0, "cprt", "Copyright", "icSigCopyrightTag", icc_tag},
	    {0, "crdi", "CrdInfo", "icSigCrdInfoTag", icc_tag},
	    {0, "dmnd", "DeviceMfgDesc", "icSigDeviceMfgDescTag", icc_tag},
	    {0, "dmdd", "DeviceModelDesc", "icSigDeviceModelDescTag", icc_tag},
	    {0, "gamt ", "Gamut", "icSigGamutTag", icc_tag},
	    {0, "kTRC", "GrayTRC", "icSigGrayTRCTag", icc_tag},
	    {0, "gXYZ", "GreenColorant", "icSigGreenColorantTag", icc_tag},
	    {0, "gTRC", "GreenTRC", "icSigGreenTRCTag", icc_tag},
	    {0, "lumi", "Luminance", "icSigLuminanceTag", icc_tag},
	    {0, "meas", "Measurement", "icSigMeasurementTag", icc_tag},
	    {0, "bkpt", "MediaBlackPoint", "icSigMediaBlackPointTag", icc_tag},
	    {0, "wtpt", "MediaWhitePoint", "icSigMediaWhitePointTag", icc_tag},
	    {0, "ncol", "NamedColor", "icSigNamedColorTag", icc_tag},
	    {0, "pre0", "Preview0", "icSigPreview0Tag", icc_tag},
	    {0, "pre1", "Preview1", "icSigPreview1Tag", icc_tag},
	    {0, "pre2", "Preview2", "icSigPreview2Tag", icc_tag},
	    {0, "desc", "ProfileDescription", "icSigProfileDescriptionTag", icc_tag},
	    {0, "pseq", "ProfileSequenceDesc", "icSigProfileSequenceDescTag", icc_tag},
	    {0, "psd0", "Ps2CRD0", "icSigPs2CRD0Tag", icc_tag},
	    {0, "psd1", "Ps2CRD1", "icSigPs2CRD1Tag", icc_tag},
	    {0, "psd2", "Ps2CRD2", "icSigPs2CRD2Tag", icc_tag},
	    {0, "psd3", "Ps2CRD3", "icSigPs2CRD3Tag", icc_tag},
	    {0, "ps2s", "Ps2CSA", "icSigPs2CSATag", icc_tag},
	    {0, "ps2i", "Ps2RenderingIntent", "icSigPs2RenderingIntentTag", icc_tag},
	    {0, "rXYZ", "RedColorant", "icSigRedColorantTag", icc_tag},
	    {0, "rTRC", "RedTRC", "icSigRedTRCTag", icc_tag},
	    {0, "scrd", "ScreeningDesc", "icSigScreeningDescTag", icc_tag},
	    {0, "scrn", "Screening", "icSigScreeningTag", icc_tag},
	    {0, "tech", "Technology", "icSigTechnologyTag", icc_tag},
	    {0, "bfd ", "UcrBg", "icSigUcrBgTag", icc_tag},
	    {0, "vued", "ViewingCondDesc", "icSigViewingCondDescTag", icc_tag},
	    {0, "view", "ViewingConditions", "icSigViewingConditionsTag", icc_tag},
		{0, NULL, NULL, NULL, NULL}
	};

	long size, offset, count, tagsize;
	uint32_t hi, lo;
	const char *indent = tabs(level);
	char sig[4];
	off_t iccpos, pos;

	if(!xml)
		return;

	iccpos = (off_t)ftello(f);
	size = get4B(f);
	fprintf(xml, "%s<cmmId>%s</cmmId>\n", indent, getkey(f));
	fprintf(xml, "%s<version>%08x</version>\n", indent, get4B(f));
	fprintf(xml, "%s<deviceClass>\n", indent);
	findbykey(f, level+1, classdict, getkey(f), 1, 1);
	fprintf(xml, "%s</deviceClass>\n", indent);
	fprintf(xml, "%s<colorSpace>\n", indent);
	findbykey(f, level+1, spacedict, getkey(f), 1, 1);
	fprintf(xml, "%s</colorSpace>\n", indent);
	fprintf(xml, "%s<pcs>\n", indent);
	findbykey(f, level+1, spacedict, getkey(f), 1, 1);
	fprintf(xml, "%s</pcs>\n", indent);
	fprintf(xml, "%s<date>", indent);
	icc_datetime(f, level, 0, parent);
	fputs("</date>\n", xml);
	fprintf(xml, "%s<magic>%s</magic>\n", indent, getkey(f));
	fprintf(xml, "%s<platform>\n", indent);
	findbykey(f, level+1, platdict, getkey(f), 1, 1);
	fprintf(xml, "%s</platform>\n", indent);
	fprintf(xml, "%s<flags>%08x</flags>\n", indent, get4B(f));
	fprintf(xml, "%s<manufacturer>%s</manufacturer>\n", indent, getkey(f));
	fprintf(xml, "%s<model>%s</model>\n", indent, getkey(f));
	hi = get4B(f);
	lo = get4B(f);
	fprintf(xml, "%s<attributes>%08x%08x</attributes>\n", indent, hi, lo);
	fprintf(xml, "%s<renderingIntent>%08x</renderingIntent>\n", indent, get4B(f));
	fprintf(xml, "%s<illuminant>", indent);
	icc_xyz(f, level, 12, parent);
	fputs("</illuminant>\n", xml);
	fprintf(xml, "%s<creator>%s</creator>\n", indent, getkey(f));
	fseeko(f, 44, SEEK_CUR); // skip reserved bytes

	count = get4B(f);
	while(count--){
		fread(sig, 1, 4, f);
		offset = get4B(f);
		tagsize = get4B(f);
		pos = (off_t)ftello(f);
		fseeko(f, iccpos + offset, SEEK_SET);
		findbykey(f, level, tagdict, sig, tagsize, 1);
		fseeko(f, pos, SEEK_SET);
	}
}
