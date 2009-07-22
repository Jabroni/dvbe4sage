#include "stdafx.h"

#include "graph.h"
#include "DVBFilter.h"
#include "Logger.h"
#include "configuration.h"
#include "extern.h"

// Constructor, initializes member variables
// and calls InitializeGraphBuilder
CBDAFilterGraph::CBDAFilterGraph(int ordinal):
	m_fGraphBuilt(FALSE),
	m_fGraphRunning(FALSE),
	m_iTunerNumber(ordinal),
	m_Tid(0),
	m_pDVBFilter(NULL),
	m_IsHauppauge(false),
	m_IsFireDTV(false)
{
	if(FAILED(InitializeGraphBuilder()))
		m_fGraphFailure = TRUE;
	else
		m_fGraphFailure = FALSE;
}


// Destructor
CBDAFilterGraph::~CBDAFilterGraph()
{
	StopGraph();

	if(m_fGraphBuilt || m_fGraphFailure)
		TearDownGraph();
	ReleaseInterfaces();
}

void
CBDAFilterGraph::ReleaseInterfaces()
{
	if(m_pITuningSpace)
		m_pITuningSpace = NULL;

	if(m_pITuner)
		m_pITuner = NULL;

	if(m_pFilterGraph)
		m_pFilterGraph = NULL;

	if(m_pIMediaControl)
		m_pIMediaControl = NULL;

	if(m_KsDemodPropSet)
		m_KsDemodPropSet = NULL;

	if(m_KsTunerPropSet)
		m_KsTunerPropSet = NULL;

	if(m_pTunerPin)
		m_pTunerPin = NULL;

	if(m_pDemodPin)
		m_pDemodPin = NULL;

	if(m_pICreateDevEnum)
		m_pICreateDevEnum = NULL;
}

// Instantiate graph object for filter graph building
HRESULT CBDAFilterGraph::InitializeGraphBuilder()
{
	HRESULT hr = S_OK;

	// we have a graph already
	if (m_pFilterGraph)
		return S_OK;

	// create the filter graph
	if (FAILED(hr = m_pFilterGraph.CoCreateInstance(CLSID_FilterGraph)))
	{
		log(0, true, TEXT("Couldn't CoCreate IGraphBuilder, error 0x%.08X\n"), hr);
		m_fGraphFailure = true;
		return hr;
	}

	return hr;
}

// BuildGraph sets up devices, adds and connects filters
HRESULT CBDAFilterGraph::BuildGraph()
{
	HRESULT hr = S_OK;

	// if we have already have a filter graph, tear it down
	if (m_fGraphBuilt)
	{
		if(m_fGraphRunning)
			hr = StopGraph();

		hr = TearDownGraph();
	}

	// STEP 1: load network provider first so that it can configure other
	// filters, such as configuring the demux to sprout output pins.
	// We also need to submit a tune request to the Network Provider so it will
	// tune to a channel
	if (FAILED(hr = LoadNetworkProvider()))
	{
		log(0, true, TEXT("Cannot load network provider, error 0x%.08X\n"), hr);
		BuildGraphError();
		return hr;
	}

	hr = m_pNetworkProvider->QueryInterface(__uuidof (ITuner), reinterpret_cast <void**> (&m_pITuner));
	if (FAILED(hr)) {
		log(0, true, TEXT("pNetworkProvider->QI: Can't QI for ITuner, error 0x%.08X\n"), hr);
		BuildGraphError();
		return hr;
	}

	// create a tune request to initialize the network provider
	// before connecting other filters
	CComPtr <IDVBTuneRequest> pDVBSTuneRequest;
	if (FAILED(hr = CreateDVBSTuneRequest(&pDVBSTuneRequest)))
	{
		log(0, true, TEXT("Cannot create tune request, error 0x%.08X\n"), hr);
		BuildGraphError();
		return hr;
	}

	//submit the tune request to the network provider
	hr = m_pITuner->put_TuneRequest(pDVBSTuneRequest);
	if (FAILED(hr)) {
		log(0, true, TEXT("Cannot submit the tune request, error 0x%.08X\n"), hr);
		BuildGraphError();
		return hr;
	}
/*
	// Step1: load our dummy source filter
	if(FAILED(hr = m_pFilterGraph->AddFilter(&m_NetworkProvider, L"Dummy Network Provider")))
	{
		log(0, true, TEXT("Cannot add dummy network provider filter to the graph, error 0x%.08X\n"), hr);
		BuildGraphError();
		return hr;
	}
*/
	// STEP2: Load tuner device and connect to network provider
//	if (FAILED(hr = LoadFilter(KSCATEGORY_BDA_NETWORK_TUNER, &m_pTunerDemodDevice, &m_NetworkProvider, TRUE, TRUE)) || !m_pTunerDemodDevice)
	if (FAILED(hr = LoadFilter(KSCATEGORY_BDA_NETWORK_TUNER, &m_pTunerDemodDevice, m_pNetworkProvider, TRUE, TRUE)) || !m_pTunerDemodDevice)
	{
		log(0, true, TEXT("Cannot load tuner device and connect network provider, error 0x%.08X\n"), hr);
		BuildGraphError();
		return hr != S_OK ? hr : E_FAIL;
	}

	// Get tuner friendly name
	FILTER_INFO filterInfo;
	if(SUCCEEDED(hr = m_pTunerDemodDevice->QueryFilterInfo(&filterInfo)))
	{
		CW2T filterName(filterInfo.achName);
		log(0, true, TEXT("Tuner Filter Info = \"%s\"\n"), (LPCTSTR)filterName);
		_tcscpy_s(m_TunerName, sizeof(m_TunerName) / sizeof(TCHAR), (LPCTSTR)filterName);

		// Let's see if this is a Hauppauge device
		if(_tcsstr(m_TunerName, TEXT("Hauppauge")) != NULL)
			m_IsHauppauge = true;

		// Let's see if this is a FireDTV device
		if(_tcsstr(m_TunerName, TEXT("FireDTV")) != NULL)
			m_IsFireDTV = true;
	}

	// Step3: load capture device and connect to tuner/demod device
	hr = LoadFilter(KSCATEGORY_BDA_RECEIVER_COMPONENT, &m_pCaptureDevice, m_pTunerDemodDevice, TRUE, FALSE);
	
	// Step4: get pointer to Tuner and Demod pins on Tuner/Demod filter
    // for setting the tuner and demod. property sets
	if(GetTunerDemodPropertySetInterfaces() == FALSE)
	{
        BuildGraphError();
		return hr;
    }

	// Create out filter
	m_pDVBFilter = new DVBFilter;

	// Step5: Finally, add our filter to the graph
	if(FAILED(hr = m_pFilterGraph->AddFilter(m_pDVBFilter, L"DVB Capture Filter")))
	{
		log(0, true, TEXT("Cannot add our filter to the graph, error 0x%.08X\n"), hr);
		BuildGraphError();
		return hr;
	}

	// And connect it
	if(FAILED(hr = ConnectFilters(m_pCaptureDevice, m_pDVBFilter)))
	{
		log(0, true, TEXT("Cannot connect our filter to the graph, error 0x%.08X\n"), hr);
		BuildGraphError();
		return hr;
	}

	log(0, true, TEXT("Loaded our transport stream filter\n"));

	// Step5: Load demux
	if (FAILED(hr = LoadDemux()))
	{
		log(0, true, TEXT("Cannot load demux, error 0x%.08X\n"), hr);
		BuildGraphError();
		return hr;
	}

	// Step6: Render demux pins
	if (FAILED(hr = RenderDemux()))
	{
		log(0, true, TEXT("Cannot Rend demux, error 0x%.08X\n"), hr);
		BuildGraphError();
		return hr;
	}
	// Register my filter with network provider
	/*CComQIPtr<IBDA_TIF_REGISTRATION> pTIFReg(m_pITuner);
	if(pTIFReg != NULL)
	{
		ULONG ctx;
		IUnknown* control;
		if(FAILED(hr = pTIFReg->RegisterTIFEx(m_pDVBFilter->GetPin(0), &ctx, &control)))
			log(0, true, TEXT("Cannot register our filter as TIF with network provider, error 0x%.08X\n"), hr);
	}*/

	m_fGraphBuilt = true;
	m_fGraphFailure = false;

	return S_OK;
}

