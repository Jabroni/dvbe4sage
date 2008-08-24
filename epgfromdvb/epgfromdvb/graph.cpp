//------------------------------------------------------------------------------
// File: Graph.cpp
//
// Desc: Sample code for BDA graph building.
//
//
// Copyright (c) 2003, Conexant System, Inc. All rights reserved.
//------------------------------------------------------------------------------
#include "stdafx.h"
#include "graph.h"
#include "DVBFilter.h"
#include <stdlib.h>
#include <tchar.h>

// 
// NOTE: In this sample, text strings are hard-coded for 
// simplicity and for readability.  For product code, you should
// use string tables and LoadString().
//

//
// An application can advertise the existence of its filter graph
// by registering the graph with a global Running Object Table (ROT).
// The GraphEdit application can detect and remotely view the running
// filter graph, allowing you to 'spy' on the graph with GraphEdit.
//
// To enable registration in this sample, define REGISTER_FILTERGRAPH.
//
//#define REGISTER_FILTERGRAPH

// Constructor, initializes member variables
// and calls InitializeGraphBuilder
CBDAFilterGraph::CBDAFilterGraph() :
	m_fGraphBuilt(FALSE),
	m_fGraphRunning(FALSE),
	m_ulCarrierFrequency(10806000),      
	m_ulSymbolRate(27500),
	m_SignalPolarisation(0),
	m_KsTunerPropSet(NULL),
	m_KsDemodPropSet(NULL),
	m_pTunerPin(NULL),
	m_pDemodPin(NULL),
	m_dwGraphRegister(0),
	m_pTIF(0),
	m_pDVBFilterToParserProxy(NULL),
	m_bLoadCaptureDevice(TRUE),
	m_iTunerNumber(1)
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
	{
		TearDownGraph();
	}
}

void
CBDAFilterGraph::ReleaseInterfaces()
{
	if (m_pITuningSpace)
	{
		m_pITuningSpace = NULL;
	}

	if (m_pTuningSpaceContainer)
	{
		m_pTuningSpaceContainer = NULL;
	}

	if (m_pITuner)
	{
		m_pITuner = NULL;
	}

	if (m_pFilterGraph)
	{
		m_pFilterGraph = NULL;
	}

	if (m_pIMediaControl)
	{
		m_pIMediaControl = NULL;
	}

	if (m_pTHParserPin)
	{
		m_pTHParserPin = NULL;
	}

	if (m_pVideoPin)
	{
		m_pVideoPin = NULL;
	}

	if (m_pAudioPin)
	{
		m_pAudioPin = NULL;
	}

	if (m_pITHPsiParser)
	{
		m_pITHPsiParser = NULL;
	}

	if ( m_KsDemodPropSet )
	{
		m_KsDemodPropSet = NULL;
	}

	if ( m_KsTunerPropSet )
	{
		m_KsTunerPropSet = NULL;
	}

	if ( m_pTunerPin )
	{
		m_pTunerPin = NULL;
	}

	if ( m_pDemodPin )
	{
		m_pDemodPin = NULL;
	}
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
		printf(TEXT("Couldn't CoCreate IGraphBuilder\n"));
		m_fGraphFailure = true;
		return hr;
	}

	return hr;
}

