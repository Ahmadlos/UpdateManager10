
#include "stdafx.h"
#include <afxwin.h>
#include "XUpdateManager.h"
#include <toolkit/XDirectoryScanner.h>
#include <compress/XZip.h>
#include <kfile/XOREn.h>
#include <toolkit/XStringUtil.h>
#include <kfile/KFileNameCipher.h>
#include <toolkit/XFileUtil.h>
#include <toolkit/safe_function.h>
#include <vector>
#include <process.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <io.h>

#include <patch/PatchFileHelper.h>

#include "PatchedInfo.h"

volatile bool	g_bCancelFlag;


struct myScanner : IDirectoryScanner::Scanner
{
	myScanner()
	{
		bPatchable = true;
	}

	virtual bool onDirectory( const char *szFullPath, const char *szDirectory, const char *szFileName )
	{
		if( !_stricmp( szDirectory, "." ) || !_stricmp( szDirectory, ".." ) ) return !g_bCancelFlag;
		bPatchable = false;
		return !g_bCancelFlag;
	}

	virtual bool onFile( const char *szFullPath, const char *szDirectory, const char *szFileName )
	{
		if( _stricmp( szFileName, "Patched.tpf" ) == 0 )
		{
			if( pLog ) pLog->SetStatusMessage( "Loading Patched.tpf file..." );

			if( Patched.LoadTPFFile( szFullPath ) == false )
			{
				Patched.Clear();
			}

			return !g_bCancelFlag;
		}

		vList.push_back( szFullPath );

		char buf[256];
		s_sprintf( buf, _countof( buf ), "Scanning Directory... (%d)", vList.size() );
		if( pLog ) pLog->SetStatusMessage( buf );

		return !g_bCancelFlag;
	}

	XUpdateManager::LogHandler*		pLog;
	bool							bPatchable;
	std::vector< std::string >		vList;
	PatchedInfo						Patched;
};


XUpdateManager&	XUpdateManager::GetInstance()
{
	static XUpdateManager update_manager;
	return update_manager;
}

XUpdateManager::XUpdateManager() : m_pLog( NULL )
{
	m_nCompress			= 0;
	m_nNameEncode		= 0;
	m_bBuildIndex		= false;
	m_bBuildDirectory	= false;
	m_bDoXOR			= false;
	m_strAppName		= "DEFAULT";
	m_nVersion			= 0;
	m_hCommandLineExecution	= (HANDLE)NULL;

	loadUpdateInfo();
}

XUpdateManager::~XUpdateManager()
{
	SaveUpdateInfo();
	CloseHandle();
}

void XUpdateManager::Cancel()
{
	g_bCancelFlag = true;
}

void XUpdateManager::CloseHandle()
{
	if( m_hCommandLineExecution )
	{
		::CloseHandle( m_hCommandLineExecution );
		m_hCommandLineExecution	= (HANDLE)NULL;
	}
}

template< typename T >
struct auto_ptr_array
{
	auto_ptr_array( T* p = NULL ) : m_p( p ) {}
	~auto_ptr_array() { delete [] m_p; }

	T* & operator=( T* p ) { return m_p = p; }

	T* & operator*() { return m_p; }

	T * m_p;
};

bool createFileFromBuffer( const char *szFileName, const void *pBuffer, size_t len )
{
	FILE *fp = NULL;
	fopen_s( &fp, szFileName, "w+b" );
	if( !fp ) return false;
	if( fwrite( pBuffer, 1, len, fp ) != len ) 
	{
		fclose( fp );
		DeleteFile( szFileName );
		return false;
	}
	fclose( fp );
	return true;
}

#define PROC_ERROR( a )		{ pLog->AddLog( a ); int nResult = ::AfxMessageBox( a, MB_ABORTRETRYIGNORE ); if( nResult == IDIGNORE ) return true; if( nResult == IDCANCEL ) return false; }

struct XOption
{ 
	int nCompress; int nNameEncode; bool bBuildIndex; bool bBuildDirectory; bool bDoXOR; std::string strAppName; int nVersion;
};

