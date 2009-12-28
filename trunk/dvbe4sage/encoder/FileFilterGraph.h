#pragma once

#include "genericfiltergraph.h"

class FileReaderFilter;

class FileFilterGraph :	public GenericFilterGraph
{
private:
	FileReaderFilter*		m_pFileReaderFilter;				// Our file reader filter
	wstring					m_FileName;

	HRESULT AddFileSource();

	// We don't need the default constructor
	FileFilterGraph();
protected:
	virtual void ReleaseInterfaces();

public:
	FileFilterGraph(LPCOLESTR pszFileName);

	virtual HRESULT BuildGraph();
    virtual HRESULT TearDownGraph();
};