// Loads the demux into the FilterGraph
HRESULT CBDAFilterGraph::LoadDemux()
{
	HRESULT hr = S_OK;

	hr = m_pDemux.CoCreateInstance(CLSID_MPEG2Demultiplexer);
	if (FAILED(hr))
	{
		log(0, true, TEXT("Cannot CoCreateInstance CLSID_MPEG2Demultiplexer, error 0x%.08X\n"), hr);
		return hr;
	}

	hr = m_pFilterGraph->AddFilter(m_pDemux, L"Demux");
	if (FAILED(hr))
	{
		log(0, true, TEXT("Unable to add demux filter to graph, error 0x%.08X\n"), hr);
		return hr;
	}

	log(0, true, TEXT("Added demux filter to the graph\n"));

	return hr;
}

// Renders demux output pins.
HRESULT CBDAFilterGraph::RenderDemux()
{
	HRESULT             hr = S_OK;
	CComPtr <IPin>      pIPin;
	CComPtr <IPin>      pDownstreamPin;
	CComPtr <IEnumPins> pIEnumPins;

	// connect the demux to our filter
	/*IPin* demuxInputPin;
	hr = m_pDemux->FindPin(L"MPEG-2 Stream", &demuxInputPin);
	hr = m_pFilterGraph->ConnectDirect(m_pDVBFilter->GetPin(1), demuxInputPin, NULL);*/
	hr = ConnectFilters(m_pDVBFilter, m_pDemux);

	if (FAILED(hr))
	{
		log(0, true, TEXT("Cannot connect demux to capture filter, error 0x%.08X\n"), hr);
		return hr;
	}

	log(0, true, TEXT("Connected demux to our filter output pin\n"));

	CComQIPtr<IMpeg2Demultiplexer> pDemux(m_pDemux);
	CComPtr<IEnumPins> pPinEnum = NULL;

	if(SUCCEEDED(m_pDemux->EnumPins(&pPinEnum)) && pPinEnum)
	{
		IPin* pins[10];
		ULONG cRetrieved = 0;
		if(SUCCEEDED(pPinEnum->Next(sizeof(pins) / sizeof(pins[0]), pins, &cRetrieved)) && cRetrieved > 0)
		{
			for(ULONG i = 0; i < cRetrieved; i++)
			{
				PIN_INFO pinInfo;
				if(SUCCEEDED(pins[i]->QueryPinInfo(&pinInfo)) && pinInfo.dir == PINDIR_OUTPUT)
					if(FAILED(pDemux->DeleteOutputPin(pinInfo.achName)))
						log(0, true, TEXT("Failed to delete a demux pin \"%S\"\n"), pinInfo.achName);
			}
		}
	}

	AM_MEDIA_TYPE mediaType;
	ZeroMemory(&mediaType, sizeof(AM_MEDIA_TYPE));
	mediaType.majortype = KSDATAFORMAT_TYPE_MPEG2_SECTIONS;
	mediaType.subtype = KSDATAFORMAT_SUBTYPE_DVB_SI;

	CComPtr<IPin> pPin1 = NULL;
	if(SUCCEEDED(pDemux->CreateOutputPin(&mediaType, L"For MS PSI", &pPin1)) && pPin1)
	{
		CComQIPtr<IMPEG2PIDMap> pMappedPin(pPin1);
		ULONG pids[] = { 12345 };
		if(FAILED(hr = pMappedPin->MapPID(sizeof(pids) / sizeof(pids[0]), pids, MEDIA_MPEG2_PSI)))
			log(0, true, TEXT("Failed to map PID in the demux, error 0x%.08X\n"), hr);
	}

	// load transform information filter and connect it to the demux
	hr = LoadFilter(KSCATEGORY_BDA_TRANSPORT_INFORMATION, &m_pTIF, m_pDemux, TRUE, FALSE);	
	if (FAILED(hr))
	{
		log(0, true, TEXT("Cannot load TIF, error 0x%.08X\n"), hr);
		return hr;
	}

	pPinEnum = NULL;
	pDemux.Release();

	pIEnumPins = NULL;
	return hr;
}

