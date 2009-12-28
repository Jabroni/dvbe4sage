// EIT2XMLTVParser.h : Declaration of the DVBFilter and stuff

#pragma once

#include "DVBParser.h"

class DVBFilterToParserProxy;

class DVBFilter : public CBaseFilter
{
private:
	DVBFilterToParserProxy*	const m_pDVBFilter;     // Main renderer object
public:
	// Constructor
    DVBFilter(DVBFilterToParserProxy *pDVBFilter,
			  LPUNKNOWN pUnk,
			  CCritSec* pLock,
			  HRESULT* phr);
	// Destructor
	virtual ~DVBFilter() {}

    // Pin enumeration
    CBasePin* GetPin(int n);
    int GetPinCount();
};

class DVBFilterInputPin : public CRenderedInputPin
{
    DVBFilterToParserProxy* const	m_pDVBFilter;     // Main renderer object
public:
    DVBFilterInputPin(DVBFilterToParserProxy *pDVBFilter,
					  CBaseFilter* pFilter,
					  CCritSec* pLock,
					  HRESULT* phr);

    // Do something with this media sample
    STDMETHODIMP Receive(IMediaSample *pSample);
    STDMETHODIMP ReceiveCanBlock();

    // Check if the pin can support this specific proposed type and format
	HRESULT CheckMediaType(const CMediaType *);
};

class DVBFilterToParserProxy : public CUnknown
{
	friend class DVBFilter;
    friend class DVBFilterInputPin;

private:
    DVBFilter*					m_pFilter;							// This is the filter
    DVBFilterInputPin*			m_pPin;								// And its input pin

	CCritSec					m_Lock;								// Main renderer critical section

    FILE* const					m_pOutFile;							// Output file
	DWORD						m_LastWriteTickCounter;				// Last write tick counter
	DVBParser					m_Parser;							// The parser itself

public:
    DECLARE_IUNKNOWN

    DVBFilterToParserProxy(FILE* const pOutFile);
    virtual ~DVBFilterToParserProxy();

	DVBFilter* GetFilter()	{ return m_pFilter; }
	DVBFilterInputPin* GetPin()	{ return m_pPin; }

	DWORD GetLastWriteTickCount() const	{ return m_LastWriteTickCounter; }
};
