// EIT2XMLTVParser.cpp : Implementation of DVBFilterToParserProxy

#include "stdafx.h"
#include "DVBFilter.h"

GUID CLSID_DVBFilter = {
	0x36a5f770, 0xfe4c, 0x11ce, 0xa8, 0xed, 0x00, 0xaa, 0x00, 0x2f, 0xec, 0xb0 };

// Constructor

DVBFilter::DVBFilter(DVBFilterToParserProxy *pDVBFilter,
					 LPUNKNOWN pUnk,
					 CCritSec* pLock,
					 HRESULT *phr) :
    CBaseFilter(NAME("DVBFilter"), pUnk, pLock, CLSID_DVBFilter),
    m_pDVBFilter(pDVBFilter)
{
}


//
// GetPin
//
CBasePin * DVBFilter::GetPin(int n)
{
	if(n == 0)
		return m_pDVBFilter->m_pPin;
	else
		return NULL;
}


//
// GetPinCount
//
int DVBFilter::GetPinCount()
{
    return 1;
}


//
//  Definition of DVBFilterInputPin
//
DVBFilterInputPin::DVBFilterInputPin(DVBFilterToParserProxy *pDVBFilter,
									 CBaseFilter *pFilter,
									 CCritSec* pLock,
									 HRESULT *phr) :
    CRenderedInputPin(NAME("DVBFilterInputPin"),
					  pFilter,						// Filter
					  pLock,						// Locking
					  phr,							// Return code
					  L"Input"),					// Pin name
    m_pDVBFilter(pDVBFilter)
{
}


//
// CheckMediaType
//
// Check if the pin can support this specific proposed type and format
//
HRESULT DVBFilterInputPin::CheckMediaType(const CMediaType *)
{
    return S_OK;
}

//
// ReceiveCanBlock
//
// We don't hold up source threads on Receive
//
STDMETHODIMP DVBFilterInputPin::ReceiveCanBlock()
{
    return S_FALSE;
}

//
// Receive
//
// Do something with this media sample
//

STDMETHODIMP DVBFilterInputPin::Receive(IMediaSample *pSample)
{
	// Copy the data to the file
	PBYTE pbData;
	pSample->GetPointer(&pbData);

	if(m_pDVBFilter->m_Parser.parseStream(pbData, pSample->GetActualDataLength()))
		m_pDVBFilter->m_LastWriteTickCounter = GetTickCount();

	return S_OK;
}

//
//  DVBFilterToParserProxy class
//
DVBFilterToParserProxy::DVBFilterToParserProxy(FILE* const pOutFile) :
    CUnknown(NAME("DVBFilterToParserProxy"), NULL),
    m_pFilter(NULL),
    m_pPin(NULL),
	m_LastWriteTickCounter(0),
	m_pOutFile(pOutFile),
	m_Parser(pOutFile)
{
    HRESULT hr = S_OK;
    
    m_pFilter = new DVBFilter(this, GetOwner(), &m_Lock, &hr);
    m_pPin = new DVBFilterInputPin(this, m_pFilter, &m_Lock, &hr);
}

// Destructor
DVBFilterToParserProxy::~DVBFilterToParserProxy()
{
    delete m_pPin;
    delete m_pFilter;
}
