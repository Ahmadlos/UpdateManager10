
#include "stdafx.h"
#include "UpdateManagerCommandLine.h"
#include <ShlObj.h>
#include <ShellAPI.h>
#include <algorithm>
#include <Shlwapi.h>
#include <assert.h>
#include <toolkit/safe_function.h>

#define MAKEINT64(a, b)		((__int64)(((DWORD)((DWORD_PTR)(a) & 0xffffffff)) | ((__int64)((DWORD)((DWORD_PTR)(b) & 0xffffffff))) << 32))

CUpdateManagerCommandLine::CUpdateManagerCommandLine()
: m_codingType( _T( "" ) ), m_appName( _T( "" ) ), m_versionFilePath( _T( "" ) ), m_patchFilePath( _T( "" ) ), m_version( 0 )
{
}

CUpdateManagerCommandLine::~CUpdateManagerCommandLine()
{
}

void	CUpdateManagerCommandLine::initLogHandler()
{
	static XUpdateManager::LogHandler logHandler;
	XUpdateManager::GetInstance().SetLogHandler( &logHandler );
}

void	CUpdateManagerCommandLine::ParseParam( const TCHAR* pszParam, BOOL bFlag, BOOL bLast )
{
	std::string command( pszParam );
	std::transform( command.begin(), command.end(), command.begin(), toupper );

	parseCommandLine( command );

	if( TRUE == bLast && checkCommandLineInfo() )
	{
		if( !m_version )
			increasePatchVersion();
		execute();
	}
}

void	CUpdateManagerCommandLine::parseCommandLine( const std::string & command )
{
	if( command.find( _T( "TYPE" ) ) < command.size() )
	{
		m_codingType = getValue( command );
	}
	else if( command.find( _T( "APP_NAME" ) ) < command.size() )
	{
		m_appName = getValue( command );
	}
	else if( command.find( _T( "VERSION_PATH" ) ) < command.size() )
	{
		m_versionFilePath = getValue( command );
	}
	else if( command.find( _T( "PATCH_PATH" ) ) < command.size() )
	{
		m_patchFilePath = getValue( command );
	}
	else if( command.find( _T( "PATCH_VERSION" ) ) < command.size() )
	{
		m_version = _ttoi( getValue( command ).c_str() );
	}
	else
	{
		assert( 0 );
	}
}

std::string	CUpdateManagerCommandLine::getValue( const std::string & command ) const
{
	const size_t nth = command.find( _T( ":" ) ) + 1;
	if( nth >= 0 && nth < command.size() )
	{
		return std::string( command.begin() + nth, command.end() );
	}
	else
	{
		assert( 0 );
		return std::string( _T( "" ) );
	}
}

void	CUpdateManagerCommandLine::increasePatchVersion()
{
	m_version = getLastestPatchNo() + 1;
}

int	CUpdateManagerCommandLine::getLastestPatchNo() const
{
	if( m_appName.empty() )
	{
		assert( 0 && "appName Missed!");
		return -1;
	}

	std::string strName = m_appName + "*.tpf";
	
	TCHAR fullPath[ MAX_PATH ] = { 0, };
	::PathCombine( fullPath, m_versionFilePath.c_str(), _T( strName.c_str() ) );

	WIN32_FIND_DATA findData;

	HANDLE handle = ::FindFirstFile( fullPath, &findData );
	if( INVALID_HANDLE_VALUE == handle )
	{
		// 아무 TPF 파일도 못찾으면 1번부터 시작
		return 1;
	}

	__int64 lastestFileTime = 0;
	std::string lastestFileName( _T( "" ) );

	do 
	{
		if( !( findData.dwFileAttributes & ( FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_DIRECTORY ) ) )
		{
			const __int64 fileTime		= MAKEINT64( findData.ftCreationTime.dwLowDateTime, findData.ftCreationTime.dwHighDateTime );
			if( fileTime > lastestFileTime )
			{
				lastestFileTime = fileTime;
				lastestFileName = findData.cFileName;
			}
		}

	} while ( ::FindNextFile( handle, &findData ) );

	::FindClose( handle );

	const size_t nthBegin	= lastestFileName.find( _T( "(" ) ) + 1;
	const size_t nthEnd		= lastestFileName.find( _T( ")" ) );

	if( lastestFileName.size() > nthBegin && lastestFileName.size() > nthEnd )
	{
		return _ttoi( std::string( lastestFileName.begin() + nthBegin, lastestFileName.begin() + nthEnd ).c_str() );
	}

	// 아무 TPF 파일도 못찾으면 1번부터 시작 (여기 올일 없긴한데..)
	return 1;
}

bool	CUpdateManagerCommandLine::checkCommandLineInfo() const
{
	return ( !m_codingType.empty() && !m_appName.empty() && (!m_versionFilePath.empty() || m_version)  && !m_patchFilePath.empty() && !isEmptyDirectory( m_patchFilePath ) );
}

bool	CUpdateManagerCommandLine::isEmptyDirectory( const std::string & path ) const
{
	TCHAR fullPath[ MAX_PATH ] = { 0, };
	::PathCombine( fullPath, path.c_str(), _T( "*" ) );

	WIN32_FIND_DATA findData;

	HANDLE handle = ::FindFirstFile( fullPath, &findData );
	if( INVALID_HANDLE_VALUE == handle )
	{
		assert( 0 );
		return true;
	}

	do 
	{
		if( !( findData.dwFileAttributes & ( FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_DIRECTORY ) ) )
		{
			::FindClose( handle );
			return false;
		}

	} while ( ::FindNextFile( handle, &findData ) );

	::FindClose( handle );

	return true;
}

bool	CUpdateManagerCommandLine::setUpdateManagerOption()
{
	if( _T( "ENCODE" ) == m_codingType )
	{
		XUpdateManager::GetInstance().SetOption( XUpdateManager::FREEZE, XUpdateManager::ENCODE, true, true, true, m_appName.c_str(), m_version );
		return true;
	}
	else if( _T( "DECODE" ) == m_codingType )
	{
		XUpdateManager::GetInstance().SetOption( XUpdateManager::MELT, XUpdateManager::DECODE, false, false, true, m_appName.c_str(), m_version );
		return true;
	}
	else
	{
		assert( 0 );
		return false;
	}
}

void	CUpdateManagerCommandLine::execute()
{
	if( !setUpdateManagerOption() )
	{
		return;
	}

	initLogHandler();

	SIZE_T alloc_size = sizeof( DROPFILES ) + m_patchFilePath.size() + 2;
	HGLOBAL hGlobal = ::GlobalAlloc( GHND, alloc_size );
	if( hGlobal )
	{
		DROPFILES* dropFiles = static_cast<DROPFILES*>( ::GlobalLock( hGlobal ) );
		dropFiles->pFiles = sizeof( DROPFILES );
		s_strcpy( reinterpret_cast<char*>( dropFiles + 1 ), alloc_size - sizeof( DROPFILES ), m_patchFilePath.c_str() );
		::GlobalUnlock( hGlobal );
		XUpdateManager::GetInstance().onDrop( reinterpret_cast<HDROP*>( hGlobal ) );
		::GlobalFree( hGlobal );
	}

	::WaitForSingleObject( XUpdateManager::GetInstance().m_hCommandLineExecution, INFINITE );

	::PostQuitMessage( 0 );
}
