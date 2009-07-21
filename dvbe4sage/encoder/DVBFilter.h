// DVBFilter.h : Declaration of the DVBFilter and stuff

#pragma once

#include "DVBParser.h"

class DVBFilter;

class DVBFilterInputPin : public CRenderedInputPin
{
private:
	// Disallow default and copy constructors
	DVBFilterInputPin();
	DVBFilterInputPin(const DVBFilterInputPin&);
public:
	// Constructor
    DVBFilterInputPin(DVBFilter* pDVBFilter, CCritSec* pLock, HRESULT* phr);

    // Do something with this media sample
    STDMETHODIMP Receive(IMediaSample* pSample);

	// We need to indicate the sender that we can block the processing
    STDMETHODIMP ReceiveCanBlock();

    // Check if the pin can support this specific proposed type and format (we support everything)
	HRESULT CheckMediaType(const CMediaType*);
	//HRESULT GetMediaType(int iPosition, CMediaType* pMediaType);

	// We override this method since our buffer must be mod(188) == 0!
	STDMETHODIMP GetAllocatorRequirements(ALLOCATOR_PROPERTIES* pProps);

	// We need to override this method in order to handle synchronization issues
	STDMETHODIMP EndOfStream(void);
};

class DVBFilterOutputPin : public CBaseOutputPin
{
protected:
	HRESULT GetMediaType(int iPosition, CMediaType* pMediaType);
	HRESULT CheckMediaType(const CMediaType* pMediaType);
	HRESULT DecideBufferSize(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* ppropInputRequest);
public:
	DVBFilterOutputPin(CBaseFilter* pFilter, CCritSec* pLock, HRESULT* phr);
};

class DVBFilter : public CBaseFilter/*,
				  public ITuneRequestInfo,
				  public IFrequencyMap,
				  public IConnectionPointContainer*/
{
private:
	// Data members
    DVBFilterInputPin*			m_pPin1;						// This is out only input pin
	DVBFilterOutputPin*			m_pPin2;						// This is out only output pin
	CCritSec					m_Lock;							// Main renderer critical section
	DVBParser					m_Parser;						// The parser itself

	// Disallow copy constructor
	DVBFilter(const DVBFilter&);

public:
	//DECLARE_IUNKNOWN

	// Constructor
    DVBFilter();
	// Destructor
	virtual ~DVBFilter();

    // Pin enumeration
	CBasePin* GetPin(int n);
	int GetPinCount() { return 2; }

	// Provide access to the parser object
	DVBParser& getParser() { return m_Parser; }

	/*STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void **ppv);

	STDMETHODIMP GetLocatorData(ITuneRequest *Request) { return E_NOTIMPL; }
	STDMETHODIMP GetComponentData(ITuneRequest *CurrentRequest)  { return E_NOTIMPL; }
	STDMETHODIMP CreateComponentList(ITuneRequest *CurrentRequest)  { return E_NOTIMPL; }
	STDMETHODIMP GetNextProgram(ITuneRequest *CurrentRequest, ITuneRequest **TuneRequest)  { return E_NOTIMPL; }
	STDMETHODIMP GetPreviousProgram(ITuneRequest *CurrentRequest, ITuneRequest **TuneRequest)  { return E_NOTIMPL; }
	STDMETHODIMP GetNextLocator(ITuneRequest *CurrentRequest, ITuneRequest **TuneRequest)  { return E_NOTIMPL; }
	STDMETHODIMP GetPreviousLocator(ITuneRequest *CurrentRequest, ITuneRequest **TuneRequest)  { return E_NOTIMPL; }

	STDMETHODIMP get_FrequencyMapping(ULONG *ulCount, ULONG **ppulList) { return E_NOTIMPL; }
	STDMETHODIMP put_FrequencyMapping(ULONG ulCount, ULONG pList[]) { return E_NOTIMPL; }
	STDMETHODIMP get_CountryCode(ULONG *pulCountryCode) { return E_NOTIMPL; }
	STDMETHODIMP put_CountryCode(ULONG ulCountryCode) { return E_NOTIMPL; }
	STDMETHODIMP get_DefaultFrequencyMapping(ULONG ulCountryCode, ULONG *pulCount, ULONG **ppulList) { return E_NOTIMPL; }
	STDMETHODIMP get_CountryCodeList(ULONG *pulCount, ULONG **ppulList) { return E_NOTIMPL; }

	STDMETHODIMP EnumConnectionPoints(IEnumConnectionPoints **ppEnum) { return E_NOTIMPL; }
	STDMETHODIMP FindConnectionPoint(REFIID riid, IConnectionPoint **ppCP) { return E_NOTIMPL; }*/
};

class DummyNetworkProvider;

class DummyNetworkProviderOutputPin : public CBaseOutputPin
{
protected:
	HRESULT GetMediaType(CMediaType* pMediaType);
	HRESULT CheckMediaType(const CMediaType*) { return S_OK; }
	HRESULT DecideBufferSize(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* ppropInputRequest) { return S_OK; }
public:
	DummyNetworkProviderOutputPin(CBaseFilter* pFilter, CCritSec* pLock, HRESULT* phr);
};

class DummyNetworkProvider : public CBaseFilter,
							 public IBDA_NetworkProvider
{
private:
	// Data members
    DummyNetworkProviderOutputPin*	m_pPin;							// This is out only input pin
	CCritSec						m_Lock;							// Main renderer critical section

	GUID							m_guidTuningSpace;
	ULONG							m_ulSignalSource;
public:
	DECLARE_IUNKNOWN

	DummyNetworkProvider();
	virtual ~DummyNetworkProvider();

	// Pin enumeration
	CBasePin* GetPin(int n) { return n == 0 ? m_pPin : NULL; }
	int GetPinCount() { return 1; }

	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void **ppv);

	STDMETHODIMP PutSignalSource(ULONG ulSignalSource);
	STDMETHODIMP GetSignalSource(ULONG *pulSignalSource);
	STDMETHODIMP GetNetworkType(GUID *pguidNetworkType);
	STDMETHODIMP PutTuningSpace(REFGUID guidTuningSpace);
	STDMETHODIMP GetTuningSpace(GUID *pguidTuingSpace);
	STDMETHODIMP RegisterDeviceFilter(IUnknown *pUnkFilterControl,ULONG *ppvRegisitrationContext);
	STDMETHODIMP UnRegisterDeviceFilter(ULONG pvRegistrationContext);
};
