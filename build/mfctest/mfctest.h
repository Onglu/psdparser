
// mfctest.h : PROJECT_NAME Ӧ�ó������ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "resource.h"		// ������


// CPsdParserApp:
// �йش����ʵ�֣������ mfctest.cpp
//

class CPsdParserApp : public CWinApp
{
public:
	CPsdParserApp();

// ��д
public:
	virtual BOOL InitInstance();

// ʵ��

	DECLARE_MESSAGE_MAP()
};

extern CPsdParserApp theApp;