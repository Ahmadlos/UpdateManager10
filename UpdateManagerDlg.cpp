
#include "stdafx.h"
#include <afxwin.h>
#include <afxcmn.h>
#include <Shlobj.h>
#include "UpdateManager.h"
#include "UpdateManagerDlg.h"
#include ".\updatemanagerdlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


//------------------------------------------------------------------------------------------------------------------------
// CAboutDlg dialog used for App About
//------------------------------------------------------------------------------------------------------------------------
class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()


//------------------------------------------------------------------------------------------------------------------------
// CUpdateManagerDlg 
//------------------------------------------------------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CUpdateManagerDlg, CDialog)
	ON_WM_SYSCOMMAND()
	//}}AFX_MSG_MAP
	ON_WM_DROPFILES()
	ON_BN_CLICKED(IDC_BUTTON_ENCODE, OnBnClickedButtonEncode)
	ON_BN_CLICKED(IDC_BUTTON_DECODE, OnBnClickedButtonDecode)
	ON_BN_CLICKED(IDC_BUTTON_CLEAR, OnBnClickedButtonClear)
	ON_BN_CLICKED(IDC_VERSION_UP, OnBnClickedVersionUp)
	ON_BN_CLICKED(IDC_VERSION_DOWN, OnBnClickedVersionDown)
	ON_BN_CLICKED(IDC_CANCEL, OnBnClickedCancel)
	ON_EN_UPDATE(IDC_APP_NAME, OnEnUpdateAppName)
	ON_EN_UPDATE(IDC_VERSION, OnEnUpdateVersion)
	ON_BN_CLICKED(IDC_BUTTON_SELECT_PATCH_SOURCE, &CUpdateManagerDlg::OnBnClickedButtonSelectPatchSource)
END_MESSAGE_MAP()



//------------------------------------------------------------------------------------------------------------------------
// 생성자
//------------------------------------------------------------------------------------------------------------------------
CUpdateManagerDlg::CUpdateManagerDlg(CWnd* pParent /*=NULL*/)
:	CDialog( IDD_UPDATEMANAGER_DIALOG, pParent )
,	m_bUpdateAppInfo( false )
,	m_hIcon( NULL )
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}


//------------------------------------------------------------------------------------------------------------------------
// DDX
//------------------------------------------------------------------------------------------------------------------------
void CUpdateManagerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


//------------------------------------------------------------------------------------------------------------------------
// 다이럴로그 초기화
//------------------------------------------------------------------------------------------------------------------------
BOOL CUpdateManagerDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// 버튼 초기화
	OnBnClickedButtonClear();

	struct myLogHandler : XUpdateManager::LogHandler
	{
		virtual void SetCancelButton( bool bEnable )
		{
			static_cast< CButton* >( pDlg->GetDlgItem( IDC_CANCEL ) )->EnableWindow( bEnable ? 1 : 0 );
		}

		virtual void SetStatusMessage( const char *szLog )
		{
			pDlg->SetDlgItemText( IDC_STATUS, szLog );
		}

		virtual void AddLog( const char *szLog )
		{
			CRichEditCtrl *pLog = static_cast< CRichEditCtrl* >( pDlg->GetDlgItem( IDC_LOG ) );
			if( pLog ) 
			{
				pLog->SetSel( pLog->GetTextLength(), pLog->GetTextLength() );
				pLog->ReplaceSel( szLog );
				int nScrollCount = pLog->GetLineCount() - 20 - pLog->GetFirstVisibleLine();
				if( nScrollCount > 0 ) pLog->LineScroll( nScrollCount );
			}
		}

		CUpdateManagerDlg *pDlg;
	};
	static myLogHandler _handler;
	_handler.pDlg = this;
	_handler.SetCancelButton( false );
	XUpdateManager::GetInstance().SetLogHandler( &_handler );

	// 기본정보 초기화
	SetDlgItemText( IDC_APP_NAME, XUpdateManager::GetInstance().GetAppName().c_str() );
	SetDlgItemInt( IDC_VERSION, XUpdateManager::GetInstance().GetVersion() );

	m_bUpdateAppInfo = true;

	//	초기화 할때 Encode 클릭한 상태로 시작
	CUpdateManagerDlg::OnBnClickedButtonEncode();
	
	return TRUE;  
}


//------------------------------------------------------------------------------------------------------------------------
// 시스템 명령
//------------------------------------------------------------------------------------------------------------------------
void CUpdateManagerDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}


//------------------------------------------------------------------------------------------------------------------------
// 그리기
//------------------------------------------------------------------------------------------------------------------------
void CUpdateManagerDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CUpdateManagerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


