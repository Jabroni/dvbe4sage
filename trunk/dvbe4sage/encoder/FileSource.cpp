#include "StdAfx.h"

#include "FileSource.h"
#include "FileFilterGraph.h"
#include "logger.h"

FileSource::FileSource(LPCWSTR inFileName) :
	m_InFileName(inFileName),
	m_CInFileName(CW2CT(inFileName))
{
	// Create the right filter graph
	m_pFilterGraph = new FileFilterGraph(inFileName);

	// Build the graph
	if(FAILED(m_pFilterGraph->BuildGraph()))
	{
		log(0, true, 0, TEXT("Error: Could not Build the file filter graph for the input file \"%s\"\n"), m_CInFileName.c_str());
		m_IsSourceOK = false;
	}
}

bool FileSource::startPlayback(bool startFullTransponderDump)
{
	// Run the graph
	HRESULT hr = S_OK;
	if(FAILED(hr = m_pFilterGraph->RunGraph()))
	{
		log(0, true, 0, TEXT("Error: Could not play the file filter graph, error 0x%.08X\n"), hr);
		return false;
	}
	else
		return true;
}
