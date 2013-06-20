
// PsdParserDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "mfctest.h"
#include "PsdParserDlg.h"
#include "afxdialogex.h"
#include "psdparser.h"
#include <math.h>
#include "zip.h"
#include "unzip.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#include "vld.h"

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
#else
#define OpenConsole()
#define CloseConsole()
#endif



// CPsdParserDlg 对话框




CPsdParserDlg::CPsdParserDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CPsdParserDlg::IDD, pParent)
	, m_strPsdFile(_T(""))
	, m_pBaseProperty(NULL)
	, m_pAdditionalProperty(NULL)
	, m_info(NULL)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

CPsdParserDlg::~CPsdParserDlg()
{
	//delete m_pBaseProperty;
	//delete m_pAdditionalProperty;

	psd_release(m_info);
	CloseConsole();
}

void CPsdParserDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_PSDFILE, m_strPsdFile);
	DDX_Control(pDX, IDC_LIST_LAYERS, m_layersListBox);
	DDX_Control(pDX, IDC_MFCPROPERTYGRID_PSDINFO, m_infoProperty);
}

BEGIN_MESSAGE_MAP(CPsdParserDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_BROWSE, &CPsdParserDlg::OnBnClickedButtonBrowse)
	ON_LBN_SELCHANGE(IDC_LIST_LAYERS, &CPsdParserDlg::OnLbnSelchangeListLayers)
	ON_BN_CLICKED(IDC_BUTTON_SAVEAS, &CPsdParserDlg::OnBnClickedButtonSaveas)
END_MESSAGE_MAP()


void addlist(const char *files[], int count)
{
	for (int i = 0; i<count;i++)
	{
		const char *path = files[i];
		const char *name = strrchr(path, '\\');
		TRACE(_T("%s: %s\n"), path, name[i]);
	}
}


// CPsdParserDlg 消息处理程序

BOOL CPsdParserDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 设置此对话框的图标。当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码
	OpenConsole();

	m_info = (PSD_LAYERS_INFO *)calloc(1, sizeof(PSD_LAYERS_INFO));

	HDITEM hdItem;
	hdItem.cxy = 100;
	hdItem.mask = HDI_WIDTH;
	m_infoProperty.GetHeaderCtrl().SetItem(0, &hdItem);
	m_infoProperty.EnableHeaderCtrl(TRUE, _T("属性"), _T("数值"));

	m_pBaseProperty = new CMFCPropertyGridProperty(_T("基本参数"));
	m_pBaseProperty->AllowEdit(FALSE);
	m_pBaseProperty->AddSubItem(new CMFCPropertyGridProperty(_T("图层类型"), _T("")));
	m_pBaseProperty->AddSubItem(new CMFCPropertyGridProperty(_T("通道数量"), _T("")));
	m_pBaseProperty->AddSubItem(new CMFCPropertyGridProperty(_T("边框大小"), _T("")));
	m_pBaseProperty->AddSubItem(new CMFCPropertyGridProperty(_T("图片大小"), _T("")));
	m_pBaseProperty->AddSubItem(new CMFCPropertyGridProperty(_T("显示区域"), _T("")));
	m_infoProperty.AddProperty(m_pBaseProperty);

	m_pAdditionalProperty = new CMFCPropertyGridProperty(_T("附加参数"));
	m_pAdditionalProperty->AddSubItem(new CMFCPropertyGridProperty(_T("中心坐标"), _T("")));
	m_pAdditionalProperty->AddSubItem(new CMFCPropertyGridProperty(_T("参考坐标"), _T("")));
	m_pAdditionalProperty->AddSubItem(new CMFCPropertyGridProperty(_T("旋转角度"), _T("")));
	m_infoProperty.AddProperty(m_pAdditionalProperty);

	m_infoProperty.ExpandAll();


