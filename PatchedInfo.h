
#pragma once

#include <string>

#include <toolkit/khash.h>
#include <toolkit/XStringUtil.h>


class	PatchedInfo
{
private:

	struct PatchFileInfo
	{
		PatchFileInfo(
			const std::string & _strFileName,
			const std::string & _strVersion,
			const std::string & _strSizeBefore,
			const std::string & _strChecksumBefore,
			const std::string & _strSizeAfter,
			const std::string & _strChecksumAfter,
			const std::string & _strApplicationName,
			const std::string & _strDirectory,
			const std::string & _strDescription ) 	
			: strFileName( _strFileName )
			, strChecksumBefore( _strChecksumBefore )
			, strChecksumAfter( _strChecksumAfter )
			, strApplicationName( _strApplicationName )
			, strDirectory( _strDirectory )
			, strDescription( _strDescription )
		{
			nVersion = atoi( _strVersion.c_str() );
			nSizeBefore = atoi( _strSizeBefore.c_str() );
			nSizeAfter = atoi( _strSizeAfter.c_str() );
		}

		bool	IsEqual( size_t _nSizeBefore, int _nChecksumBefore, size_t _nSizeAfter, int _nChecksumAfter ) const
		{
			if( _nSizeBefore == nSizeBefore &&
				_nSizeAfter == nSizeAfter )
			{
				char szChecksumBefore[16];
				char szChecksumAfter[16];
				s_sprintf( szChecksumBefore, _countof( szChecksumBefore ), "%08X", _nChecksumBefore );
				s_sprintf( szChecksumAfter, _countof( szChecksumAfter ), "%08X", _nChecksumAfter );

				if( strChecksumBefore == szChecksumBefore &&
					strChecksumAfter == szChecksumAfter )
				{
					return true;
				}
			}

			return false;
		}

		std::string strFileName;
		int			nVersion;
		int			nSizeBefore;
		std::string strChecksumBefore;
		int			nSizeAfter;
		std::string strChecksumAfter;
		std::string strApplicationName;
		std::string strDirectory;
		std::string strDescription;
	};

public:

	PatchedInfo();
	~PatchedInfo();

	void	Clear();

	bool	LoadTPFFile( const char* szFileName );

	bool	IsPatchedFile(
		const std::string & _strFileName,
		size_t _nSizeBefore,
		int _nChecksumBefore,
		size_t _nSizeAfter,
		int _nChecksumAfter ) const;

private:

	void	AddPatchFileInfo(
		const std::string & strFileName,
		const std::string & strVersion,
		const std::string & strSizeBefore,
		const std::string & strChecksumBefore,
		const std::string & strSizeAfter,
		const std::string & strChecksumAfter,
		const std::string & strApplicationName,
		const std::string & strDirectory,
		const std::string & strDescription );

private:

	KHash< PatchFileInfo*, hashPr_string_nocase >	m_file_hash;

};