// BuildGraph sets up devices, adds and connects filters
HRESULT CBDAFilterGraph::BuildGraph(CComBSTR pNetworkType)
{
	HRESULT hr = S_OK;

	// if we have already have a filter graph, tear it down
	if (m_fGraphBuilt)
	{
		if(m_fGraphRunning)
		{
			hr = StopGraph();
		}

		hr = TearDownGraph();
	}

	// STEP 1: load network provider first so that it can configure other
	// filters, such as configuring the demux to sprout output pins.
	// We also need to submit a tune request to the Network Provider so it will
	// tune to a channel
	if (FAILED(hr = LoadNetworkProvider(pNetworkType)))
	{
		printf(TEXT("Cannot load network provider\n"));
		BuildGraphError();
		return hr;
	}

	hr = m_pNetworkProvider->QueryInterface(__uuidof (ITuner), reinterpret_cast <void**> (&m_pITuner));
	if (FAILED(hr)) {
		printf(TEXT("pNetworkProvider->QI: Can't QI for ITuner.\n"));
		BuildGraphError();
		return hr;
	}

	// create a tune request to initialize the network provider
	// before connecting other filters
	CComPtr <IDVBTuneRequest> pDVBSTuneRequest;
	if (FAILED(hr = CreateDVBSTuneRequest(&pDVBSTuneRequest)))
	{
		printf(TEXT("Cannot create tune request\n"));
		BuildGraphError();
		return hr;
	}

	//submit the tune request to the network provider
	hr = m_pITuner->put_TuneRequest(pDVBSTuneRequest);
	if (FAILED(hr)) {
		printf(TEXT("Cannot submit the tune request\n"));
		BuildGraphError();
		return hr;
	}

	// STEP2: Load tuner device and connect to network provider
	if (FAILED(hr = LoadFilter(KSCATEGORY_BDA_NETWORK_TUNER, &m_pTunerDemodDevice, m_pNetworkProvider, TRUE, TRUE)) || !m_pTunerDemodDevice)
	{
		printf(TEXT("Cannot load tuner device and connect network provider\n"));
		BuildGraphError();
		return hr;
	}

	// Let's see whether this is a Starbox or a PCI card
	FILTER_INFO filterInfo;
	if(SUCCEEDED(hr = m_pTunerDemodDevice->QueryFilterInfo(&filterInfo)))
		printf("Tuner Filter Info = \"%S\"\n", filterInfo.achName);

	// We don't load capture device only for TWINHAN StarBox
	m_bLoadCaptureDevice = CComBSTR(L"DTV-DVB UDST7020BDA DVB-S Receiver") != filterInfo.achName;

	if(m_bLoadCaptureDevice)
		// Step3: Load capture device and connect to tuner/demod device
		if (FAILED(hr = LoadFilter(KSCATEGORY_BDA_RECEIVER_COMPONENT, &m_pCaptureDevice, m_pTunerDemodDevice, TRUE, FALSE)))
			m_bLoadCaptureDevice = FALSE;

	// Step4: Load demux
	if (FAILED(hr = LoadDemux()))
	{
		printf(TEXT("Cannot load demux\n"));
		BuildGraphError();
		return hr;
	}

	// Step5: Render demux pins
	if (FAILED(hr = RenderDemux()))
	{
		printf(TEXT("Cannot Rend demux\n"));
		BuildGraphError();
		return hr;
	}

	// Step6: get pointer to Tuner and Demod pins on Tuner/Demod filter
	// for setting the tuner and demod. property sets
	if (GetTunerDemodPropertySetInterfaces() == FALSE)
	{
		printf(TEXT("Getting tuner/demod pin pointers failed\n"));
		BuildGraphError();
		return hr;
	}

	m_fGraphBuilt = true;
	m_fGraphFailure = false;

	return S_OK;
}


// Get the IPin addresses to the Tuner (Input0) and the
// Demodulator (MPEG2 Transport) pins on the Tuner/Demod filter.
// Then gets the IKsPropertySet interfaces for the pins.
//
BOOL CBDAFilterGraph::GetTunerDemodPropertySetInterfaces()
{
	if (!m_pTunerDemodDevice)
		return FALSE;

	//Acer Modified
	m_pTunerPin = FindPinOnFilter(m_pTunerDemodDevice, "Antenna In Pin",TRUE);
	if (!m_pTunerPin)
	{
		m_pTunerPin = FindPinOnFilter(m_pTunerDemodDevice, "Input0",TRUE);
		if (!m_pTunerPin)
		{
			printf(TEXT("Cannot find Input0 pin on BDA Tuner/Demod filter!\n"));
			return FALSE;
		}
	}

	//Acer Modified
	m_pDemodPin = FindPinOnFilter(m_pTunerDemodDevice, "MPEG2 Transport",FALSE);
	if (!m_pDemodPin)
	{
		printf(TEXT("Cannot find MPEG2 Transport pin on BDA Tuner/Demod filter!\n"));
		return FALSE;
	}

	HRESULT hr = m_pTunerPin->QueryInterface(IID_IKsPropertySet, reinterpret_cast<void**>(&m_KsTunerPropSet));
	if (FAILED(hr))
	{
		printf(TEXT("QI of IKsPropertySet failed\n"));
		m_pTunerPin = NULL;
		return FALSE;
	}

	hr = m_pDemodPin->QueryInterface(IID_IKsPropertySet, reinterpret_cast<void**>(&m_KsDemodPropSet));
	if (FAILED(hr))
	{
		printf(TEXT("QI of IKsPropertySet failed\n"));
		m_pDemodPin = NULL;
		return FALSE;
	}

	return TRUE;
}

