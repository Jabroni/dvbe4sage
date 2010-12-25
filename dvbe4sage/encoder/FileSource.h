#pragma once

#include "genericsource.h"

using namespace std;
class FileSource : public GenericSource
{
private:
	const wstring					m_InFileName;
#ifdef _UNICODE
	const wstring
#else
	const string
#endif
									m_CInFileName;

	// Unneeded constructors
	FileSource();
	FileSource(const FileSource&);
public:
	FileSource(LPCWSTR inFileName);

	virtual bool startPlayback(USHORT onid, bool startFullTransponderDump);
	virtual LPCTSTR getSourceFriendlyName() const					{ return m_CInFileName.c_str(); }
	virtual bool getLockStatus()									{ return true; }
};
