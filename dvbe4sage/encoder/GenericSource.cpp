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
	// Stop the full transponder dump
	m_pFilterGraph->getParser()->stopTransponderDump();

	// Tell the parser to stop processing PSI packets
	m_pFilterGraph->getParser()->stopPSIPackets();

	// The tuner stops the running graph
	m_pFilterGraph->StopGraph();

	// Destroy the graph
	m_pFilterGraph->TearDownGraph();
}