// Makes call to tear down the graph and sets graph failure
// flag to true
VOID CBDAFilterGraph::BuildGraphError()
{
	TearDownGraph();
	m_fGraphFailure = true;
}


// This creates a new DVBS Tuning Space entry in the registry.
HRESULT CBDAFilterGraph::CreateTuningSpace(CComBSTR pNetworkType)
{
	HRESULT hr = S_OK;

	CComPtr <IDVBSTuningSpace> pIDVBTuningSpace;
	hr = pIDVBTuningSpace.CoCreateInstance(CLSID_DVBSTuningSpace);
	if (FAILED(hr) || !pIDVBTuningSpace)
	{
		printf(TEXT("Failed to create system tuning space."));
		return FALSE;
	}

	// set the Frequency mapping
	WCHAR szFreqMapping[ 3 ]=L"-1";
	BSTR bstrFreqMapping = SysAllocString(szFreqMapping);
	if (bstrFreqMapping) {
		hr = pIDVBTuningSpace->put_FrequencyMapping(bstrFreqMapping); // not used
		SysFreeString(bstrFreqMapping);
	}

	// set the Friendly Name of the network type, shown as Name in the registry
	hr = pIDVBTuningSpace->put_UniqueName(pNetworkType);
	if (FAILED(hr))
		return hr;

	hr = pIDVBTuningSpace->put_SystemType(DVB_Satellite);
	if (FAILED(hr))
		return hr;

	hr = pIDVBTuningSpace->put__NetworkType(CLSID_DVBSNetworkProvider);
	if (FAILED(hr))
		return hr;

	WCHAR szFriendlyName[32]=L"EPG Grabber DVBS";
	BSTR bstrFriendlyName = SysAllocString(szFriendlyName);
	if (bstrFriendlyName)
	{
		hr = pIDVBTuningSpace->put_FriendlyName(bstrFriendlyName);
		SysFreeString(bstrFriendlyName);
	}

	// create DVBS Locator
	CComPtr <IDVBSLocator> pIDVBSLocator;
	hr = CoCreateInstance(CLSID_DVBSLocator, NULL, CLSCTX_INPROC_SERVER, IID_IDVBSLocator, reinterpret_cast<void**>(&pIDVBSLocator));
	if (FAILED(hr) || !pIDVBSLocator)
		return hr;

	// NOTE: The below parameter with the exception of
	//       setting the carrier frequency don't need to set.
	//       Thus they are set to -1.  The demodulator can
	//       handle these parameters without having to specifically
	//       set them in the hardware.

	// set the Carrier Frequency
	hr = pIDVBSLocator->put_CarrierFrequency(m_ulCarrierFrequency);
	hr = pIDVBSLocator->put_SymbolRate(m_ulSymbolRate);
	hr = pIDVBSLocator->put_SignalPolarisation(m_SignalPolarisation ? BDA_POLARISATION_LINEAR_H : BDA_POLARISATION_LINEAR_V);

	// set the Bandwidth
	//hr = pIDVBSLocator->put_Bandwidth(-1);                            // not used 

	// set the Low Priority FEC type
	//hr = pIDVBSLocator->put_LPInnerFEC(BDA_FEC_METHOD_NOT_SET);       // not used

	// set the Low Priority code rate
	//hr = pIDVBSLocator->put_LPInnerFECRate(BDA_BCC_RATE_NOT_SET);     // not used

	// set the hieriarcy alpha
	//hr = pIDVBSLocator->put_HAlpha(BDA_HALPHA_NOT_SET);               // not used 

	// set the guard interval
	//hr = pIDVBSLocator->put_Guard(BDA_GUARD_NOT_SET);                 // not used 

	// set the transmission mode/FFT
	//hr = pIDVBSLocator->put_Mode(BDA_XMIT_MODE_NOT_SET);              // not used 

	// set whether the frequency is being used by another DVB-S broadcaster
	//hr = pIDVBSLocator->put_OtherFrequencyInUse(VARIANT_BOOL(FALSE)); // not used

	// set the inner FEC type
	hr = pIDVBSLocator->put_InnerFEC(BDA_FEC_METHOD_NOT_SET);         // not used

	// set the inner code rate
	hr = pIDVBSLocator->put_InnerFECRate(BDA_BCC_RATE_NOT_SET);       // not used 

	// set the outer FEC type
	hr = pIDVBSLocator->put_OuterFEC(BDA_FEC_METHOD_NOT_SET);         // not used 

	// set the outer code rate
	hr = pIDVBSLocator->put_OuterFECRate(BDA_BCC_RATE_NOT_SET);       // not used 

	// set the modulation type
	hr = pIDVBSLocator->put_Modulation(BDA_MOD_NOT_SET);              // not used 

	// set the symbol rate
	hr = pIDVBSLocator->put_SymbolRate(-1);                           // not used

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
		OutputDebugString (TEXT("Invalid pointer\n"));
		return E_POINTER;
	}

	// Making sure we have a valid tuning space
	if (m_pITuningSpace == NULL)
	{
		printf(TEXT("Tuning Space is NULL\n"));
		return E_FAIL;
	}

	//  Create an instance of the DVBS tuning space
	CComQIPtr<IDVBSTuningSpace> pDVBSTuningSpace (m_pITuningSpace);
	if(!pDVBSTuningSpace)
	{
		printf(TEXT("Cannot QI for an IDVBSTuningSpace\n"));
		return E_FAIL;
	}

	//=====================================================================================================
	//			Added by Michael
	//=====================================================================================================

	pDVBSTuningSpace->put_LNBSwitch(11700*1000);
	pDVBSTuningSpace->put_LowOscillator(9750*1000);
	pDVBSTuningSpace->put_HighOscillator(10600*1000);

	//=====================================================================================================
	//			End of added by Michael
	//=====================================================================================================

	//  Create an empty tune request.
	CComPtr <ITuneRequest> pNewTuneRequest;
	hr = pDVBSTuningSpace->CreateTuneRequest(&pNewTuneRequest);
	if(FAILED (hr))
	{
		printf(TEXT("CreateTuneRequest: Can't create tune request.\n"));
		return hr;
	}

	//query for an IDVBTuneRequest interface pointer
	CComQIPtr<IDVBTuneRequest> pDVBSTuneRequest (pNewTuneRequest);
	if(!pDVBSTuneRequest)
	{
		printf(TEXT("CreateDVBSTuneRequest: Can't QI for IDVBTuneRequest.\n"));
		return E_FAIL;
	}

	CComPtr<IDVBSLocator> pDVBSLocator;
	hr = pDVBSLocator.CoCreateInstance (CLSID_DVBSLocator);	
	if (FAILED(hr) || !pDVBSLocator)
		return hr;

	if(FAILED(hr))
	{
		printf(TEXT("Cannot create the DVB-S locator failed\n"));
		return hr;
	}

	hr = pDVBSLocator->put_CarrierFrequency( m_ulCarrierFrequency );
	if(FAILED(hr))
	{
		printf(TEXT("put_CarrierFrequency failed\n"));
		return hr;
	}

	hr = pDVBSLocator->put_SymbolRate(m_ulSymbolRate);

	if(m_SignalPolarisation)
		hr= pDVBSLocator->put_SignalPolarisation(BDA_POLARISATION_LINEAR_H);
	else
		hr= pDVBSLocator->put_SignalPolarisation(BDA_POLARISATION_LINEAR_V);

	/*
	hr = pDVBSLocator->put_InnerFEC(BDA_FEC_METHOD_NOT_SET);
	hr = pDVBSLocator->put_InnerFECRate(BDA_BCC_RATE_2_3);
	hr = pDVBSLocator->put_OuterFEC(BDA_FEC_METHOD_NOT_SET);
	hr = pDVBSLocator->put_OuterFECRate(BDA_BCC_RATE_2_3);
	hr = pDVBSLocator->put_Modulation(BDA_MOD_8PSK);
*/
	hr = pDVBSTuneRequest->put_Locator(pDVBSLocator);
	if(FAILED (hr))
	{
		printf(TEXT("Cannot put the locator\n"));
		return hr;
	}

	hr = pDVBSTuneRequest.QueryInterface(pTuneRequest);

	return hr;
}


