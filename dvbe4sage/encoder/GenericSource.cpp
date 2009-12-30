#include "StdAfx.h"
#include "GenericSource.h"

GenericSource::GenericSource() :
	m_pFilterGraph(NULL),
	m_IsSourceOK(true)
{
}

GenericSource::~GenericSource()
{
	if(m_pFilterGraph != NULL)
	{
		// Stop the graph if it's running
		if(m_pFilterGraph->m_fGraphRunning)
			m_pFilterGraph->StopGraph();

		// Destroy the graph
		m_pFilterGraph->TearDownGraph();
		// And delete the object
		delete m_pFilterGraph;
	}
}

void GenericSource::stopPlayback()
{
	// Get the filter parser
	DVBParser* pParser = m_pFilterGraph->getParser();

	if(pParser != NULL)
	{
		// Stop the full transponder dump
		pParser->stopTransponderDump();

		// And tell the parser to stop processing PSI packets
		pParser->stopPSIPackets();
	}

	// The tuner stops the running graph
	m_pFilterGraph->StopGraph();

	// Destroy the graph
	m_pFilterGraph->TearDownGraph();
}