bool ProcessFile( const PatchedInfo& Patched, XUpdateManager::LogHandler *pLog, const XOption* pOpt, const char *szBaseDirectory, const char *szFullPath, std::string * pPatchInfo = NULL )
{
	std::string strSourceFileName;
	std::string strDecodedSourceFileName;
	std::string strTargetFileName;
	std::string strFileExtension;
	std::string strURLDirectory = "/";
	bool bDoXOR = pOpt->bDoXOR;
	bool		bNeedSubDirectory = true;
	int nCompress = pOpt->nCompress;

	auto_ptr_array< unsigned char >		bufSource;
	size_t								nSourceSize = 0;
	int									nSourceChecksum = 0;

	auto_ptr_array< unsigned char >		bufTarget;
	size_t								nTargetSize = 0;
	int									nTargetChecksum = 0;

	if( !XFileUtil::GetFileName( szFullPath, strSourceFileName ) ) return false;
	
	/*
	pLog->AddLog( "Processing : " );
	pLog->AddLog( szFullPath );
	pLog->AddLog( "\n" );
	*/

	strTargetFileName			= strSourceFileName;
	strDecodedSourceFileName	= strSourceFileName;

	std::string strErrorMsg( "[ERROR] FileName is English Only!! Ignore File ... : ");

	const char *p = strSourceFileName.c_str();
	while( *p )
	{
		if( *p < 0 ) 
		{
			strErrorMsg.append( strSourceFileName );
			strErrorMsg.append( "\n" );

			PROC_ERROR( strErrorMsg.c_str() );
			return true;
		}
		++p;
	}

	// { 이름 인코드/디코드
	if( pOpt->nNameEncode == XUpdateManager::ENCODE )
	{	
		char buf[256] = { 0, };
		s_strcpy( buf, _countof( buf ), strTargetFileName.c_str() );
		s_tolower( buf, _countof( buf ) );
		strTargetFileName = buf;
		KFileNameCipher::EncodeFileName( strTargetFileName );
	}
	else if( pOpt->nNameEncode == XUpdateManager::DECODE )
	{
		if( !KFileNameCipher::IsEncodedName( strTargetFileName ) )
		{
			return true;
		}

		KFileNameCipher::DecodeFileName( strTargetFileName );
		strDecodedSourceFileName = strTargetFileName;
	}
	// }
	
	if( !XFileUtil::GetFileExtension( strDecodedSourceFileName.c_str(), strFileExtension ) ) return false;

	// 확장자가 .exe .dll .v3d .flt .asi 파일인 경우 압축/XOR/이름인코딩을 하지 않는다.
	if( IsNativeFile( strDecodedSourceFileName.c_str() ) == true )
	{
		bNeedSubDirectory = false;
		bDoXOR = false;
		nCompress = XUpdateManager::NONE;
		if( pOpt->nNameEncode == XUpdateManager::ENCODE ) strTargetFileName = strSourceFileName;
	}

	if( bNeedSubDirectory ) 
	{
		strURLDirectory = "/";
		strURLDirectory += getDirectoryName( strTargetFileName );
		strURLDirectory += "/";
	}

	if( bDoXOR || nCompress != XUpdateManager::NONE || pOpt->bBuildDirectory || pOpt->bBuildIndex )
	{
		// 원본 얻기
		while( true )
		{
			nSourceSize = XFileUtil::GetFileSize( szFullPath );
			bufSource = new unsigned char[ nSourceSize ];
			_chmod( szFullPath, _S_IREAD | _S_IWRITE );
			if( XFileUtil::ReadFile( szFullPath, *bufSource, nSourceSize ) != nSourceSize )
			{
				std::string strError = std::string( "Can't read file : " ) + strSourceFileName.c_str() + "\n";

				PROC_ERROR( strError.c_str() );

				nSourceSize = 0;
				delete [] (*bufSource);

				continue;
			}

			break;
		}

		// 원본 체크섬 얻기
		nSourceChecksum = getChecksum( *bufSource, nSourceSize );

		// 압축해제
		if( nCompress == XUpdateManager::MELT )
		{
			nTargetSize = XZip::GetOriginalSize( *bufSource, nSourceSize );
			bufTarget = new unsigned char[ nTargetSize ];
			if( XZip::Uncompress( *bufSource, nSourceSize, *bufTarget, nTargetSize ) != true )
			{
				pLog->AddLog( "Can't melt file : " );
				pLog->AddLog( szFullPath );
				pLog->AddLog( "\n" );
				return false;
			}
		}

		// .raw 나 .mp3 가 아닐경우에만 XOR 을 한다.
		if( bDoXOR )
		{
			size_t size			= nSourceSize;
			unsigned char *p	= (*bufSource);
			if( nCompress == XUpdateManager::MELT ) 
			{
				p = (*bufTarget);
				size = nTargetSize;
			}
			for( size_t i = 0; i < size; ++i )
			{
				p[i] ^= XOREn::GetEncodeKeyChar( i );
			}
		}
	}

	// 압축이 안일어 났다면 일단 소스로부터 복사
	if( nCompress == XUpdateManager::NONE && nSourceSize )
	{
		bufTarget = new unsigned char[ nSourceSize ];
		memcpy( *bufTarget, *bufSource, nSourceSize );
		nTargetSize = nSourceSize;
	}

	// 압축
	if( nCompress == XUpdateManager::FREEZE )
	{
		nTargetSize = XZip::GetBufferSize( nSourceSize );
		bufTarget = new unsigned char[ nTargetSize ];
		nTargetSize = XZip::Compress( *bufSource, nSourceSize, *bufTarget, nTargetSize );
		if( nTargetSize == 0 )
		{
			pLog->AddLog( "Can't freeze file : " );
			pLog->AddLog( szFullPath );
			pLog->AddLog( "\n" );
			return false;
		}
	}

	if( nTargetSize )
	{
		// 타겟 체크섬 얻기
		nTargetChecksum = getChecksum( *bufTarget, nTargetSize );
	}
	
	// 디렉토리 arrange 일경우 
	if( pOpt->bBuildDirectory )
	{
		////////////////////////////////////////////////////////////////////////
		// 원본 파일을 Original 폴더 밑으로 옮긴다.

		// Original 폴더 생성
		std::string strOriginalDirectory	= szBaseDirectory;
		strOriginalDirectory				+= "Original\\";
		if( !XFileUtil::IsDirectory( strOriginalDirectory.c_str() ) )
		{
			if( !CreateDirectory( strOriginalDirectory.c_str(), NULL ) )
			{
				pLog->AddLog( "Can't create directory : " );
				pLog->AddLog( strOriginalDirectory.c_str() );
				pLog->AddLog( "\n" );
				return false;
			}
		}

		// 파일 이동
		std::string strTargetOriginalFile	= strOriginalDirectory + strSourceFileName;
		while( true )
		{
			if( !MoveFile( szFullPath, strTargetOriginalFile.c_str() ) )
			{
				char buf[1024];
				DWORD ret = FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, ::GetLastError(), 0, buf, sizeof(buf), NULL );

				std::string strError = std::string( "Can't move file : " ) + szFullPath + " to " + strTargetOriginalFile + "\n" + buf + "\n";

				PROC_ERROR( strError.c_str() );

				continue;
			}
			break;
		}


		////////////////////////////////////////////////////////////////////////
		// 타겟 파일을 Target 폴더 밑에 생성한다.
		if( Patched.IsPatchedFile( strTargetFileName, nSourceSize, nSourceChecksum, nTargetSize, nTargetChecksum ) == true )
		{
			return true;
		}

		// Target 폴더 생성
		std::string strTargetDirectory	= szBaseDirectory;
		strTargetDirectory				+= "Target\\";
		if( !XFileUtil::IsDirectory( strTargetDirectory.c_str() ) )
		{
			if( !CreateDirectory( strTargetDirectory.c_str(), NULL ) )
			{
				pLog->AddLog( "Can't create directory : " );
				pLog->AddLog( strTargetDirectory.c_str() );
				pLog->AddLog( "\n" );
				return false;
			}
		}

		// Target 폴더 및 숫자 디렉토리 생성
		if( bNeedSubDirectory )
		{
			strTargetDirectory += getDirectoryName( strTargetFileName );
			if( !XFileUtil::IsDirectory( strTargetDirectory.c_str() ) )
			{
				if( !CreateDirectory( strTargetDirectory.c_str(), NULL ) )
				{
					pLog->AddLog( "Can't create directory : " );
					pLog->AddLog( strTargetDirectory.c_str() );
					pLog->AddLog( "\n" );
					return false;
				}
			}
			strTargetDirectory += "\\";
		}

		// 타겟 파일 생성
		std::string strTargetFilePathName = strTargetDirectory + strTargetFileName;
		while( true )
		{
			if( !createFileFromBuffer( strTargetFilePathName.c_str(), *bufTarget, nTargetSize ) )
			{
				std::string strError = std::string( "Can't write file : " ) + strTargetFilePathName + "\n";

				PROC_ERROR( strError.c_str() );

				continue;
			}
			break;
		}
	}
	else
	{
		if( Patched.IsPatchedFile( strTargetFileName, nSourceSize, nSourceChecksum, nTargetSize, nTargetChecksum ) == true )
		{
			return true;
		}

		std::string strTargetFilePathName = szBaseDirectory + strTargetFileName;

		if( nTargetSize )
		{
			char tmpFileName[256];
			s_sprintf( tmpFileName, _countof( tmpFileName ), ".\\%d_%d_%d.tmp", ::GetProcessId( ::GetCurrentProcess() ), ::GetCurrentThreadId(), rand() );

			// 타겟 파일 생성
			while( true )
			{
				if( !createFileFromBuffer( tmpFileName, *bufTarget, nTargetSize ) )
				{
					std::string strError = std::string( "Can't write file : " ) + tmpFileName + "\n";
					PROC_ERROR( strError.c_str() );
					continue;
				}
				break;
			}

			// 원본 파일 삭제
			while( true )
			{
				if( !DeleteFile( szFullPath ) )
				{
					char buf[1024];
					DWORD ret = FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, ::GetLastError(), 0, buf, sizeof(buf), NULL );

					std::string strError = std::string( "Can't delete file : " ) + szFullPath + "\n" + buf + "\n";
					PROC_ERROR( strError.c_str() );
					continue;
				}
				break;
			}

			// 타겟 파일 이동
			while( true )
			{
				if( !MoveFile( tmpFileName, strTargetFilePathName.c_str() ) )
				{
					char buf[1024];
					DWORD ret = FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, ::GetLastError(), 0, buf, sizeof(buf), NULL );

					std::string strError = std::string( "Can't move file : " ) + tmpFileName + " to " + strTargetFilePathName + "\n" + buf + "\n";
					PROC_ERROR( strError.c_str() );
					continue;
				}
				break;
			}
			
		}
		else
		{
			while( true )
			{
				if( !MoveFile( szFullPath, strTargetFilePathName.c_str() ) )
				{
					char buf[1024];
					DWORD ret = FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, ::GetLastError(), 0, buf, sizeof(buf), NULL );

					std::string strError = std::string( "Can't move file : " ) + szFullPath + " to " + strTargetFilePathName + "\n" + buf + "\n";
					PROC_ERROR( strError.c_str() );
					continue;
				}
				break;
			}
		}
	}

	if( pPatchInfo )
	{
		char szPatchInfo[1024];
		s_sprintf( szPatchInfo, _countof( szPatchInfo ),
								":%s:%d:%s:%d:%08X:%d:%08X:%s::\n",
								pOpt->strAppName.c_str(),
								pOpt->nVersion,
								strTargetFileName.c_str(),
								nSourceSize,
								nSourceChecksum,
								nTargetSize,
								nTargetChecksum,
								strURLDirectory.c_str() );
		*pPatchInfo += szPatchInfo;
	}

	return true;	
}


