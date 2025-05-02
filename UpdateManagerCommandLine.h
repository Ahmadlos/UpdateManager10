
#pragma once

#include <afxwin.h>
#include <string>
#include "XUpdateManager.h"

class CUpdateManagerCommandLine : public CCommandLineInfo
{
public:
	CUpdateManagerCommandLine();
	virtual ~CUpdateManagerCommandLine();

	virtual void	ParseParam( const TCHAR* pszParam, BOOL bFlag, BOOL bLast );

private:
	void			initLogHandler();
	void			parseCommandLine( const std::string & command );
	std::string		getValue( const std::string & command ) const;
	void			increasePatchVersion();
	int				getLastestPatchNo() const;
	bool			checkCommandLineInfo() const;
	bool			isEmptyDirectory( const std::string & path ) const;
	bool			setUpdateManagerOption();
	void			execute();


private:
	enum			{	none = 0, encode, decode	};

	std::string		m_codingType;
 	std::string		m_appName;
	std::string		m_versionFilePath;
	std::string		m_patchFilePath;
	int				m_version;

};