// Makes call to tear down the graph and sets graph failure
// flag to true
VOID CBDAFilterGraph::BuildGraphError()
{
	TearDownGraph();
	m_fGraphFailure = true;
}


// This creates a new DVBS Tuning Space entry in the registry.
HRESULT CBDAFilterGraph::CreateTuningSpace()
{
	HRESULT hr = S_OK;

	CComPtr<IDVBSTuningSpace> pIDVBTuningSpace;
	hr = pIDVBTuningSpace.CoCreateInstance(__uuidof(DVBSTuningSpace));
	if (FAILED(hr) || !pIDVBTuningSpace)
	{
		log(0, true, TEXT("Failed to create system tuning space, error 0x%.08X\n"), hr);
		return FALSE;
	}

	hr = pIDVBTuningSpace->put_SystemType(DVB_Satellite);
	if (FAILED(hr))
		return hr;

	hr = pIDVBTuningSpace->put__NetworkType(CLSID_NetworkProvider);
	if (FAILED(hr))
		return hr;

	// create DVBS Locator
	CComPtr<IDVBSLocator> pIDVBSLocator;
	hr = pIDVBSLocator.CoCreateInstance(__uuidof(DVBSLocator));
	if (FAILED(hr) || !pIDVBSLocator)
		return hr;

	// set the Tuner parameters
	hr = pIDVBSLocator->put_CarrierFrequency(m_ulCarrierFrequency);
	hr = pIDVBSLocator->put_SymbolRate(m_ulSymbolRate);
	hr = pIDVBSLocator->put_SignalPolarisation(m_SignalPolarisation);
	hr = pIDVBSLocator->put_Modulation(m_Modulation);
	hr = pIDVBSLocator->put_InnerFEC(BDA_FEC_VITERBI);
	hr = pIDVBSLocator->put_InnerFECRate(m_FECRate);

	// set this a default locator
	hr = pIDVBTuningSpace->put_DefaultLocator(pIDVBSLocator);

	// create a copy of this tuning space for the 
	// class member variable.
	hr = pIDVBTuningSpace->Clone(&m_pITuningSpace);
	if(FAILED(hr))
	{
		m_pITuningSpace = NULL;
		return hr;
	}

	pIDVBTuningSpace     = NULL;
	pIDVBSLocator        = NULL;

	return hr;
}

// Creates an DVBT Tune Request
HRESULT CBDAFilterGraph::CreateDVBSTuneRequest(IDVBTuneRequest** pTuneRequest)
{
	HRESULT hr = S_OK;

	if (pTuneRequest == NULL)
	{
		log(0, true, TEXT("Invalid pointer\n"));
		return E_POINTER;
	}

	// Making sure we have a valid tuning space
	if (m_pITuningSpace == NULL)
	{
		log(0, true, TEXT("Tuning Space is NULL\n"));
		return E_FAIL;
	}

	//  Create an instance of the DVBS tuning space
	CComQIPtr<IDVBSTuningSpace> pDVBSTuningSpace (m_pITuningSpace);
	if(!pDVBSTuningSpace)
	{
		log(0, true, TEXT("Cannot QI for an IDVBSTuningSpace\n"));
		return E_FAIL;
	}

	//=====================================================================================================
	//			Added by Michael
	//=====================================================================================================

	pDVBSTuningSpace->put_LNBSwitch(g_Configuration.getLNBSW());
	pDVBSTuningSpace->put_LowOscillator(g_Configuration.getLNBLOF1());
	pDVBSTuningSpace->put_HighOscillator(g_Configuration.getLNBLOF2());
	pDVBSTuningSpace->put_SpectralInversion(BDA_SPECTRAL_INVERSION_AUTOMATIC);

	//=====================================================================================================
	//			End of added by Michael
	//=====================================================================================================

	//  Create an empty tune request.
	CComPtr<ITuneRequest> pNewTuneRequest;
	hr = pDVBSTuningSpace->CreateTuneRequest(&pNewTuneRequest);
	if(FAILED (hr))
	{
		log(0, true, TEXT("CreateTuneRequest: Can't create tune request, error 0x%.08X\n"), hr);
		return hr;
	}

	//query for an IDVBTuneRequest interface pointer
	CComQIPtr<IDVBTuneRequest> pDVBSTuneRequest(pNewTuneRequest);
	if(!pDVBSTuneRequest)
	{
		log(0, true, TEXT("CreateDVBSTuneRequest: Can't QI for IDVBTuneRequest, error 0x%.08X\n"), hr);
		return E_FAIL;
	}

	CComPtr<IDVBSLocator> pDVBSLocator;
	hr = pDVBSLocator.CoCreateInstance(__uuidof(DVBSLocator));	
	if (FAILED(hr) || !pDVBSLocator)
		return hr;

	if(FAILED(hr))
	{
		log(0, true, TEXT("Cannot create the DVB-S locator, error 0x%.08X\n"), hr);
		return hr;
	}

	hr = pDVBSLocator->put_CarrierFrequency(m_ulCarrierFrequency);
	hr = pDVBSLocator->put_SymbolRate(m_ulSymbolRate);
	hr = pDVBSLocator->put_SignalPolarisation(m_SignalPolarisation);
	hr = pDVBSLocator->put_Modulation(m_Modulation);
	hr = pDVBSLocator->put_InnerFEC(BDA_FEC_VITERBI);
	hr = pDVBSLocator->put_InnerFECRate(m_FECRate);

	hr = pDVBSTuneRequest->put_Locator(pDVBSLocator);
	if(FAILED (hr))
	{
		log(0, true, TEXT("Cannot put the locator, error 0x%.08X\n"), hr);
		return hr;
	}

	hr = pDVBSTuneRequest.QueryInterface(pTuneRequest);

	return hr;
}


