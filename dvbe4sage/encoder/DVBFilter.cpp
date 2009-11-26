#include "stdafx.h"

#include "DVBFilter.h"
#include "configuration.h"

GUID CLSID_DVBFilter = { 0x36A5F770, 0xFE4C, 0x11CE, 0xA8, 0xED, 0x00, 0xAA, 0x00, 0x2F, 0xEC, 0xB0 };

DVBFilter::DVBFilter() :
    CBaseFilter(NAME("DVBFilter"), NULL, &m_Lock, CLSID_DVBFilter)
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
		//m_pPin1->GetConnected()->Disconnect();
		m_pPin1->Disconnect();
	}
	delete m_pPin1;
	if(m_pPin2 && m_pPin2->IsConnected())
	{
		//m_pPin2->GetConnected()->Disconnect();
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
	request.cBuffers = 100;
	HRESULT hr = pAlloc->SetProperties(&request, &result);
	return hr;
}

/*STDMETHODIMP DVBFilter::NonDelegatingQueryInterface(REFIID riid,
													void **ppv)
{
    CheckPointer(ppv,E_POINTER);
	IID un1 = { 0xABBA0018, 0x3075, 0x11D6, 0x88, 0xA4, 0x00, 0xB0, 0xD0, 0x20, 0x0F, 0x88};
	IID un2 = { 0xD749E960, 0x0C9F, 0x11D3, 0x8F, 0xF2, 0x00, 0xA0, 0xC9, 0x22, 0x4C, 0xF4};

	if(riid == __uuidof(ITuneRequestInfo))
		return GetInterface((ITuneRequestInfo *) this, ppv);

	if(riid == __uuidof(IFrequencyMap))
		return GetInterface((IFrequencyMap *) this, ppv);

	if(riid == __uuidof(IConnectionPointContainer))
		return GetInterface((IConnectionPointContainer *) this, ppv);

	if(riid == un1 || riid == un2)
		return S_OK;

    return CBaseFilter::NonDelegatingQueryInterface(riid, ppv);
} // NonDelegatingQueryInterface
*/

DVBFilterInputPin::DVBFilterInputPin(DVBFilter* pDVBFilter,
									 CCritSec* pLock,
									 HRESULT* phr) :
    CRenderedInputPin(NAME("DVBFilterInputPin"), pDVBFilter, pLock, phr, L"Input")
{
}

HRESULT DVBFilterInputPin::CheckMediaType(const CMediaType* pMediaType)
{
	return S_OK;
	/*if(*pMediaType->Type() == MEDIATYPE_Stream && *pMediaType->Subtype() == KSDATAFORMAT_SUBTYPE_BDA_MPEG2_TRANSPORT)
		return S_OK;
	else
		return VFW_E_INVALIDMEDIATYPE;*/
}

/*HRESULT DVBFilterInputPin::GetMediaType(int iPosition,
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
}*/

STDMETHODIMP DVBFilterInputPin::ReceiveCanBlock()
{
    return S_OK;
}

// We have very spicific requirement for the buffer size, it must be devisible by 188!
STDMETHODIMP DVBFilterInputPin::GetAllocatorRequirements(ALLOCATOR_PROPERTIES *pProps)
{
	pProps->cbBuffer = 188 * g_pConfiguration->getTSPacketsPerBuffer();
	pProps->cBuffers = g_pConfiguration->getNumberOfBuffers();
	return S_OK;
}

STDMETHODIMP DVBFilterInputPin::Receive(IMediaSample *pSample)
{
	// Autolock
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
	// Autolock
	CAutoLock lock(m_pLock);

	return CRenderedInputPin::EndOfStream();
}

DummyNetworkProviderOutputPin::DummyNetworkProviderOutputPin(CBaseFilter* pFilter,
															 CCritSec* pLock,
															 HRESULT* phr) :
	CBaseOutputPin(NAME("Antenna Pin"), pFilter, pLock, phr, L"Antenna")
{
}

HRESULT DummyNetworkProviderOutputPin::GetMediaType(CMediaType* pMediaType)
{
	pMediaType->majortype = KSDATAFORMAT_TYPE_BDA_ANTENNA;
	//pMediaType->subtype = STATIC_KSDATAFORMAT_SUBTYPE_BDA_MPEG2_TRANSPORT;
	return S_OK;
}

// {A9BE14D7-479B-4b61-A834-2FFB73D3340D}
static const GUID CLSID_DummyNetworkProvider = 
{ 0xa9be14d7, 0x479b, 0x4b61, { 0xa8, 0x34, 0x2f, 0xfb, 0x73, 0xd3, 0x34, 0xd } };

DummyNetworkProvider::DummyNetworkProvider() :
	CBaseFilter(NAME("DummyNetworkProvider"), NULL, &m_Lock, CLSID_DummyNetworkProvider)
{
	HRESULT hr = S_OK;
    m_pPin = new DummyNetworkProviderOutputPin(this, &m_Lock, &hr);
}

DummyNetworkProvider::~DummyNetworkProvider()
{
	if(m_pPin && m_pPin->IsConnected())
	{
		m_pPin->GetConnected()->Disconnect();
		m_pPin->Disconnect();
		delete m_pPin;
	}
}

STDMETHODIMP DummyNetworkProvider::NonDelegatingQueryInterface(REFIID riid,
															   void **ppv)
{
    CheckPointer(ppv,E_POINTER);

	if(riid == __uuidof(IBDA_NetworkProvider))
		return GetInterface((IBDA_NetworkProvider *) this, ppv);

    return CBaseFilter::NonDelegatingQueryInterface(riid, ppv);
} // NonDelegatingQueryInterface

STDMETHODIMP DummyNetworkProvider::PutSignalSource(ULONG ulSignalSource)
{
	m_ulSignalSource = ulSignalSource;
	return S_OK;
}

STDMETHODIMP DummyNetworkProvider::GetSignalSource(ULONG *pulSignalSource)
{
	*pulSignalSource = m_ulSignalSource;
	return S_OK;
}

STDMETHODIMP DummyNetworkProvider::GetNetworkType(GUID *pguidNetworkType)
{
	*pguidNetworkType = CLSID_DVBSNetworkProvider;
	return S_OK;
}

STDMETHODIMP DummyNetworkProvider::PutTuningSpace(REFGUID guidTuningSpace)
{
	m_guidTuningSpace = guidTuningSpace;
	return S_OK;
}

STDMETHODIMP DummyNetworkProvider::GetTuningSpace(GUID *pguidTuingSpace)
{
	*pguidTuingSpace = m_guidTuningSpace;
	return S_OK;
}

STDMETHODIMP DummyNetworkProvider::RegisterDeviceFilter(IUnknown *pUnkFilterControl,
														ULONG *ppvRegisitrationContext)
{
	return S_OK;
}

STDMETHODIMP DummyNetworkProvider::UnRegisterDeviceFilter(ULONG pvRegistrationContext)
{
	return S_OK;
}