// 	TCHAR *lk = L"E:\images\ex\澶氬眰閬僵.psd";
// 	const char *k = "E:\images\ex\澶氬眰閬僵.psd";
// 	char buf[MAX_PATH] = {0};
// 	WideCharToMultiByte(CP_UTF8,0,lk,-1,buf,MAX_PATH,0,0);
// 
// 	USES_CONVERSION;
// 	char *m = W2A(lk);

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CPsdParserDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CPsdParserDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CPsdParserDlg::OnBnClickedButtonBrowse()
{
	// TODO: 在此添加控件通知处理程序代码
	CFileDialog dlg(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_ENABLESIZING, L"PSD Files (*.psd)|*.psd|All Files (*.*)|*.*||", this);
	
	if (IDOK == dlg.DoModal())
	{
		m_strPsdFile = dlg.GetPathName();
		UpdateData(FALSE);

		m_layersListBox.ResetContent();

		USES_CONVERSION;
		strcpy(m_info->file, W2A(m_strPsdFile));
		psd_parse(m_info);

		CoInitialize(NULL);

 		CString strItem;
		for (int i = 0; i < m_info->count; i++)
		{
			if (!strlen(m_info->layers[i].name))
			{
				continue;
			}

			GUID guid;

			memset(&guid, 0, sizeof(guid));
			if (S_OK == ::CoCreateGuid(&guid))
			{
				_snprintf(m_info->layers[i].lid, MAX_UUID_LEN,
					"%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X" ,
					guid.Data1,
					guid.Data2,
					guid.Data3,
					guid.Data4[0], guid.Data4[1],
					guid.Data4[2], guid.Data4[3],
					guid.Data4[4], guid.Data4[5],
					guid.Data4[6], guid.Data4[7]);
			}

			strItem.Format(_T("Layer %d - %s(%d)"), i, A2W(m_info->layers[i].name), m_info->layers[i].type);
			m_layersListBox.AddString(strItem);
			//TRACE(_T("%s: center point(x = %lf, y = %lf)\n"), strItem, m_info->layers[i].center.x, m_info->layers[i].center.y);
		}

		CoUninitialize();

		SetLayerInfo(0);
	}
}

void CPsdParserDlg::OnBnClickedButtonSaveas()
{
	// TODO: 在此添加控件通知处理程序代码
	psd_to_png(m_info);

	USES_CONVERSION;
	TRACE(_T("count = %d\n"), m_info->count);

	CString strItem;
	for (int i = 0; i < m_info->count; i++)
	{
		strItem.Format(_T("Layer %d - %s(%d)"), i, A2W(m_info->layers[i].name), m_info->layers[i].type);
		TRACE(_T("%s: lid = %s\n"), strItem, A2W(m_info->layers[i].lid));
	}
}

void CPsdParserDlg::OnLbnSelchangeListLayers()
{
	// TODO: 在此添加控件通知处理程序代码
	SetLayerInfo(m_layersListBox.GetCurSel());
}

void CPsdParserDlg::SetLayerInfo(int id)
{
	CString type, channels, size, rect, center, offset, angle;

	if (m_info->count <= id)
	{
		return;
	}

	m_layersListBox.SetSel(id);

	type.Format(_T("%d"), m_info->layers[id].type);
	m_pBaseProperty->GetSubItem(0)->SetValue(type);

	channels.Format(_T("%d"), m_info->layers[id].channels);
	m_pBaseProperty->GetSubItem(1)->SetValue(channels);

	size.Format(_T("%u x %u"), m_info->layers[id].bound.width, m_info->layers[id].bound.height);
	m_pBaseProperty->GetSubItem(2)->SetValue(size);

	size.Format(_T("%u x %u"), m_info->layers[id].actual.width, m_info->layers[id].actual.height);
	m_pBaseProperty->GetSubItem(3)->SetValue(size);

	rect.Format(_T("%d, %d, %d, %d"), m_info->layers[id].top, m_info->layers[id].left, m_info->layers[id].bottom, m_info->layers[id].right);
	m_pBaseProperty->GetSubItem(4)->SetValue(rect);

	center.Format(_T("%lf, %lf"), m_info->layers[id].center.x, m_info->layers[id].center.y);
	m_pAdditionalProperty->GetSubItem(0)->SetValue(center);

	offset.Format(_T("%lf, %lf"), m_info->layers[id].ref.x, m_info->layers[id].ref.y);
	m_pAdditionalProperty->GetSubItem(1)->SetValue(offset);

	angle.Format(_T("%lf"), m_info->layers[id].angle);
	m_pAdditionalProperty->GetSubItem(2)->SetValue(angle);
}

