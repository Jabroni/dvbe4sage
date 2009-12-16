#pragma once

#include "DVBFilter.h"

class GenericFilterGraph
{
protected:
	HRESULT LoadFilter(REFCLSID clsid, 
					   IBaseFilter** ppFilter,
					   IBaseFilter* pConnectFilter, 
					   BOOL fIsUpstream,
					   BOOL useCounter);

    HRESULT ConnectFilters(IBaseFilter* pFilterUpstream, 
						   IBaseFilter* pFilterDownstream);

	HRESULT InitializeGraphBuilder();
	HRESULT LoadDemux();
    HRESULT RenderDemux();
	void BuildGraphError();

	IPin* FindPinOnFilter(IBaseFilter *pBaseFilter, char *pPinName,BOOL bCheckPinName);	

	virtual void ReleaseInterfaces();

	CComPtr<IGraphBuilder>	m_pFilterGraph;				// for current graph
	CComPtr<ICreateDevEnum>	m_pICreateDevEnum;			// for enumerating system devices
	CComPtr<IBaseFilter>	m_pDemux;					// for demux filter
    CComPtr<IBaseFilter>	m_pTIF;						// for transport information filter
	CComPtr<IMediaControl>	m_pIMediaControl;			// for controlling graph state
	DVBFilter*				m_pDVBFilter;				// Our filter
	UINT					m_iTunerNumber;				// Tuner ordinal number (0 for any non-tuner graph)

public:
	GenericFilterGraph();
	virtual ~GenericFilterGraph();

	bool m_fGraphBuilt;
    bool m_fGraphRunning;
    bool m_fGraphFailure;

	virtual HRESULT BuildGraph() = 0;
	virtual HRESULT RunGraph();
    virtual HRESULT StopGraph();
    virtual HRESULT TearDownGraph();

	DVBParser& getParser() { return m_pDVBFilter->getParser(); }

	int getTunerOrdinal() const { return m_iTunerNumber; }
};
