
// PsdParserDlg.cpp : ʵ���ļ�
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



// CPsdParserDlg �Ի���




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


// CPsdParserDlg ��Ϣ�������

BOOL CPsdParserDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// ���ô˶Ի����ͼ�ꡣ��Ӧ�ó��������ڲ��ǶԻ���ʱ����ܽ��Զ�
	//  ִ�д˲���
	SetIcon(m_hIcon, TRUE);			// ���ô�ͼ��
	SetIcon(m_hIcon, FALSE);		// ����Сͼ��

	// TODO: �ڴ���Ӷ���ĳ�ʼ������
	OpenConsole();

	m_info = (PSD_LAYERS_INFO *)calloc(1, sizeof(PSD_LAYERS_INFO));

	HDITEM hdItem;
	hdItem.cxy = 100;
	hdItem.mask = HDI_WIDTH;
	m_infoProperty.GetHeaderCtrl().SetItem(0, &hdItem);
	m_infoProperty.EnableHeaderCtrl(TRUE, _T("����"), _T("��ֵ"));

	m_pBaseProperty = new CMFCPropertyGridProperty(_T("��������"));
	m_pBaseProperty->AllowEdit(FALSE);
	m_pBaseProperty->AddSubItem(new CMFCPropertyGridProperty(_T("ͼ������"), _T("")));
	m_pBaseProperty->AddSubItem(new CMFCPropertyGridProperty(_T("ͨ������"), _T("")));
	m_pBaseProperty->AddSubItem(new CMFCPropertyGridProperty(_T("�߿��С"), _T("")));
	m_pBaseProperty->AddSubItem(new CMFCPropertyGridProperty(_T("ͼƬ��С"), _T("")));
	m_pBaseProperty->AddSubItem(new CMFCPropertyGridProperty(_T("��ʾ����"), _T("")));
	m_infoProperty.AddProperty(m_pBaseProperty);

	m_pAdditionalProperty = new CMFCPropertyGridProperty(_T("���Ӳ���"));
	m_pAdditionalProperty->AddSubItem(new CMFCPropertyGridProperty(_T("��������"), _T("")));
	m_pAdditionalProperty->AddSubItem(new CMFCPropertyGridProperty(_T("�ο�����"), _T("")));
	m_pAdditionalProperty->AddSubItem(new CMFCPropertyGridProperty(_T("��ת�Ƕ�"), _T("")));
	m_infoProperty.AddProperty(m_pAdditionalProperty);

	m_infoProperty.ExpandAll();


// 	TCHAR *lk = L"E:\images\ex\多层遮罩.psd";
// 	const char *k = "E:\images\ex\多层遮罩.psd";
// 	char buf[MAX_PATH] = {0};
// 	WideCharToMultiByte(CP_UTF8,0,lk,-1,buf,MAX_PATH,0,0);
// 
// 	USES_CONVERSION;
// 	char *m = W2A(lk);

	return TRUE;  // ���ǽ��������õ��ؼ������򷵻� TRUE
}

// �����Ի��������С����ť������Ҫ����Ĵ���
//  �����Ƹ�ͼ�ꡣ����ʹ���ĵ�/��ͼģ�͵� MFC Ӧ�ó���
//  �⽫�ɿ���Զ���ɡ�

void CPsdParserDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // ���ڻ��Ƶ��豸������

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// ʹͼ���ڹ����������о���
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// ����ͼ��
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//���û��϶���С������ʱϵͳ���ô˺���ȡ�ù��
//��ʾ��
HCURSOR CPsdParserDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CPsdParserDlg::OnBnClickedButtonBrowse()
{
	// TODO: �ڴ���ӿؼ�֪ͨ����������
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
	// TODO: �ڴ���ӿؼ�֪ͨ����������
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
	// TODO: �ڴ���ӿؼ�֪ͨ����������
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

