
// PsdParserDlg.h : 头文件
//

#pragma once
#include "afxcmn.h"
#include "afxwin.h"
#include "afxpropertygridctrl.h"
#include "psdparser.h"

// CPsdParserDlg 对话框
class CPsdParserDlg : public CDialogEx
{
// 构造
public:
	CPsdParserDlg(CWnd* pParent = NULL);	// 标准构造函数
	~CPsdParserDlg();

// 对话框数据
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
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedButtonBrowse();
	afx_msg void OnLbnSelchangeListLayers();
	afx_msg void OnBnClickedButtonSaveas();
};