// LoadNetworkProvider loads network provider
HRESULT CBDAFilterGraph::LoadNetworkProvider()
{
	HRESULT  hr = S_OK;
	CLSID    CLSIDNetworkType;

	// obtain tuning space then load network provider
	if (m_pITuningSpace == NULL)
	{
		// so we create one.
		hr = CreateTuningSpace();
		if (FAILED(hr) || !m_pITuningSpace)
		{
			log(0, true, TEXT("Cannot create tuning space, error 0x%.08X\n"), hr);
			return E_FAIL;
		}
	}

	// Get the current Network Type clsid
	hr = m_pITuningSpace->get__NetworkType(&CLSIDNetworkType);
	if (FAILED(hr))
	{
		log(0, true, TEXT("ITuningSpace::get__NetworkType failed, error 0x%.08X\n"), hr);
		return hr;
	}

	// create the network provider based on the clsid obtained from the tuning space
	hr = m_pNetworkProvider.CoCreateInstance(CLSIDNetworkType);
	if (FAILED(hr))
	{
		log(0, true, TEXT("Cannot CoCreate Network Provider, error 0x%.08X\n"), hr);
		return hr;
	}

	//add the Network Provider filter to the graph
	hr = m_pFilterGraph->AddFilter(m_pNetworkProvider, L"Network Provider");

	return hr;
}


// enumerates through registered filters
// instantiates the the filter object and adds it to the graph
// it checks to see if it connects to upstream filter
// if not,  on to the next enumerated filter
// used for tuner, capture, MPE Data Filters and decoders that
// could have more than one filter object
// if pUpstreamFilter is NULL don't bother connecting
HRESULT CBDAFilterGraph::LoadFilter(REFCLSID clsid, 
									IBaseFilter** ppFilter,
									IBaseFilter* pConnectFilter, 
									BOOL fIsUpstream,
									BOOL useCounter)
{
	HRESULT                hr = S_OK;
	BOOL                   fFoundFilter = FALSE;
	CComPtr <IMoniker>     pIMoniker;
	CComPtr <IEnumMoniker> pIEnumMoniker;

	if (!m_pICreateDevEnum)
	{
		hr = m_pICreateDevEnum.CoCreateInstance(CLSID_SystemDeviceEnum);
		if (FAILED(hr))
		{
			log(0, true, TEXT("LoadFilter(): Cannot CoCreate ICreateDevEnum, error 0x%.08X\n"), hr);
			return hr;
		}
	}

	// obtain the enumerator
	hr = m_pICreateDevEnum->CreateClassEnumerator(clsid, &pIEnumMoniker, 0);
	
	// the call can return S_FALSE if no moniker exists, so explicitly check S_OK
	if (FAILED(hr))
	{
		log(0, true, TEXT("LoadFilter(): Cannot CreateClassEnumerator, error 0x%.08X\n"), hr);
		return hr;
	}

	if (S_OK != hr) 
	{
		// Class not found
		log(0, true, TEXT("LoadFilter(): Class not found, CreateClassEnumerator returned S_FALSE\n"));
		return E_UNEXPECTED;
	}

	int counter = 0;
	// next filter
	while (pIEnumMoniker->Next(1, &pIMoniker, 0) == S_OK)
	{
		counter++;

		if(useCounter && counter != m_iTunerNumber)
		{
			pIMoniker = NULL;
			continue;
		}

		// obtain filter's friendly name
		CComPtr <IPropertyBag> pBag;

		hr = pIMoniker->BindToStorage(NULL, NULL, IID_IPropertyBag,	reinterpret_cast<void**>(&pBag));
		if (FAILED(hr))
			return hr;

		CComVariant varBSTR;
		hr = pBag->Read(L"FriendlyName", &varBSTR, NULL);
		if (FAILED(hr))
		{
			pIMoniker = NULL;
			continue;
		}

		log(0, true, TEXT("Loading filter \"%S\""), varBSTR.bstrVal);

		pBag = NULL;

		// bind the filter
		CComPtr <IBaseFilter> pFilter;

		hr = pIMoniker->BindToObject(NULL, NULL, IID_IBaseFilter, reinterpret_cast<void**>(&pFilter));

		if (FAILED(hr))
		{
			pIMoniker = NULL;
			pFilter = NULL;
			continue;
		}

		hr = m_pFilterGraph->AddFilter(pFilter, varBSTR.bstrVal);

		if (FAILED(hr))
			return hr;

		if(pConnectFilter)
		{
			if (fIsUpstream)
				hr = ConnectFilters(pConnectFilter, pFilter);
			else
				hr = ConnectFilters(pFilter, pConnectFilter);

			if (SUCCEEDED(hr))
			{
				//that's the filter we want
				fFoundFilter = TRUE;
				pFilter.QueryInterface(ppFilter);
				log(0, false, TEXT(" - succeeded!\n"));
				break;
			}
			else
			{
				fFoundFilter = FALSE;
				// that wasn't the the filter we wanted
				// so unload and try the next one
				hr = m_pFilterGraph->RemoveFilter(pFilter);
				log(0, false, TEXT(" - doesn't fit, unloading!\n"));
				if (FAILED(hr))
					return hr;
			}
		}
		else
		{
			fFoundFilter = TRUE;
			pFilter.QueryInterface(ppFilter);
			break;
		}

		pIMoniker = NULL;
		pFilter = NULL;

	} // while

	pIEnumMoniker = NULL;

	return S_OK;
}