bool XUpdateManager::onDirectory( struct XOption *pOpt, const char* szBaseDirectory, const char* szPathName, std::string& strPatchInfo )
{
	myScanner _myScanner;
	_myScanner.pLog = GetLogHandler();

	// 디렉토리 스캔
	GetLogHandler()->SetStatusMessage( "Scanning Directory..." );
	IDirectoryScanner::Instance().Scan( szPathName, &_myScanner );

	m_pLog->SetStatusMessage( "Processing directory..." );

	size_t cnt = 0;
	for( std::vector< std::string >::iterator it = _myScanner.vList.begin(); it != _myScanner.vList.end(); ++it )
	{
		if( g_bCancelFlag ) break;

		if( onFile( _myScanner.Patched, pOpt, szBaseDirectory, (*it).c_str(), strPatchInfo ) == false )
		{
			return false;
		}

		++cnt;

		char buf[256];
		s_sprintf( buf, _countof( buf ), "Processing files... (%d/%d) - %d%%", cnt, _myScanner.vList.size(), (int)((float)cnt/_myScanner.vList.size() * 100 ) );
		m_pLog->SetStatusMessage( buf );
	}

	return true;
}

bool XUpdateManager::onFile( const PatchedInfo& Patched, struct XOption *pOpt, const char* szBaseDirectory, const char* szFileName, std::string& strPatchInfo )
{
	if( ProcessFile( Patched, m_pLog, pOpt, szBaseDirectory, szFileName, &strPatchInfo ) == false )
	{
		if( m_pLog != NULL )
		{
			m_pLog->AddLog( "ERROR File: " );
			m_pLog->AddLog( szFileName );
			m_pLog->AddLog( "\n" );
			m_pLog->SetStatusMessage( "Error occured..." );
			m_pLog->SetCancelButton( false );
		}

		return false;
	}

	return true;
}