//------------------------------------------------------------------------------------------------------------------------
// 패치 준비 ( 다이얼로그에서 설정 된 값에 따라서 옵션을 설정한다. )
//------------------------------------------------------------------------------------------------------------------------
void CUpdateManagerDlg::PrepareForMakePatch()
{
	int nCompress	= XUpdateManager::NONE;
	int nNameEncode	= XUpdateManager::NONE;

	if( static_cast< CButton* >( GetDlgItem( IDC_COMP_FREEZE ) )->GetCheck() == BST_CHECKED )
		nCompress = XUpdateManager::FREEZE;
	else if( static_cast< CButton* >( GetDlgItem( IDC_COMP_MELT ) )->GetCheck() == BST_CHECKED )
		nCompress = XUpdateManager::MELT;
	else
		nCompress = XUpdateManager::NONE;

	if( static_cast< CButton* >( GetDlgItem( IDC_NAME_ENCODE ) )->GetCheck() == BST_CHECKED )
		nNameEncode = XUpdateManager::ENCODE;
	else if( static_cast< CButton* >( GetDlgItem( IDC_NAME_DECODE ) )->GetCheck() == BST_CHECKED )
		nNameEncode = XUpdateManager::DECODE;
	else
		nNameEncode = XUpdateManager::NONE;

	CString strAppName( "" );
	GetDlgItemText( IDC_APP_NAME, strAppName );

	XUpdateManager::GetInstance().SetOption(
							nCompress,
							nNameEncode, 
							static_cast< CButton* >( GetDlgItem( IDC_BUILD_INDEX ) )->GetCheck() == BST_CHECKED,
							static_cast< CButton* >( GetDlgItem( IDC_BUILD_DIRECTORY ) )->GetCheck() == BST_CHECKED,
							static_cast< CButton* >( GetDlgItem( IDC_XOR ) )->GetCheck() == BST_CHECKED,
							strAppName,
							GetDlgItemInt( IDC_VERSION )
						 );
}


//------------------------------------------------------------------------------------------------------------------------
// 드롭 이벤트 처리
//------------------------------------------------------------------------------------------------------------------------
void CUpdateManagerDlg::OnDropFiles(HDROP hDropInfo)
{
	PrepareForMakePatch();
						   
	XUpdateManager::GetInstance().onDrop( &hDropInfo );

	CDialog::OnDropFiles(hDropInfo);
}


//------------------------------------------------------------------------------------------------------------------------
// Encode 버튼 클릭
//------------------------------------------------------------------------------------------------------------------------
void CUpdateManagerDlg::OnBnClickedButtonEncode()
{
	static_cast< CButton* >( GetDlgItem( IDC_COMP_NONE ) )->SetCheck( BST_UNCHECKED );	
	static_cast< CButton* >( GetDlgItem( IDC_COMP_FREEZE ) )->SetCheck( BST_CHECKED );
	static_cast< CButton* >( GetDlgItem( IDC_COMP_MELT ) )->SetCheck( BST_UNCHECKED );

	static_cast< CButton* >( GetDlgItem( IDC_NAME_NONE ) )->SetCheck( BST_UNCHECKED );	
	static_cast< CButton* >( GetDlgItem( IDC_NAME_ENCODE ) )->SetCheck( BST_CHECKED );
	static_cast< CButton* >( GetDlgItem( IDC_NAME_DECODE ) )->SetCheck( BST_UNCHECKED );

	static_cast< CButton* >( GetDlgItem( IDC_XOR ) )->SetCheck( BST_CHECKED );	
	static_cast< CButton* >( GetDlgItem( IDC_BUILD_INDEX ) )->SetCheck( BST_CHECKED );	
	static_cast< CButton* >( GetDlgItem( IDC_BUILD_DIRECTORY ) )->SetCheck( BST_CHECKED );		
}


//------------------------------------------------------------------------------------------------------------------------
// Decode 버튼 클릭
//------------------------------------------------------------------------------------------------------------------------
void CUpdateManagerDlg::OnBnClickedButtonDecode()
{
	static_cast< CButton* >( GetDlgItem( IDC_COMP_NONE ) )->SetCheck( BST_UNCHECKED );	
	static_cast< CButton* >( GetDlgItem( IDC_COMP_FREEZE ) )->SetCheck( BST_UNCHECKED );
	static_cast< CButton* >( GetDlgItem( IDC_COMP_MELT ) )->SetCheck( BST_CHECKED );

	static_cast< CButton* >( GetDlgItem( IDC_NAME_NONE ) )->SetCheck( BST_UNCHECKED );	
	static_cast< CButton* >( GetDlgItem( IDC_NAME_ENCODE ) )->SetCheck( BST_UNCHECKED );
	static_cast< CButton* >( GetDlgItem( IDC_NAME_DECODE ) )->SetCheck( BST_CHECKED );

	static_cast< CButton* >( GetDlgItem( IDC_XOR ) )->SetCheck( BST_CHECKED );	
	static_cast< CButton* >( GetDlgItem( IDC_BUILD_INDEX ) )->SetCheck( BST_UNCHECKED );	
	static_cast< CButton* >( GetDlgItem( IDC_BUILD_DIRECTORY ) )->SetCheck( BST_UNCHECKED );	
}


