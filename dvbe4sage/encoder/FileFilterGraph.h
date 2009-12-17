#pragma once

#include "genericfiltergraph.h"

class FileFilterGraph :	public GenericFilterGraph
{
private:
	CComPtr<IBaseFilter>	m_pFileReader;
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