static std::string getIndexFileName( const char *szAppName, int nVersion )
{
	time_t tt = time( NULL );
	struct tm lt;
	localtime_s( &lt, &tt );
	char temp[1024];
	s_sprintf( temp, _countof( temp ), "%s (%d) %04d-%02d-%02d %02d-%02d-%02d.tpf",
		szAppName, nVersion,
		lt.tm_year+1900, lt.tm_mon+1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec );	

	return temp;
}


unsigned int __stdcall procPatchFunc( void* pArg )
{
	XUpdateManager* pDlg = reinterpret_cast< XUpdateManager* >( pArg );
	XUpdateManager::LogHandler *pLog = pDlg->GetLogHandler();

	std::string strBasePathName;

	XOption option;
	pDlg->GetOption( option.nCompress, option.nNameEncode, option.bBuildIndex, option.bBuildDirectory, option.bDoXOR, option.strAppName, option.nVersion );

	strBasePathName = pDlg->GetPatchPatchName();

	XFileUtil::NomalizeDirectoryName( strBasePathName );

	myScanner _myScanner;
	_myScanner.pLog = pLog;

	// 디렉토리 스캔
	pLog->SetStatusMessage( "Scanning Directory..." );
	IDirectoryScanner::Instance().Scan( strBasePathName.c_str(), &_myScanner );

	// 파일 처리
	std::string strPatchInfo;
	pLog->SetStatusMessage( "Processing files..." );
	size_t cnt = 0;
	for( std::vector< std::string >::iterator it = _myScanner.vList.begin(); it != _myScanner.vList.end(); ++it )
	{
		if( g_bCancelFlag ) break;

		if( !ProcessFile( _myScanner.Patched, pLog, &option, strBasePathName.c_str(), (*it).c_str(), &strPatchInfo ) ) 
		{
			pLog->AddLog( "ERROR : " );
			pLog->AddLog( (*it).c_str() );
			pLog->AddLog( "\n" );
			pLog->SetStatusMessage( "Error occured..." );
			pLog->SetCancelButton( false );
			return 0;
		}

		cnt++;

		char buf[256];
		s_sprintf( buf, _countof( buf ), "Processing files... (%d/%d) - %d%% )", cnt, _myScanner.vList.size(), (int)((float)cnt/_myScanner.vList.size() * 100 ) );
		pLog->SetStatusMessage( buf );
	}

	pDlg->MakeTPF( option, strPatchInfo );

	if( g_bCancelFlag ) pLog->SetStatusMessage( "Patch canceled." );
	else				pLog->SetStatusMessage( "Patch complete." );

	pLog->SetCancelButton( false );

	pLog->AddLog( "Patch complete.\n" );

	::SetEvent( pDlg->m_hCommandLineExecution );

	return 0;
}