//------------------------------------------------------------------------------------------------------------------------
// Clear 버튼 클릭
//------------------------------------------------------------------------------------------------------------------------
void CUpdateManagerDlg::OnBnClickedButtonClear()
{	
	static_cast< CButton* >( GetDlgItem( IDC_COMP_NONE ) )->SetCheck( BST_CHECKED );
	static_cast< CButton* >( GetDlgItem( IDC_COMP_FREEZE ) )->SetCheck( BST_UNCHECKED );
	static_cast< CButton* >( GetDlgItem( IDC_COMP_MELT ) )->SetCheck( BST_UNCHECKED );

	static_cast< CButton* >( GetDlgItem( IDC_NAME_NONE ) )->SetCheck( BST_CHECKED );
	static_cast< CButton* >( GetDlgItem( IDC_NAME_ENCODE ) )->SetCheck( BST_UNCHECKED );
	static_cast< CButton* >( GetDlgItem( IDC_NAME_DECODE ) )->SetCheck( BST_UNCHECKED );
	
	static_cast< CButton* >( GetDlgItem( IDC_XOR ) )->SetCheck( BST_UNCHECKED );
	static_cast< CButton* >( GetDlgItem( IDC_BUILD_INDEX ) )->SetCheck( BST_UNCHECKED );	
	static_cast< CButton* >( GetDlgItem( IDC_BUILD_DIRECTORY ) )->SetCheck( BST_UNCHECKED );	
}


//------------------------------------------------------------------------------------------------------------------------
// Version Up 버튼 클릭
//------------------------------------------------------------------------------------------------------------------------
void CUpdateManagerDlg::OnBnClickedVersionUp()
{
	SetDlgItemInt( IDC_VERSION, GetDlgItemInt( IDC_VERSION ) + 1 );
}


//------------------------------------------------------------------------------------------------------------------------
// Version Down 버튼 클릭
//------------------------------------------------------------------------------------------------------------------------
void CUpdateManagerDlg::OnBnClickedVersionDown()
{
	SetDlgItemInt( IDC_VERSION, GetDlgItemInt( IDC_VERSION ) - 1 );
}


//------------------------------------------------------------------------------------------------------------------------
// Cancel 버튼 클릭
//------------------------------------------------------------------------------------------------------------------------
void CUpdateManagerDlg::OnBnClickedCancel()
{
	XUpdateManager::GetInstance().Cancel();
}


//------------------------------------------------------------------------------------------------------------------------
// App Name 갱신
//------------------------------------------------------------------------------------------------------------------------
void CUpdateManagerDlg::OnEnUpdateAppName()
{
	if( !m_bUpdateAppInfo ) return;

	CString strAppName;
	GetDlgItemText( IDC_APP_NAME, strAppName );

	XUpdateManager::GetInstance().SetAppName( strAppName );
	XUpdateManager::GetInstance().SaveUpdateInfo();
}


//------------------------------------------------------------------------------------------------------------------------
// Version 갱신
//------------------------------------------------------------------------------------------------------------------------
void CUpdateManagerDlg::OnEnUpdateVersion()
{
	if( !m_bUpdateAppInfo ) return;

	XUpdateManager::GetInstance().SetVersion( GetDlgItemInt( IDC_VERSION ) );
	XUpdateManager::GetInstance().SaveUpdateInfo();
}


//------------------------------------------------------------------------------------------------------------------------
// 패치 대상 선택 버튼 클릭
//------------------------------------------------------------------------------------------------------------------------
void CUpdateManagerDlg::OnBnClickedButtonSelectPatchSource()
{
	LPITEMIDLIST	pidlSelected( NULL );
	BROWSEINFOA		stBrowseInfo;
	char			szFolderPath[MAX_PATH] = { NULL, };
	char			szFolderDisplayName[MAX_PATH] = { NULL, };

	::ZeroMemory( &stBrowseInfo, sizeof( BROWSEINFO ) );
	
	stBrowseInfo.hwndOwner		= m_hWnd;
	stBrowseInfo.pszDisplayName = szFolderDisplayName;
	stBrowseInfo.lpszTitle		= _T( "Please Select Patch Resource Folder" );
	stBrowseInfo.ulFlags		= BIF_DONTGOBELOWDOMAIN | BIF_USENEWUI;
	
	pidlSelected = ::SHBrowseForFolder( &stBrowseInfo );

	if( pidlSelected != NULL )
		::SHGetPathFromIDList( pidlSelected, szFolderPath );
	else
		return;
	
	char szBuffer[512] = { NULL, };
	_sntprintf_s( szBuffer, _countof( szBuffer ), _TRUNCATE, "Do Work!! Are You Sure Execute? \n[PATH] : [ %s ]", szFolderPath );

	UINT nResult = AfxMessageBox( szBuffer, MB_YESNO | MB_ICONQUESTION );	
	
	if( nResult != IDYES )
		return;
	
	PrepareForMakePatch();

	XUpdateManager::GetInstance().onSelectPath( szFolderPath );
}