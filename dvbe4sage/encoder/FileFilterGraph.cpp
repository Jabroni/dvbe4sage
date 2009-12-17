#include "StdAfx.h"
#include "FileFilterGraph.h"
#include "logger.h"

FileFilterGraph::FileFilterGraph(LPCOLESTR pszFileName) :
	m_FileName(pszFileName)
{
}

void FileFilterGraph::ReleaseInterfaces()
{
	if(m_pFileReader)
		m_pFileReader = NULL;

	GenericFilterGraph::ReleaseInterfaces();
}

HRESULT FileFilterGraph::BuildGraph()
{
	HRESULT hr = S_OK;

	// if we have already have a filter graph, tear it down
	if (m_fGraphBuilt)
	{
		if(m_fGraphRunning)
			hr = StopGraph();

		hr = TearDownGraph();
	}

	// STEP 1: Load file filter
	if(FAILED(hr = AddFileSource()))
	{
		log(0, true, m_iTunerNumber, TEXT("Cannot load the file source, error 0x%.08X\n"), hr);
		BuildGraphError();
		return hr;
	}

	// Create out filter
	m_pDVBFilter = new DVBFilter(m_iTunerNumber);

	// Step5: Finally, add our filter to the graph
	if(FAILED(hr = m_pFilterGraph->AddFilter(m_pDVBFilter, L"DVB Capture Filter")))
	{
		log(0, true, m_iTunerNumber, TEXT("Cannot add our filter to the graph, error 0x%.08X\n"), hr);
		BuildGraphError();
		return hr;
	}

	// And connect it
	if(FAILED(hr = ConnectFilters(m_pFileReader, m_pDVBFilter)))
	{
		log(0, true, m_iTunerNumber, TEXT("Cannot connect our filter to the graph, error 0x%.08X\n"), hr);
		BuildGraphError();
		return hr;
	}

	log(0, true, m_iTunerNumber, TEXT("Loaded our transport stream filter\n"));

	// Step5: Load demux
	if (FAILED(hr = LoadDemux()))
	{
		log(0, true, m_iTunerNumber, TEXT("Cannot load demux, error 0x%.08X\n"), hr);
		BuildGraphError();
		return hr;
	}

	// Step6: Render demux pins
	if (FAILED(hr = RenderDemux()))
	{
		log(0, true, m_iTunerNumber, TEXT("Cannot Rend demux, error 0x%.08X\n"), hr);
		BuildGraphError();
		return hr;
	}

	m_fGraphBuilt = true;
	m_fGraphFailure = false;

	return S_OK;
}

HRESULT FileFilterGraph::TearDownGraph()
{
	if(m_fGraphBuilt || m_fGraphFailure)
	{
		if(m_pFileReader)
		{
			m_pFilterGraph->RemoveFilter(m_pFileReader);
			m_pFileReader = NULL;
		}
	}
	return GenericFilterGraph::TearDownGraph();
}

HRESULT FileFilterGraph::AddFileSource()
{
	HRESULT hr = S_OK;

	hr = m_pFileReader.CoCreateInstance(CLSID_AsyncReader);
	if(FAILED(hr))
	{
		log(0, true, m_iTunerNumber, TEXT("Cannot CoCreateInstance CLSID_AsyncReader, error 0x%.08X\n"), hr);
		return hr;
	}

	hr = m_pFilterGraph->AddFilter(m_pFileReader, L"File Reader");
	if(FAILED(hr))
	{
		log(0, true, m_iTunerNumber, TEXT("Unable to add the file reader filter to graph, error 0x%.08X\n"), hr);
		return hr;
	}

	log(0, true, m_iTunerNumber, TEXT("Added file reader filter to the graph\n"));

	CComQIPtr<IFileSourceFilter> pFileSource(m_pFileReader);

	if(pFileSource)
		if(FAILED(hr = pFileSource->Load(m_FileName.c_str(), NULL)))
			log(0, true, m_iTunerNumber, TEXT("Cannot load the file \"%S\", error 0x%.08X\n"), m_FileName.c_str(), hr);
		else;
	else
		log(0, true, m_iTunerNumber, TEXT("Cannot QI for IFileSourceFilter on the Async Reader\n"));
	return hr;
}