// Removes each filter from the graph and un-registers
// the filter.
HRESULT CBDAFilterGraph::TearDownGraph()
{
	HRESULT hr = S_OK;
	CComPtr <IBaseFilter> pFilter;
	CComPtr <IEnumFilters> pIFilterEnum;

	if(m_fGraphBuilt || m_fGraphFailure)
	{
		// unload manually added filters
		if(m_pTIF)
		{
			m_pFilterGraph->RemoveFilter(m_pTIF);
			//m_pGuideData.Release();
			m_pTIF = NULL;
		}
		if(m_pDemux)
		{
			m_pFilterGraph->RemoveFilter(m_pDemux);
			m_pDemux = NULL;
		}
		if(m_pNetworkProvider)
		{
			m_pFilterGraph->RemoveFilter(m_pNetworkProvider);
			m_pNetworkProvider = NULL;
		}

		if(m_pTunerDemodDevice)
		{
			m_pFilterGraph->RemoveFilter(m_pTunerDemodDevice);
			m_pTunerDemodDevice = NULL;
		}

		if(m_pITuner)
			m_pITuner = NULL;

		if(m_KsTunerPropSet)
			m_KsTunerPropSet = NULL;

		if(m_KsDemodPropSet)
			m_KsDemodPropSet = NULL;

		if(m_pCaptureDevice)
		{
			m_pFilterGraph->RemoveFilter(m_pCaptureDevice);
			m_pCaptureDevice = NULL;
		}

		if(m_pDVBFilter)
		{
			m_pFilterGraph->RemoveFilter(m_pDVBFilter);
			m_pDVBFilter = NULL;
		}

		// now go unload rendered filters
		hr = m_pFilterGraph->EnumFilters(&pIFilterEnum);

		if(FAILED(hr))
		{
			log(0, true, TEXT("TearDownGraph: cannot EnumFilters, error 0x%.08X\n"), hr);
			return E_FAIL;
		}

		pIFilterEnum->Reset();

		while(pIFilterEnum->Next(1, &pFilter, 0) == S_OK) // addrefs filter
		{
			hr = m_pFilterGraph->RemoveFilter(pFilter);

			if (FAILED (hr))
				return hr;

			pIFilterEnum->Reset();
			pFilter = NULL;
		}
	}

	m_fGraphBuilt = FALSE;
	return S_OK;
}


// ConnectFilters is called from BuildGraph
// to enumerate and connect pins
HRESULT CBDAFilterGraph::ConnectFilters(IBaseFilter* pFilterUpstream, 
										IBaseFilter* pFilterDownstream)
{
	HRESULT         hr = E_FAIL;

	CComPtr <IPin>  pIPinUpstream;

	PIN_INFO        PinInfoUpstream;
	PIN_INFO        PinInfoDownstream;

	// grab upstream filter's enumerator
	CComPtr <IEnumPins> pIEnumPinsUpstream;
	hr = pFilterUpstream->EnumPins(&pIEnumPinsUpstream);

	if(FAILED(hr))
	{
		log(0, true, TEXT("Cannot Enumerate Upstream Filter's Pins, error 0x%.08X\n"), hr);
		return hr;
	}

	// iterate through upstream filter's pins
	while (pIEnumPinsUpstream->Next (1, &pIPinUpstream, 0) == S_OK)
	{
		hr = pIPinUpstream->QueryPinInfo (&PinInfoUpstream);
		if(FAILED(hr))
		{
			log(0, true, TEXT("Cannot Obtain Upstream Filter's PIN_INFO, error 0x%.08X\n"), hr);
			return hr;
		}

		CComPtr <IPin> pPinDown;
		pIPinUpstream->ConnectedTo (&pPinDown);

		// bail if pins are connected
		// otherwise check direction and connect
		if ((PINDIR_OUTPUT == PinInfoUpstream.dir) && (pPinDown == NULL))
		{
			// grab downstream filter's enumerator
			CComPtr <IEnumPins> pIEnumPinsDownstream;
			hr = pFilterDownstream->EnumPins (&pIEnumPinsDownstream);
			if(FAILED(hr))
			{
				log(0, true, TEXT("Cannot enumerate pins on downstream filter, error 0x%.08X\n"), hr);
				return hr;
			}

			// iterate through downstream filter's pins
			CComPtr <IPin>  pIPinDownstream;
			while (pIEnumPinsDownstream->Next (1, &pIPinDownstream, 0) == S_OK)
			{
				// make sure it is an input pin
				hr = pIPinDownstream->QueryPinInfo(&PinInfoDownstream);
				if(SUCCEEDED(hr))
				{
					CComPtr <IPin>  pPinUp;

					// Determine if the pin is already connected.  Note that 
					// VFW_E_NOT_CONNECTED is expected if the pin isn't yet connected.
					hr = pIPinDownstream->ConnectedTo (&pPinUp);
					if(FAILED(hr) && hr != VFW_E_NOT_CONNECTED)
					{
						log(0, true, TEXT("Failed in pIPinDownstream->ConnectedTo(), error 0x%.08X\n"), hr);
						continue;
					}

					if ((PINDIR_INPUT == PinInfoDownstream.dir) && (pPinUp == NULL))
					{
						if (SUCCEEDED(m_pFilterGraph->Connect(pIPinUpstream, pIPinDownstream)))
						{
							PinInfoDownstream.pFilter->Release();
							PinInfoUpstream.pFilter->Release();
							return S_OK;
						}
					}
				}

				PinInfoDownstream.pFilter->Release();
				pIPinDownstream = NULL;

			} // while next downstream filter pin

			//We are now back into the upstream pin loop
		} // if output pin

		pIPinUpstream = NULL;
		PinInfoUpstream.pFilter->Release();

	} // while next upstream filter pin

	return E_FAIL;
}


