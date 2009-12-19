#pragma once

#include "GenericFilterGraph.h"

class GenericSource
{
protected:
	// DirectShow graph
	GenericFilterGraph*		m_pFilterGraph;
	bool					m_IsSourceOK;

public:
	GenericSource();
	virtual ~GenericSource();

	virtual bool startPlayback(bool startFullTransponderDump) = 0;
	void stopPlayback();
	virtual LPCTSTR getSourceFriendlyName() const = 0;
	virtual bool getLockStatus() = 0;

	DVBParser* getParser()							{ return m_pFilterGraph->getParser(); }
	int getSourceOrdinal() const					{ return m_pFilterGraph->getTunerOrdinal(); }
	bool running()									{ return m_pFilterGraph->m_fGraphRunning; }
	bool isSourceOK() const							{ return m_IsSourceOK; }
};