void XUpdateManager::procPatch( const char *szPathName )
{
	m_strPatchPatchName = szPathName;
	m_hCommandLineExecution	= ::CreateEvent( NULL, FALSE, FALSE, NULL );
	_beginthreadex( NULL, 0, procPatchFunc, this, 0, NULL );	
}

void XUpdateManager::loadUpdateInfo()
{
	FILE *fp = NULL;
	fopen_s( &fp, "config.txt", "r" );
	if( fp )
	{
		char szAppName[512];
		fscanf_s( fp, "%s : %d", szAppName, _countof( szAppName ), &m_nVersion );
		fclose( fp );
		m_strAppName = szAppName;
	}
}

void XUpdateManager::SaveUpdateInfo()
{
	FILE *fp = NULL;
	fopen_s( &fp, "config.txt", "w+b" );
	if( fp )
	{
		fprintf( fp, "%s : %d", m_strAppName.c_str(), m_nVersion );
		fclose( fp );
	}
}

void XUpdateManager::onDrop( HDROP* pFile )
{
	g_bCancelFlag = false;
	m_pLog->SetCancelButton( true );

	char szFileName[256];
	unsigned int nFileCount = DragQueryFile( *pFile, 0xFFFFFFFF, szFileName, sizeof(szFileName) );

	if( nFileCount == 1 )
	{
		DragQueryFile( *pFile, 0, szFileName, sizeof(szFileName) );

		if( XFileUtil::IsDirectory( szFileName ) && m_bBuildIndex ) 
		{
			m_pLog->SetStatusMessage( "Starting patch..." );
			procPatch( szFileName );
			return;
		}
	}

	XOption option;
	GetOption( option.nCompress, option.nNameEncode, option.bBuildIndex, option.bBuildDirectory, option.bDoXOR, option.strAppName, option.nVersion );

	char szCurrentDir[MAX_PATH] = "";
	::GetCurrentDirectory( _countof( szCurrentDir ), szCurrentDir );
	std::string strBaseDir = szCurrentDir;
	if( strBaseDir.back() != '\\' )
	{
		strBaseDir += "\\";
	}

	std::string strPatchInfo;
	for( unsigned int i = 0; i < nFileCount; ++i )
	{
		if( g_bCancelFlag ) break;

		DragQueryFile( *pFile, i, szFileName, sizeof(szFileName) );

		if( XFileUtil::IsDirectory( szFileName ) )
		{
			if( onDirectory( &option, strBaseDir.c_str(), szFileName, strPatchInfo ) == false )
			{
				if( m_pLog != NULL )
				{
					m_pLog->AddLog( "ERROR Directory: " );
					m_pLog->AddLog( szFileName );
					m_pLog->AddLog( "\n" );
					m_pLog->SetStatusMessage( "Error occured..." );
					m_pLog->SetCancelButton( false );
				}

				return;
			}
		}
		else if( XFileUtil::IsFile( szFileName ) )
		{
			PatchedInfo Patched;
			if( onFile( Patched, &option, strBaseDir.c_str(), szFileName, strPatchInfo ) == false )
			{
				return;
			}
		}
	}

	MakeTPF( option, strPatchInfo );

	if( g_bCancelFlag ) m_pLog->SetStatusMessage( "Patch canceled." );
	else				m_pLog->SetStatusMessage( "Patch complete." );

	m_pLog->SetCancelButton( false );

	m_pLog->AddLog( "Patch complete.\n" );
}

