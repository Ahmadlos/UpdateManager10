
#include "stdafx.h"
#include "PatchedInfo.h"


PatchedInfo::PatchedInfo()
{
}

PatchedInfo::~PatchedInfo()
{
	Clear();
}

bool	PatchedInfo::LoadTPFFile( const char* szFileName )
{ 
	FILE *fp = NULL;
	if( fopen_s( &fp, szFileName, "r" ) != 0 )
	{
		return false;
	}

	if( !fp )
	{
		return false;
	}

	char buf[2048];
	while( !feof( fp ) ) 
	{
		memset( buf, 0, sizeof(buf) );
		fgets( buf, sizeof(buf), fp );

		if( buf[0] == '\r' ) 
			continue;
		if( buf[0] == '\n' ) 
			continue;
		if( buf[0] == '#' ) 
			continue;

		XStringUtil::TrimRight( buf );

		std::vector< std::string > vList;

		XStringUtil::Split( buf, vList, ":", false );

		// 파일 정보라면
		if( vList.size() >= 8 )
		{
			std::string & strApplicationName	= vList[0];
			std::string & strVersion			= vList[1];
			std::string & strFileName			= vList[2];
			std::string & strSizeBefore			= vList[3];
			std::string & strChecksumBefore		= vList[4];
			std::string & strSizeAfter			= vList[5];
			std::string & strChecksumAfter		= vList[6];
			std::string strDirectory			= vList.size() > 7 ? vList[7] : "";
			std::string strDescription			= vList.size() > 8 ? vList[8] : "";

			if( strFileName.empty() || strVersion.empty() || strSizeBefore.empty() || strChecksumBefore.empty() || strSizeAfter.empty() || strChecksumAfter.empty() || strApplicationName.empty() )
			{
				break;
			}

			if( strDirectory.empty() ) strDirectory = "/";

			AddPatchFileInfo( strFileName, strVersion, strSizeBefore, strChecksumBefore, strSizeAfter, strChecksumAfter, strApplicationName, strDirectory, strDescription );
		}
	}

	fclose( fp );

	return true;
}

void	PatchedInfo::Clear()
{
	if( m_file_hash.size() > 0 )
	{
		PatchFileInfo* pInfo = NULL;
		m_file_hash.get_first_value( pInfo );
		do
		{
			if( pInfo != NULL )
			{
				delete pInfo;
				pInfo = NULL;
			}

		} while( m_file_hash.get_next_value( pInfo ) );
	}

	m_file_hash.clear();
}

bool	PatchedInfo::IsPatchedFile( const std::string & _strFileName,
	size_t _nSizeBefore,
	int _nChecksumBefore,
	size_t _nSizeAfter,
	int _nChecksumAfter ) const
{
	PatchFileInfo* file = NULL;
	m_file_hash.lookup( _strFileName.c_str(), file );
	if( file != NULL )
	{
		return file->IsEqual( _nSizeBefore, _nChecksumBefore, _nSizeAfter, _nChecksumAfter );
	}

	return false;
}

void	PatchedInfo::AddPatchFileInfo( const std::string & strFileName,
	const std::string & strVersion,
	const std::string & strSizeBefore,
	const std::string & strChecksumBefore,
	const std::string & strSizeAfter,
	const std::string & strChecksumAfter,
	const std::string & strApplicationName,
	const std::string & strDirectory,
	const std::string & strDescription )
{

	PatchFileInfo* pInfo = new PatchFileInfo(
		strFileName,
		strVersion,
		strSizeBefore,
		strChecksumBefore,
		strSizeAfter,
		strChecksumAfter,
		strApplicationName,
		strDirectory,
		strDescription );

	m_file_hash.add( pInfo->strFileName, pInfo );
}