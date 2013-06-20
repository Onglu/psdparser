// console.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "psdparser.h"
#include <Windows.h>
#include <io.h>
#include <fcntl.h>

FILE* g_fp = NULL; 

void OpenConsole()  
{  
	int nRet= 0;  

	AllocConsole();
	SetConsoleTitle(L"Debug Output Console");

	nRet= _open_osfhandle((long)GetStdHandle(STD_OUTPUT_HANDLE), _O_TEXT);  
	g_fp = _fdopen(nRet, "w");  
	*stdout = *g_fp;  
	setvbuf(stdout, NULL, _IONBF, 0);  
}

void CloseConsole()
{
	if (g_fp)
	{
		fclose(g_fp);
		FreeConsole();
	}
}

int _tmain(int argc, _TCHAR* argv[])
{
	PSD_LAYERS_INFO *info;
	info = (PSD_LAYERS_INFO *)calloc(1, sizeof(PSD_LAYERS_INFO));

	int r = psd_parse("E:\\images\\templates\\1.psd", info, PARSE_MODE_READONLY);

	for (int i = 0; i < info->count; i++)
	{
		printf("Layer %d: name = %s, angle = %lf\n", i, info->layers[i].name, info->layers[i].angle);
	}

	psd_release(info);

	return 0;
}

