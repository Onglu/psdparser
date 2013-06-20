
// PsdParserDlg.h : ͷ�ļ�
//

#pragma once
#include "afxcmn.h"
#include "afxwin.h"
#include "afxpropertygridctrl.h"
#include "psdparser.h"

// CPsdParserDlg �Ի���
class CPsdParserDlg : public CDialogEx
{
// ����
public:
	CPsdParserDlg(CWnd* pParent = NULL);	// ��׼���캯��
	~CPsdParserDlg();

// �Ի�������
	enum { IDD = IDD_MFCTEST_DIALOG };

	PSD_LAYERS_INFO *m_info;

	CString m_strPsdFile;
	CListBox m_layersListBox;
	CMFCPropertyGridCtrl m_infoProperty;
	CMFCPropertyGridProperty *m_pBaseProperty, *m_pAdditionalProperty;

	//void CreateZipFromDir(const CString& dirName, const CString& zipFileName);

private:
	void SetLayerInfo(int id = 0);

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV ֧��


// ʵ��
protected:
	HICON m_hIcon;

	// ���ɵ���Ϣӳ�亯��
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedButtonBrowse();
	afx_msg void OnLbnSelchangeListLayers();
	afx_msg void OnBnClickedButtonSaveas();
};