// LoadNetworkProvider loads network provider
HRESULT CBDAFilterGraph::LoadNetworkProvider(CComBSTR pNetworkType)
{
	HRESULT  hr = S_OK;
	CComBSTR bstrNetworkType;
	CLSID    CLSIDNetworkType;

	// obtain tuning space then load network provider
	if (m_pITuningSpace == NULL)
	{
		// so we create one.
		hr = CreateTuningSpace(pNetworkType);
		if (FAILED(hr) || !m_pITuningSpace)
		{
			printf(TEXT("Cannot create tuning space!\n"));
			return E_FAIL;
		}
	}

	// Get the current Network Type clsid
	hr = m_pITuningSpace->get_NetworkType(&bstrNetworkType);
	if (FAILED(hr))
	{
		printf(TEXT("ITuningSpace::Get Network Type failed\n"));
		return hr;
	}

	hr = CLSIDFromString(bstrNetworkType, &CLSIDNetworkType);
	if (FAILED(hr))
	{
		printf(TEXT("Couldn't get CLSIDFromString\n"));
		return hr;
	}

	// create the network provider based on the clsid obtained from the tuning space
	hr = CoCreateInstance(CLSIDNetworkType, NULL, CLSCTX_INPROC_SERVER,	IID_IBaseFilter, reinterpret_cast<void**>(&m_pNetworkProvider));
	if (FAILED(hr))
	{
		printf(TEXT("Couldn't CoCreate Network Provider\n"));
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
			printf(TEXT("LoadFilter(): Cannot CoCreate ICreateDevEnum\n"));
			return hr;
		}
	}

	// obtain the enumerator
	hr = m_pICreateDevEnum->CreateClassEnumerator(clsid, &pIEnumMoniker, 0);
	
	// the call can return S_FALSE if no moniker exists, so explicitly check S_OK
	if (FAILED(hr))
	{
		printf(TEXT("LoadFilter(): Cannot CreateClassEnumerator\n"));
		return hr;
	}

	if (S_OK != hr) 
	{
		// Class not found
		printf(TEXT("LoadFilter(): Class not found, CreateClassEnumerator returned S_FALSE\n"));
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

		printf("Loading filter \"%S\"", varBSTR.bstrVal);

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
				printf(" - succeeded!\n");
				break;
			}
			else
			{
				fFoundFilter = FALSE;
				// that wasn't the the filter we wanted
				// so unload and try the next one
				hr = m_pFilterGraph->RemoveFilter(pFilter);
				printf(" - doesn't fit, unloading!\n");
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

// Loads the demux into the FilterGraph
HRESULT CBDAFilterGraph::LoadDemux()
{
	HRESULT hr = S_OK;

	hr = CoCreateInstance(CLSID_MPEG2Demultiplexer, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, reinterpret_cast<void**>(&m_pDemux));
	if (FAILED(hr))
	{
		printf(TEXT("Could not CoCreateInstance CLSID_MPEG2Demultiplexer\n"));
		return hr;
	}

	hr = m_pFilterGraph->AddFilter(m_pDemux, L"Demux");
	if (FAILED(hr))
	{
		printf(TEXT("Unable to add demux filter to graph\n"));
		return hr;
	}

	return hr;
}

// Renders demux output pins.
HRESULT CBDAFilterGraph::RenderDemux()
{
	HRESULT             hr = S_OK;
	CComPtr <IPin>      pIPin;
	CComPtr <IPin>      pDownstreamPin;
	CComPtr <IEnumPins> pIEnumPins;
	//PIN_DIRECTION       direction;

	// connect the demux to the capture device
	//Acer Marked
	if(m_bLoadCaptureDevice)
		hr = ConnectFilters(m_pCaptureDevice, m_pDemux);
	else
		hr = ConnectFilters(m_pTunerDemodDevice, m_pDemux);

	if (FAILED(hr))
	{
		printf(TEXT("Cannot connect demux to capture filter\n"));
		return hr;
	}

	CComQIPtr<IMpeg2Demultiplexer> pDemux(m_pDemux);
	CComPtr<IEnumPins> pPinEnum = NULL;

	if(SUCCEEDED(m_pDemux->EnumPins(&pPinEnum)) && pPinEnum)
	{
		IPin* pins[10];
		ULONG cRetrieved = 0;
		if(SUCCEEDED(pPinEnum->Next(sizeof(pins) / sizeof(pins[0]), pins, &cRetrieved)) && cRetrieved > 0)
		{
			for(ULONG i = 1; i < cRetrieved; i++)
			{
				PIN_INFO pinInfo;
				if(SUCCEEDED(pins[i]->QueryPinInfo(&pinInfo)))
					if(FAILED(pDemux->DeleteOutputPin(pinInfo.achName)))
						printf(TEXT("Failed to delete a demux pin\n"));
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
		ULONG pids[] = { 0 };
		if(FAILED(pMappedPin->MapPID(sizeof(pids) / sizeof(pids[0]), pids, MEDIA_MPEG2_PSI)))
			printf(TEXT("Failed to map PID in the demux\n"));
	}

	// load transform information filter and connect it to the demux
	hr = LoadFilter(KSCATEGORY_BDA_TRANSPORT_INFORMATION, &m_pTIF, m_pDemux, TRUE, FALSE);	
	if (FAILED(hr))
	{
		printf(TEXT("Cannot load TIF\n"));
		return hr;
	}

	CComPtr<IPin> pPin2 = NULL;
	if(SUCCEEDED(pDemux->CreateOutputPin(&mediaType, L"For Michael's PSI", &pPin2)) && pPin2)
	{
		CComQIPtr<IMPEG2PIDMap> pMappedPin(pPin2);
		ULONG pids[] = { 0x12, 0x11 };					// We get only STDs/BATs (0x11) and EITs (0x12) from the mux
		if(FAILED(pMappedPin->MapPID(sizeof(pids) / sizeof(pids[0]), pids, MEDIA_MPEG2_PSI)))
			printf(TEXT("Failed to map PID in the demux\n"));
	}

	CComPtr<IBaseFilter> pDVBFilterToParserProxy(m_pDVBFilterToParserProxy->GetFilter());
	if(FAILED(hr = m_pFilterGraph->AddFilter(pDVBFilterToParserProxy, L"Michael's Filter")))
	{
		printf(TEXT("Failed to load Michael's Filter\n"));
	}

	hr = ConnectFilters(m_pDemux, pDVBFilterToParserProxy);
	if (FAILED(hr))
	{
		printf(TEXT("Cannot connect filters\n"));
		return hr;
	}

	pPinEnum = NULL;
	pDemux.Release();

	pIEnumPins = NULL;

	return hr;
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
		if ( m_pIPSink )
		{
			m_pFilterGraph->RemoveFilter(m_pIPSink);
			m_pIPSink = NULL;
		}

		if ( m_pTHPsiParser )
		{
			m_pFilterGraph->RemoveFilter(m_pTHPsiParser);
			m_pTHPsiParser = NULL;
		}

		if ( m_pTIF )
		{
			m_pFilterGraph->RemoveFilter(m_pTIF);
			//m_pGuideData.Release();
			m_pTIF = NULL;
		}

		if ( m_pDemux )
		{
			m_pFilterGraph->RemoveFilter(m_pDemux);
			m_pDemux = NULL;
		}

		if ( m_pNetworkProvider )
		{
			m_pFilterGraph->RemoveFilter(m_pNetworkProvider);
			m_pNetworkProvider = NULL;
		}

		if ( m_pTunerDemodDevice )
		{
			m_pFilterGraph->RemoveFilter(m_pTunerDemodDevice);
			m_pTunerDemodDevice = NULL;
		}

		if ( m_pCaptureDevice )
		{
			m_pFilterGraph->RemoveFilter(m_pCaptureDevice);
			m_pCaptureDevice = NULL;
		}

		if( m_pDVBFilterToParserProxy )
		{
			m_pDVBFilterToParserProxy = NULL;
		}

		// now go unload rendered filters
		hr = m_pFilterGraph->EnumFilters(&pIFilterEnum);

		if(FAILED(hr))
		{
			printf(TEXT("TearDownGraph: cannot EnumFilters\n"));
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
		printf(TEXT("Cannot Enumerate Upstream Filter's Pins\n"));
		return hr;
	}

	// iterate through upstream filter's pins
	while (pIEnumPinsUpstream->Next (1, &pIPinUpstream, 0) == S_OK)
	{
		hr = pIPinUpstream->QueryPinInfo (&PinInfoUpstream);
		if(FAILED(hr))
		{
			printf(TEXT("Cannot Obtain Upstream Filter's PIN_INFO\n"));
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
				printf(TEXT("Cannot enumerate pins on downstream filter!\n"));
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
						printf(TEXT("Failed in pIPinDownstream->ConnectedTo()!\n"));
						continue;
					}

					if ((PINDIR_INPUT == PinInfoDownstream.dir) && (pPinUp == NULL))
					{
						if (SUCCEEDED (m_pFilterGraph->Connect(pIPinUpstream, pIPinDownstream)))
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
		// run the graph
		hr = m_pIMediaControl->Run();
		if(SUCCEEDED(hr))
		{
			m_fGraphRunning = true;
		}
		else
		{
			// stop parts of the graph that ran
			m_pIMediaControl->Stop();
			printf(TEXT("Cannot run graph: %X\n"), hr);
		}
	}

	return hr;
}


// StopGraph calls MediaCtrl - Stop
HRESULT
CBDAFilterGraph::StopGraph()
{
	// check to see if the graph is already stopped
	if(m_fGraphRunning == false)
		return S_OK;

	HRESULT hr = S_OK;

	// pause before stopping
	hr = m_pIMediaControl->Pause();

	// stop the graph
	hr = m_pIMediaControl->Stop();

	m_fGraphRunning = (FAILED (hr))?true:false;

	return hr;
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
	char* pString;
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
					WideCharToMultiByte(CP_ACP, 0, PinInfo.achName, -1, pString, length, NULL, NULL);

					strcat_s(String, sizeof(String), pString);

					// Compare Ref Name
					if (bCheckPinName)
					{
						// is there a match
						if (!strcmp(String, pPinName))
						{					
							delete pString;
							PinInfo.pFilter->Release( );
							pEnumPin->Release();
							return pPin;
						}
					}
					else
					{
						delete pString;
						PinInfo.pFilter->Release( );
						pEnumPin->Release();
						return pPin;
					}

					pPin->Release();

					delete pString;					
				}
				else 
				{ 									
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


BOOL CBDAFilterGraph::GetTunerStatus(BOOLEAN *pLocked, LONG *pQuality, LONG *pStrength)
{
	BOOLEAN bRet = false;
	HRESULT hr;
	BOOLEAN locked=false;
	LONG quality=0;
	LONG strength=0;

	CComQIPtr<IBDA_Topology> pBDATopology(m_pTunerDemodDevice);

	if (pBDATopology)
	{
		CComPtr <IUnknown> pControlNode;

		if (SUCCEEDED(hr=pBDATopology->GetControlNode(0,1,0,&pControlNode)))
		{
			CComQIPtr <IBDA_SignalStatistics> pSignalStats (pControlNode);

			if (pSignalStats)
			{
				if (SUCCEEDED(hr=pSignalStats->get_SignalLocked(&locked)) &&
					SUCCEEDED(hr=pSignalStats->get_SignalQuality(&quality)) &&
					SUCCEEDED(hr=pSignalStats->get_SignalStrength(&strength)))
					bRet=true;
			}
		}
	}

	*pLocked = locked;
	*pQuality = quality;
	*pStrength = strength;

	return bRet;
}


BOOL CBDAFilterGraph::SetPidInfo(VOID)
{
	return THBDA_IOCTL_SET_PID_FILTER_INFO_Fun();
}

BOOL CBDAFilterGraph::GetPidInfo(VOID)
{
	return THBDA_IOCTL_GET_PID_FILTER_INFO_Fun();
}

BOOL CBDAFilterGraph::ChangeSetting(void)
{
	HRESULT hr = S_OK;

	// create a tune request to initialize the network provider
	// before connecting other filters
	CComPtr <IDVBTuneRequest> pDVBSTuneRequest;
	if (FAILED(hr = CreateDVBSTuneRequest(&pDVBSTuneRequest)))
	{
		printf(TEXT("Cannot create tune request\n"));
		BuildGraphError();
		return hr;
	}

	//submit the tune request to the network provider
	hr = m_pITuner->put_TuneRequest(pDVBSTuneRequest);
	if (FAILED(hr)) {
		printf(TEXT("Cannot submit the tune request\n"));
		BuildGraphError();
		return hr;
	}

	return hr;
}