bool	XUpdateManager::MakeTPF( const XOption& option, const std::string& strPatchInfo )
{
	if( option.bBuildIndex == false )
	{
		return true;
	}

	if( strPatchInfo.empty() == true )
	{
		m_pLog->SetStatusMessage( "Patch info empty." );
		return true;
	}

	m_pLog->SetStatusMessage( "Generating index..." );

	std::vector< std::string > vToken;
	XStringUtil::Split( option.strAppName.c_str(), vToken, ";", false  );

	std::vector< std::string >::iterator it;
	for( it = vToken.begin(); it != vToken.end(); ++it )
	{
		std::string strPatchInfoName = getIndexFileName( (*it).c_str(), option.nVersion );
		m_pLog->AddLog( "Generating patch info : " );
		m_pLog->AddLog( strPatchInfoName.c_str() );
		m_pLog->AddLog( "\n" );

		std::string strPatchInfoData;
		strPatchInfoData = strPatchInfo;
		XStringUtil::Replace( strPatchInfoData, option.strAppName.c_str(), (*it).c_str() );

		if( !createFileFromBuffer( strPatchInfoName.c_str(), strPatchInfoData.c_str(), strPatchInfoData.size() ) )
		{
			m_pLog->AddLog( "ERROR : Can't create patch info : " );
			m_pLog->AddLog( strPatchInfoName.c_str() );
			m_pLog->AddLog( "\n" );
			m_pLog->SetStatusMessage( "Error occured..." );
			m_pLog->SetCancelButton( false );
			return false;
		}
	}

	return true;
}

void XUpdateManager::onSelectPath( std::string szFolderFullPath )
{
	g_bCancelFlag = false;
	m_pLog->SetCancelButton( true );

	szFolderFullPath += "\\";

	XOption option;
	GetOption( option.nCompress, option.nNameEncode, option.bBuildIndex, option.bBuildDirectory, option.bDoXOR, option.strAppName, option.nVersion );

	m_pLog->SetStatusMessage( "Starting patch..." );
	procPatch( szFolderFullPath.c_str() );
}
