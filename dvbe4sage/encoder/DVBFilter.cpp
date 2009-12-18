#include "stdafx.h"

#include "DVBFilter.h"
#include "configuration.h"

GUID CLSID_DVBFilter = { 0x36A5F770, 0xFE4C, 0x11CE, 0xA8, 0xED, 0x00, 0xAA, 0x00, 0x2F, 0xEC, 0xB0 };

DVBFilter::DVBFilter(UINT ordinal) :
    CBaseFilter(NAME("DVBFilter"), NULL, &m_Lock, CLSID_DVBFilter),
	m_Parser(ordinal)
{
	HRESULT hr = S_OK;
    m_pPin1 = new DVBFilterInputPin(this, &m_Lock, &hr);
	m_pPin2 = new DVBFilterOutputPin(this, &m_Lock, &hr);
}

CBasePin* DVBFilter::GetPin(int n)
{
	if(n == 0)
		return m_pPin1;
	else if(n == 1)
		return m_pPin2;
	else
		return NULL;
}

DVBFilter::~DVBFilter()
{
	if(m_pPin1 && m_pPin1->IsConnected())
	{
		m_pPin1->Disconnect();
	}
	delete m_pPin1;
	if(m_pPin2 && m_pPin2->IsConnected())
	{
		m_pPin2->Disconnect();
	}
	delete m_pPin2;
}

DVBFilterOutputPin::DVBFilterOutputPin(CBaseFilter* pFilter,
									   CCritSec* pLock,
									   HRESULT* phr) :
	CBaseOutputPin(NAME("TS Out"), pFilter, pLock, phr, L"TS Output")
{
}

HRESULT DVBFilterOutputPin::GetMediaType(int iPosition,
										 CMediaType* pMediaType)
{
	if(iPosition == 0)
	{
		pMediaType->SetType(&MEDIATYPE_Stream);
		pMediaType->SetSubtype(&KSDATAFORMAT_SUBTYPE_BDA_MPEG2_TRANSPORT);
		return S_OK;
	}
	else
		return VFW_S_NO_MORE_ITEMS;
}

HRESULT DVBFilterOutputPin::CheckMediaType(const CMediaType* pMediaType)
{
	return S_OK;
}

HRESULT DVBFilterOutputPin::DecideBufferSize(IMemAllocator* pAlloc,
											 ALLOCATOR_PROPERTIES* ppropInputRequest)
{
	ALLOCATOR_PROPERTIES request, result;
	request.cbAlign = 1;
	request.cbBuffer = 188;
	request.cbPrefix = 0;
	request.cBuffers = 1;
	HRESULT hr = pAlloc->SetProperties(&request, &result);
	return hr;
}

DVBFilterInputPin::DVBFilterInputPin(DVBFilter* pDVBFilter,
									 CCritSec* pLock,
									 HRESULT* phr) :
    CRenderedInputPin(NAME("DVBFilterInputPin"), pDVBFilter, pLock, phr, L"Input"),
	m_PullPin(this)
{
}

HRESULT DVBFilterInputPin::CheckMediaType(const CMediaType* pMediaType)
{
	return S_OK;
}

HRESULT DVBFilterInputPin::CheckConnect(IPin* pPin)
{
	HRESULT hr = m_PullPin.Connect(pPin, NULL, FALSE);
	return SUCCEEDED(hr) && CRenderedInputPin::CheckConnect(pPin);
}

HRESULT DVBFilterInputPin::BreakConnect()
{
	return m_PullPin.Disconnect();
}

HRESULT DVBFilterInputPin::Active()
{
	HRESULT hr = m_PullPin.Active();
	return SUCCEEDED(hr) && CRenderedInputPin::Active();
}

HRESULT DVBFilterInputPin::Inactive()
{
	HRESULT hr = m_PullPin.Inactive();
	return SUCCEEDED(hr) && CRenderedInputPin::Inactive();
}

STDMETHODIMP DVBFilterInputPin::ReceiveCanBlock()
{
    return S_OK;
}

// We have very specific requirement for the buffer size, it must be divisible by 188!
STDMETHODIMP DVBFilterInputPin::GetAllocatorRequirements(ALLOCATOR_PROPERTIES *pProps)
{
	pProps->cbBuffer = 188 * g_pConfiguration->getTSPacketsPerBuffer();
	pProps->cBuffers = g_pConfiguration->getNumberOfBuffers();
	pProps->cbAlign = 0;
	pProps->cbPrefix = 0;
	return S_OK;
}

STDMETHODIMP DVBFilterInputPin::Receive(IMediaSample *pSample)
{
	// Auto lock
	CAutoLock lock(m_pLock);

	// Copy the data to the file
	PBYTE pbData;
	pSample->GetPointer(&pbData);

	// Just pass the data to the parser
	((DVBFilter*)m_pFilter)->getParser().parseTSStream(pbData, pSample->GetActualDataLength());

	return S_OK;
}

STDMETHODIMP DVBFilterInputPin::EndOfStream(void)
{
	// Auto lock
	CAutoLock lock(m_pLock);

	return CRenderedInputPin::EndOfStream();
}

HRESULT DVBPullPin::Receive(IMediaSample* sample)
{
	return m_pParentPin->Receive(sample);
}

HRESULT DVBPullPin::EndOfStream()
{
	return m_pParentPin->EndOfStream();
}

void DVBPullPin::OnError(HRESULT hr)
{
}

HRESULT DVBPullPin::BeginFlush()
{
	return m_pParentPin->BeginFlush();
}

HRESULT DVBPullPin::EndFlush()
{
	return m_pParentPin->EndFlush();
}

// We have very specific requirement for the buffer size, it must be divisible by 188!
HRESULT DVBPullPin::DecideAllocator(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* pProps)
{
    ALLOCATOR_PROPERTIES *pRequest;
    ALLOCATOR_PROPERTIES Request;
    if(pProps == NULL)
	{
		Request.cBuffers = g_pConfiguration->getNumberOfBuffers() / 2;
		Request.cbBuffer = 188 * g_pConfiguration->getTSPacketsPerBuffer() / 2;
		Request.cbAlign = 0;
		Request.cbPrefix = 0;
		pRequest = &Request;
    }
	else
		pRequest = pProps;

	return CPullPin::DecideAllocator(pAlloc, pRequest);
}