// RunGraph checks to see if a graph has been built
// if not it calls BuildGraph
// RunGraph then calls MediaCtrl-Run
HRESULT CBDAFilterGraph::RunGraph()
{
	// check to see if the graph is already running
	if(m_fGraphRunning)
		return S_OK;

	HRESULT hr = S_OK;

	if (m_pIMediaControl == NULL)
	{
		hr = m_pFilterGraph.QueryInterface (&m_pIMediaControl);
	}

	if (SUCCEEDED (hr))
	{
		DVBParser& parser = m_pDVBFilter->getParser();
		parser.lock();
		parser.resetParser(true);
		parser.unlock();

		// run the graph
		hr = m_pIMediaControl->Run();
		if(SUCCEEDED(hr))
		{
			// The graph is running now
			m_fGraphRunning = true;
			// But we can't assume yet anything regarding its TID
			m_Tid = 0;
		}
		else
		{
			// stop parts of the graph that ran
			m_pIMediaControl->Stop();
			log(0, true, TEXT("Cannot run the graph, error 0x%.08X\n"), hr);
		}
	}

	return hr;
}


// StopGraph calls MediaCtrl - Stop
HRESULT CBDAFilterGraph::StopGraph()
{
	// check to see if the graph is already stopped
	if(m_fGraphRunning == false)
		return S_OK;

	if(m_pIMediaControl == NULL)
	{
		log(2, true, TEXT("Graph already deleted\n"));
		return S_OK;
	}

	HRESULT hr = S_OK;

	// pause before stopping
	hr = m_pIMediaControl->Pause();

	// stop the graph
	hr = m_pIMediaControl->Stop();

	// Set the tunning flag according to the result of the stop operation
	m_fGraphRunning = (FAILED (hr))?true:false;

	// Reset the TID
	m_Tid = 0;

	if(!m_fGraphRunning)
		log(2, true, TEXT("Graph successfully stopped\n"));
	else
		log(0, true, TEXT("Cannot stop the graph, error 0x%.08X\n"), hr);

	return hr;
}

bool CBDAFilterGraph::GetTunerStatus(BOOLEAN *pLocked, LONG *pQuality, LONG *pStrength)
{
	bool bRet = false;
	BOOLEAN locked=false;
	LONG quality=0;
	LONG strength=0;

	CComQIPtr<IBDA_Topology> pBDATopology(m_pTunerDemodDevice);
	if (pBDATopology)
	{
		CComPtr<IUnknown> pControlNode;
		if (SUCCEEDED(pBDATopology->GetControlNode(0,1,0,&pControlNode)))
		{
			CComQIPtr <IBDA_SignalStatistics> pSignalStats (pControlNode);
			if (pSignalStats && SUCCEEDED(pSignalStats->get_SignalLocked(&locked)) &&
								SUCCEEDED(pSignalStats->get_SignalQuality(&quality)) &&
								SUCCEEDED(pSignalStats->get_SignalStrength(&strength)))
				bRet = true;
		}
	}

	*pLocked = locked;
	*pQuality = quality;
	*pStrength = strength;

	return bRet;
}

