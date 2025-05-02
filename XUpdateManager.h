#pragma once
#include <windows.h>
#include <string>

#include "PatchedInfo.h"


class XUpdateManager
{
public:

	static XUpdateManager&	GetInstance();

	struct LogHandler
	{
		virtual void SetCancelButton( bool bEnable )		{};
		virtual void SetStatusMessage( const char *szLog )	{};
		virtual void AddLog( const char *szLog )			{};
	};

	void			SetLogHandler( LogHandler* pLog )	{ m_pLog = pLog; }
	LogHandler*		GetLogHandler() const				{ return m_pLog; }

	enum
	{
		NONE = 0, FREEZE = 1, MELT = 2, ENCODE = 1, DECODE = 2
	};

	void SetOption( int nCompress, int nNameEncode, bool bBuildIndex, bool bBuildDirectory, bool bDoXOR, const char *szAppName, int nVerison )
	{
		m_nCompress			= nCompress;
		m_nNameEncode		= nNameEncode;
		m_bBuildIndex		= bBuildIndex;
		m_bBuildDirectory	= bBuildDirectory;
		m_bDoXOR			= bDoXOR;
		m_strAppName		= szAppName;
		m_nVersion			= nVerison;
	}

	void GetOption( int & nCompress, int & nNameEncode, bool & bBuildIndex, bool & bBuildDirectory, bool & bDoXOR, std::string & strAppName, int & nVerison ) const
	{
		nCompress			= m_nCompress;
		nNameEncode			= m_nNameEncode;
		bBuildIndex			= m_bBuildIndex;
		bBuildDirectory		= m_bBuildDirectory;
		bDoXOR				= m_bDoXOR;
		strAppName			= m_strAppName;
		nVerison			= m_nVersion;
	}

	void			SetVersion( int nVersion )					{ m_nVersion = nVersion;		}
	void			SetAppName( const char* szAppName )			{ m_strAppName = szAppName;		}
	int				GetVersion( void )					const	{ return m_nVersion;			}
	std::string		GetAppName( void )					const	{ return m_strAppName;			}
	std::string		GetPatchPatchName( void )			const	{ return m_strPatchPatchName;	}

	void			SaveUpdateInfo( void );
	void			CloseHandle( void );
	void			Cancel( void );

	void			onDrop( HDROP* pFile );
	void			onSelectPath( std::string szFolderFullPath );

	bool			onDirectory( struct XOption *pOpt, const char* szBaseDirectory, const char *szPathName, std::string& strPatchInfo );
	bool			onFile( const PatchedInfo& Patched, struct XOption *pOpt, const char* szBaseDirectory, const char* szFileName, std::string& strPatchInfo );

	bool			MakeTPF( const XOption& option, const std::string& strPatchInfo );


private:

	XUpdateManager();
	virtual ~XUpdateManager();

	void 			loadUpdateInfo( void );
	void 			procPatch( const char *szPathName );

	LogHandler*		m_pLog;
	std::string		m_strAppName;
	std::string		m_strPatchPatchName;

	int				m_nCompress;
	int				m_nNameEncode;
	int				m_nVersion;

	bool 			m_bBuildIndex;
	bool 			m_bBuildDirectory;
	bool 			m_bDoXOR;

public:

	HANDLE			m_hCommandLineExecution;
};