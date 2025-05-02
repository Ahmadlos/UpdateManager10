
#pragma once

#include <Mshtml.h>
#include "XUpdateManager.h"


// CUpdateManagerDlg dialog
class CUpdateManagerDlg : public CDialog
{

public:

							CUpdateManagerDlg(CWnd* pParent = NULL);

protected:

	virtual BOOL			OnInitDialog( void );
	virtual void			DoDataExchange(CDataExchange* pDX);

	HRESULT					OnButtonOK(IHTMLElement *pElement);
	HRESULT					OnButtonCancel(IHTMLElement *pElement);

	void					PrepareForMakePatch( void );

protected:

	bool					m_bUpdateAppInfo;
	HICON					m_hIcon;

protected:

	afx_msg void			OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void			OnPaint( void );
	afx_msg HCURSOR 		OnQueryDragIcon( void );
	DECLARE_MESSAGE_MAP()

public:

	afx_msg void 			OnBnClickedButtonSelectPatchSource( void );
	afx_msg void 			OnBnClickedButtonEncode( void );
	afx_msg void 			OnBnClickedButtonDecode( void );
	afx_msg void 			OnBnClickedButtonClear( void );
	afx_msg void 			OnBnClickedVersionUp( void );
	afx_msg void 			OnBnClickedVersionDown( void );
	afx_msg void 			OnBnClickedCancel( void );

	afx_msg void 			OnEnUpdateAppName( void );
	afx_msg void 			OnEnUpdateVersion( void );

	afx_msg void 			OnDropFiles( HDROP hDropInfo );
};