BOOL CBDAFilterGraph::ChangeSetting(void)
{
	HRESULT hr = S_OK;

	if(!g_Configuration.getUseNewTuningMethod())
	{
		log(2, true, TEXT("Using tuning request-based tuning method...\n"));

		// create a tune request to initialize the network provider
		// before connecting other filters
		CComPtr <IDVBTuneRequest> pDVBSTuneRequest;
		if (FAILED(hr = CreateDVBSTuneRequest(&pDVBSTuneRequest)))
		{
			log(0, true, TEXT("Cannot create tune request, error 0x%.08X\n"), hr);
			BuildGraphError();
			return FALSE;
		}

		//submit the tune request to the network provider
		hr = m_pITuner->put_TuneRequest(pDVBSTuneRequest);
		if (FAILED(hr))
		{
			log(0, true, TEXT("Cannot submit the tune request, error 0x%.08X\n"), hr);
			BuildGraphError();
			return FALSE;
		}
	}
	else
	{
		log(2, true, TEXT("Using device control-based tuning method...\n"));

		CComQIPtr<IBDA_DeviceControl> pDeviceControl(m_pTunerDemodDevice);
		if(pDeviceControl == NULL)
		{
			log(0, true, TEXT("Failed to QI IBDA_DeviceControl on tuner demod device, tuning failed\n"));
			BuildGraphError();
			return FALSE;
		}

		CComQIPtr<IBDA_Topology> pBDATopology(m_pTunerDemodDevice);
		if(pBDATopology == NULL)
		{
			log(0, true, TEXT("Failed to QI IBDA_Topology on tuner demod device, tuning failed\n"));
			BuildGraphError();
			return FALSE;
		}

		// Get 0 control node from the tuner demod device (tuner)
		CComPtr <IUnknown> pControlNode;
		hr = pBDATopology->GetControlNode(0, 1, 0, &pControlNode);
		if(FAILED(hr))
		{
			log(0, true, TEXT("Can't obtain the tuner control node, error 0x%.08X, tuning failed\n"), hr);
			BuildGraphError();
			return FALSE;
		}

		CComQIPtr<IBDA_FrequencyFilter> pFreqControl(pControlNode);
		if(pFreqControl == NULL)
		{
			log(0, true, TEXT("Failed to QI IBDA_FrequencyFilter on the tuner node, tuning failed\n"));
			BuildGraphError();
			return FALSE;
		}

		hr = pDeviceControl->StartChanges();
		if(FAILED(hr))
		{
			log(0, true, TEXT("Start changes failed on the device control, error 0x%.08X, tuning failed\n"), hr);
			BuildGraphError();
			return FALSE;
		}
				
		hr = pFreqControl->put_Frequency(m_ulCarrierFrequency);
		if(FAILED(hr))
		{
			log(0, true, TEXT("Failed to set frequency \"%lu\" on the frequency control, error 0x%.08X, tuning failed\n"), m_ulCarrierFrequency, hr);
			BuildGraphError();
			return FALSE;
		}

		hr = pFreqControl->put_Polarity(m_SignalPolarisation);
		if(FAILED(hr))
		{
			log(0, true, TEXT("Failed to set polarization \"%s\" on the frequency control, error 0x%.08X, tuning failed\n"), printablePolarization(m_SignalPolarisation), hr);
			BuildGraphError();
			return FALSE;
		}
				
		CComQIPtr<IBDA_LNBInfo> pLNB(pControlNode);
		if(pLNB == NULL)
		{
			log(0, true, TEXT("Failed to QI IBDA_LNBInfo on the tuner node, tuning failed\n"));
			BuildGraphError();
			return FALSE;
		}
		
		hr = pLNB->put_HighLowSwitchFrequency(g_Configuration.getLNBSW());
		if(FAILED(hr))
		{
			log(0, true, TEXT("Failed to set LNB switch frequency to \"%lu\" on the LNB control, error 0x%.08X, tuning failed\n"), g_Configuration.getLNBSW(), hr);
			BuildGraphError();
			return FALSE;
		}

		hr = pLNB->put_LocalOscilatorFrequencyLowBand(g_Configuration.getLNBLOF1());
		if(FAILED(hr))
		{
			log(0, true, TEXT("Failed to set LNB low band to \"%lu\" on the LNB control, error 0x%.08X, tuning failed\n"), g_Configuration.getLNBLOF1(), hr);
			BuildGraphError();
			return FALSE;
		}

		hr = pLNB->put_LocalOscilatorFrequencyHighBand(g_Configuration.getLNBLOF2());
		if(FAILED(hr))
		{
			log(0, true, TEXT("Failed to set LNB high band to \"%lu\" on the LNB control, error 0x%.08X, tuning failed\n"), g_Configuration.getLNBLOF2(), hr);
			BuildGraphError();
			return FALSE;
		}

		pControlNode = NULL;
		hr = pBDATopology->GetControlNode(0, 1, 1, &pControlNode);
		if(FAILED(hr))
		{
			log(0, true, TEXT("Can't obtain the demod control node, error 0x%.08X, tuning failed\n"), hr);
			BuildGraphError();
			return FALSE;
		}

		CComQIPtr<IBDA_DigitalDemodulator> pDemod(pControlNode);
		if(pDemod == NULL)
		{
			log(0, true, TEXT("Failed to QI IBDA_DigitalDemodulator on the demod node, tuning failed\n"));
			BuildGraphError();
			return FALSE;
		}

		FECMethod fecMethod = BDA_FEC_VITERBI;
		hr = pDemod->put_InnerFECMethod(&fecMethod);
		if(FAILED(hr))
		{
			log(0, true, TEXT("Failed to set Inner FEC method to \"BDA_FEC_VITERBI\" on the demod, error 0x%.08X, tuning failed\n"), hr);
			BuildGraphError();
			return FALSE;
		}

		hr = pDemod->put_InnerFECRate(&m_FECRate);
		if(FAILED(hr))
		{
			log(0, true, TEXT("Failed to set Inner FEC rate to \"%s\" on the demod, error 0x%.08X, tuning failed\n"), printableFEC(m_FECRate), hr);
			BuildGraphError();
			return FALSE;
		}

		hr = pDemod->put_ModulationType(&m_Modulation);
		if(FAILED(hr))
		{
			log(0, true, TEXT("Failed to set Modulation to \"%s\" on the demod, error 0x%.08X, tuning failed\n"), printableModulation(m_Modulation), hr);
			BuildGraphError();
			return FALSE;
		}

		hr = pDemod->put_SymbolRate(&m_ulSymbolRate);
		if(FAILED(hr))
		{
			log(0, true, TEXT("Failed to set Symbol Rate to \"%lu\" on the demod, error 0x%.08X, tuning failed\n"), m_ulSymbolRate, hr);
			BuildGraphError();
			return FALSE;
		}

		hr = pDeviceControl->CommitChanges();
		if(FAILED(hr))
		{
			log(0, true, TEXT("Start changes failed on the device control, error 0x%.08X, tuning failed\n"), hr);
			BuildGraphError();
			return FALSE;
		}
	}

	return TRUE;
}

int CBDAFilterGraph::getNumberOfTuners()
{
	// Get instance of device enumberator
	CComPtr<ICreateDevEnum>	pICreateDevEnum;
	HRESULT hr = pICreateDevEnum.CoCreateInstance(CLSID_SystemDeviceEnum);
	if(FAILED(hr))
	{
		log(0, true, TEXT("getNumberOfTuners(): Cannot CoCreate ICreateDevEnum, error 0x%.08X\n"), hr);
		return 0;
	}

	// Obtain the enumerator
	CComPtr<IEnumMoniker>	pIEnumMoniker;
	hr = pICreateDevEnum->CreateClassEnumerator(KSCATEGORY_BDA_NETWORK_TUNER, &pIEnumMoniker, 0);
	
	// the call can return S_FALSE if no moniker exists, so explicitly check S_OK
	if(hr != S_OK)
	{
		log(0, true, TEXT("getNumberOfTuners(): Cannot CreateClassEnumerator, error 0x%.08X\n"), hr);
		return 0;
	}

	// Go through the filters
	CComPtr<IMoniker>		pIMoniker;
	int counter = 0;
	while(pIEnumMoniker->Next(1, &pIMoniker, 0) == S_OK)
	{
		counter++;
		pIMoniker = NULL;
	}

	// We counted all the tuners
	return counter;
}

// Get the IPin addresses to the Tuner (Input0) and the
// Demodulator (MPEG2 Transport) pins on the Tuner/Demod filter.
// Then gets the IKsPropertySet interfaces for the pins.
//
BOOL CBDAFilterGraph::GetTunerDemodPropertySetInterfaces()
{
    if (!m_pTunerDemodDevice) {
        return FALSE;
    }

	//Acer Modified
    m_pTunerPin = FindPinOnFilter(m_pTunerDemodDevice, "Antenna In Pin",TRUE);
    if (!m_pTunerPin) {
		m_pTunerPin = FindPinOnFilter(m_pTunerDemodDevice, "Input0",TRUE);
		if (!m_pTunerPin) {
			printf(TEXT("Cannot find Input0 pin on BDA Tuner/Demod filter!\n"));
			return FALSE;
		}
    }
	//Acer Modified
    m_pDemodPin = FindPinOnFilter(m_pTunerDemodDevice, "MPEG2 Transport",FALSE);
    if (!m_pDemodPin) {
        printf(TEXT("Cannot find MPEG2 Transport pin on BDA Tuner/Demod filter!\n"));
        return FALSE;
    }

    HRESULT hr = m_pTunerPin->QueryInterface(IID_IKsPropertySet,
                                   reinterpret_cast<void**>(&m_KsTunerPropSet));
    if (FAILED(hr)) {
        printf(TEXT("QI of IKsPropertySet failed, error 0x%.08X\n"), hr);
        m_pTunerPin = NULL;
		return FALSE;
	}

    hr = m_pDemodPin->QueryInterface(IID_IKsPropertySet,
                                   reinterpret_cast<void**>(&m_KsDemodPropSet));
    if (FAILED(hr)) {
        printf(TEXT("QI of IKsPropertySet failed, error 0x%.08X\n"), hr);
        m_pDemodPin = NULL;
		return FALSE;
	}

    return TRUE;
}

// Returns a pointer address of the name of a Pin on a 
// filter.
IPin* CBDAFilterGraph::FindPinOnFilter(IBaseFilter* pBaseFilter,
									   char* pPinName,
									   BOOL bCheckPinName)
{
	HRESULT hr;
	IEnumPins *pEnumPin = NULL;
	ULONG CountReceived = 0;
	IPin *pPin = NULL, *pThePin = NULL;
	char String[80];
	char* pString = NULL;
    PIN_INFO PinInfo;
	int length;

	// enumerate of pins on the filter 
	hr = pBaseFilter->EnumPins(&pEnumPin);
	if (hr == S_OK && pEnumPin)
	{
		pEnumPin->Reset();

		pThePin = NULL;
		while (pEnumPin->Next( 1, &pPin, &CountReceived) == S_OK && pPin)
		{
			memset(String, NULL, sizeof(String));

			hr = pPin->QueryPinInfo(&PinInfo);
			
			if (hr == S_OK)
			{				
				if (pPinName != NULL) 
				{
					length = (int)wcslen (PinInfo.achName) + 1;
					pString = new char [length];
			
					// get the pin name 
					WideCharToMultiByte(CP_ACP, 0, PinInfo.achName, -1, pString, length,
										NULL, NULL);
			
					strcat_s (String, sizeof(String), pString);
					
					// Compare Ref Name
					if (bCheckPinName)
					{
					   // is there a match
					   if (!strcmp(String, pPinName))
					   {					
						  delete [] pString;
						  PinInfo.pFilter->Release( );
						  pEnumPin->Release();
						  return pPin;
					   }
					}
					else
					{
						//OutputDebugString (TEXT("FindPinOnFilter: Do not compare Name Finding...\n"));

						delete [] pString;
						PinInfo.pFilter->Release( );
						pEnumPin->Release();
						return pPin;
					}
				
					pPin->Release();

					delete [] pString;					
				}
				else 
				{ 									
					//pThePin = pPin;	
					
					PinInfo.pFilter->Release( );
					pEnumPin->Release();
					return pPin;
				}
				
				PinInfo.pFilter->Release( );
			}
			else
			{				
				// need to release this pin
				pPin->Release();
			}
		}	// end if have pin

		// need to release the enumerator
		pEnumPin->Release();
	}

	// return address of pin if found on the filter
	return pThePin;
}
