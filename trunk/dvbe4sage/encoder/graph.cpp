#include "stdafx.h"

#include "graph.h"
#include "DVBFilter.h"
#include "Logger.h"
#include "configuration.h"
#include "extern.h"
#include "bdaapi_Typedefs.h"

// Constructor, initializes member variables
// and calls InitializeGraphBuilder
DVBSFilterGraph::DVBSFilterGraph(UINT ordinal):
	m_Tid(0),
	m_IsHauppauge(false),
	m_IsFireDTV(false),
	m_IsTTBDG2(false),
	m_IsTTUSB2(false)
{
	m_iTunerNumber = ordinal;
}

void DVBSFilterGraph::ReleaseInterfaces()
{
	if(m_pITuningSpace)
		m_pITuningSpace = NULL;

	if(m_pITuner)
		m_pITuner = NULL;

	if(m_KsDemodPropSet)
		m_KsDemodPropSet = NULL;

	if(m_KsTunerPropSet)
		m_KsTunerPropSet = NULL;

	if(m_pTunerPin)
		m_pTunerPin = NULL;

	if(m_pDemodPin)
		m_pDemodPin = NULL;

	GenericFilterGraph::ReleaseInterfaces();
}

// BuildGraph sets up devices, adds and connects filters
HRESULT DVBSFilterGraph::BuildGraph()
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
	if(FAILED(hr = LoadNetworkProvider()))
	{
		log(0, true, m_iTunerNumber, TEXT("Cannot load network provider, error 0x%.08X\n"), hr);
		BuildGraphError();
		return hr;
	}

	hr = m_pNetworkProvider->QueryInterface(__uuidof (ITuner), reinterpret_cast <void**> (&m_pITuner));
	if(FAILED(hr)) {
		log(0, true, m_iTunerNumber, TEXT("pNetworkProvider->QI: Can't QI for ITuner, error 0x%.08X\n"), hr);
		BuildGraphError();
		return hr;
	}

	// create a tune request to initialize the network provider
	// before connecting other filters
	CComPtr <IDVBTuneRequest> pDVBSTuneRequest;
	if(FAILED(hr = CreateDVBSTuneRequest(&pDVBSTuneRequest)))
	{
		log(0, true, m_iTunerNumber, TEXT("Cannot create tune request, error 0x%.08X\n"), hr);
		BuildGraphError();
		return hr;
	}

	//submit the tune request to the network provider
	hr = m_pITuner->put_TuneRequest(pDVBSTuneRequest);
	if(FAILED(hr)) {
		log(0, true, m_iTunerNumber, TEXT("Cannot submit the tune request, error 0x%.08X\n"), hr);
		BuildGraphError();
		return hr;
	}

	// STEP2: Load tuner device and connect to network provider
	if(FAILED(hr = LoadFilter(KSCATEGORY_BDA_NETWORK_TUNER, &m_pTunerDemodDevice, m_pNetworkProvider, TRUE, TRUE)) || !m_pTunerDemodDevice)
	{
		log(0, true, m_iTunerNumber, TEXT("Cannot load tuner device and connect network provider, error 0x%.08X\n"), hr);
		BuildGraphError();
		return hr != S_OK ? hr : E_FAIL;
	}

	// Get tuner friendly name
	FILTER_INFO filterInfo;
	if(SUCCEEDED(hr = m_pTunerDemodDevice->QueryFilterInfo(&filterInfo)))
	{
		CW2T filterName(filterInfo.achName);
		log(0, true, m_iTunerNumber, TEXT("Tuner Filter Info = \"%s\"\n"), (LPCTSTR)filterName);
		_tcscpy_s(m_TunerName, sizeof(m_TunerName) / sizeof(TCHAR), (LPCTSTR)filterName);

		// Let's see if this is a Hauppauge device
		if(_tcsstr(m_TunerName, TEXT("Hauppauge")) != NULL)
			m_IsHauppauge = true;

		// Let's see if this is a FireDTV device
		if(_tcsstr(m_TunerName, TEXT("FireDTV")) != NULL)
			m_IsFireDTV = true;

		// Let's see if this is a TT Budget 2 device
		if(_tcsstr(m_TunerName, BDG2_NAME_S_TUNER) != NULL)
			m_IsTTBDG2 = true;

		// Let's see if this is a TT USB 2.0 device
		if(_tcsstr(m_TunerName, USB2BDA_DVB_NAME_S_TUNER ) != NULL)
			m_IsTTUSB2 = true;
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
	m_pDVBFilter = new DVBFilter(m_iTunerNumber);

	// Step5: Finally, add our filter to the graph
	if(FAILED(hr = m_pFilterGraph->AddFilter(m_pDVBFilter, L"DVB Capture Filter")))
	{
		log(0, true, m_iTunerNumber, TEXT("Cannot add our filter to the graph, error 0x%.08X\n"), hr);
		BuildGraphError();
		return hr;
	}

	// And connect it
	if(FAILED(hr = ConnectFilters(m_pCaptureDevice, m_pDVBFilter)))
	{
		log(0, true, m_iTunerNumber, TEXT("Cannot connect our filter to the graph, error 0x%.08X\n"), hr);
		BuildGraphError();
		return hr;
	}

	log(0, true, m_iTunerNumber, TEXT("Loaded our transport stream filter\n"));

	// Step5: Load demux
	if(FAILED(hr = LoadDemux()))
	{
		log(0, true, m_iTunerNumber, TEXT("Cannot load demux, error 0x%.08X\n"), hr);
		BuildGraphError();
		return hr;
	}

	// Step6: Render demux pins
	if(FAILED(hr = RenderDemux()))
	{
		log(0, true, m_iTunerNumber, TEXT("Cannot render demux, error 0x%.08X\n"), hr);
		BuildGraphError();
		return hr;
	}

	m_fGraphBuilt = true;
	m_fGraphFailure = false;

	return S_OK;
}

// This creates a new DVBS Tuning Space entry in the registry.
HRESULT DVBSFilterGraph::CreateTuningSpace()
{
	// We're optimistic in the beginning
	HRESULT hr = S_OK;

	// Create an empty DVB-S tuning space
	hr = m_pITuningSpace.CoCreateInstance(__uuidof(DVBSTuningSpace));
	if (FAILED(hr) || m_pITuningSpace == NULL)
	{
		log(0, true, m_iTunerNumber, TEXT("CreateTuningSpace : failed to create a DVB-S system tuning space, error 0x%.08X\n"), hr);
		return hr;
	}

	// Get QI from the tuning space
	CComQIPtr<IDVBSTuningSpace> pIDVBTuningSpace(m_pITuningSpace);
	if(pIDVBTuningSpace == NULL)
	{
		log(0, true, m_iTunerNumber, TEXT("CreateTuningSpace : Cannot QI for IDVBSTuningSpace\n"));
		return E_FAIL;
	}

	hr = pIDVBTuningSpace->put_SystemType(DVB_Satellite);
	if(FAILED(hr))
	{
		log(0, true, m_iTunerNumber, TEXT("CreateTuningSpace: failed to set system type DVB_Satellite, error 0x%.08X\n"), hr);
		return hr;
	}

	hr = pIDVBTuningSpace->put__NetworkType(CLSID_DVBSNetworkProvider);
	if(FAILED(hr))
	{
		log(0, true, m_iTunerNumber, TEXT("CreateTuningSpace : failed to set network type CLSID_DVBSNetworkProvider, error 0x%.08X\n"), hr);
		return hr;
	}

	// Create a DVB-S Locator
	CComPtr<IDVBSLocator> pIDVBSLocator;
	hr = pIDVBSLocator.CoCreateInstance(__uuidof(DVBSLocator));
	if(FAILED(hr) || !pIDVBSLocator)
	{
		log(0, true, m_iTunerNumber, TEXT("CreateTuningSpace : failed to create a DVB-S locator, error 0x%.08X\n"), hr);
		return hr;
	}

	// Set the initial Tuner parameters
	hr = pIDVBSLocator->put_CarrierFrequency(m_ulCarrierFrequency);
	hr = pIDVBSLocator->put_SymbolRate(m_ulSymbolRate);
	hr = pIDVBSLocator->put_SignalPolarisation(m_SignalPolarisation);
	hr = pIDVBSLocator->put_Modulation(m_Modulation);
	hr = pIDVBSLocator->put_InnerFEC(BDA_FEC_VITERBI);
	hr = pIDVBSLocator->put_InnerFECRate(m_FECRate);

	// Set this a default locator
	hr = pIDVBTuningSpace->put_DefaultLocator(pIDVBSLocator);
	if(FAILED(hr))
	{
		log(0, true, m_iTunerNumber, TEXT("CreateTuningSpace : failed to set the locator to the tuning space, error 0x%.08X\n"), hr);
		return hr;
	}

	return hr;
}

// Creates an DVBT Tune Request
HRESULT DVBSFilterGraph::CreateDVBSTuneRequest(IDVBTuneRequest** pTuneRequest)
{
	HRESULT hr = S_OK;

	if (pTuneRequest == NULL)
	{
		log(0, true, m_iTunerNumber, TEXT("CreateDVBSTuneRequest : invalid pointer\n"));
		return E_POINTER;
	}

	// Making sure we have a valid tuning space
	if (m_pITuningSpace == NULL)
	{
		log(0, true, m_iTunerNumber, TEXT("CreateDVBSTuneRequest : the tuning Space is NULL\n"));
		return E_FAIL;
	}

	//  Create an instance of the DVBS tuning space
	CComQIPtr<IDVBSTuningSpace> pDVBSTuningSpace (m_pITuningSpace);
	if(!pDVBSTuningSpace)
	{
		log(0, true, m_iTunerNumber, TEXT("CreateDVBSTuneRequest : cannot QI for an IDVBSTuningSpace\n"));
		return E_FAIL;
	}

	//=====================================================================================================
	//			Added by Michael
	//=====================================================================================================

	pDVBSTuningSpace->put_LNBSwitch(g_pConfiguration->getLNBSW());
	pDVBSTuningSpace->put_LowOscillator(g_pConfiguration->getLNBLOF1());
	pDVBSTuningSpace->put_HighOscillator(g_pConfiguration->getLNBLOF2());
	pDVBSTuningSpace->put_SpectralInversion(BDA_SPECTRAL_INVERSION_AUTOMATIC);

	//=====================================================================================================
	//			End of added by Michael
	//=====================================================================================================

	//  Create an empty tune request.
	CComPtr<ITuneRequest> pNewTuneRequest;
	hr = pDVBSTuningSpace->CreateTuneRequest(&pNewTuneRequest);
	if(FAILED (hr))
	{
		log(0, true, m_iTunerNumber, TEXT("CreateTuneRequest : cannot create a tuninig request, error 0x%.08X\n"), hr);
		return hr;
	}

	//query for an IDVBTuneRequest interface pointer
	CComQIPtr<IDVBTuneRequest> pDVBSTuneRequest(pNewTuneRequest);
	if(!pDVBSTuneRequest)
	{
		log(0, true, m_iTunerNumber, TEXT("CreateDVBSTuneRequest: cannot QI for IDVBTuneRequest, error 0x%.08X\n"), hr);
		return E_FAIL;
	}

	CComPtr<IDVBSLocator> pDVBSLocator;
	hr = pDVBSLocator.CoCreateInstance(__uuidof(DVBSLocator));	
	if (FAILED(hr) || !pDVBSLocator)
		return hr;

	if(FAILED(hr))
	{
		log(0, true, m_iTunerNumber, TEXT("CreateDVBSTuneRequest : cannot create a DVB-S locator, error 0x%.08X\n"), hr);
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
		log(0, true, m_iTunerNumber, TEXT("CreateDVBSTuneRequest : cannot put the locator to the tuning space, error 0x%.08X\n"), hr);
		return hr;
	}

	hr = pDVBSTuneRequest.QueryInterface(pTuneRequest);

	return hr;
}


// LoadNetworkProvider loads network provider
HRESULT DVBSFilterGraph::LoadNetworkProvider()
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
			log(0, true, m_iTunerNumber, TEXT("LoadNetworkProvider : cannot create tuning space, error 0x%.08X\n"), hr);
			return E_FAIL;
		}
	}

	// Get the current Network Type clsid
	hr = m_pITuningSpace->get__NetworkType(&CLSIDNetworkType);
	if (FAILED(hr))
	{
		log(0, true, m_iTunerNumber, TEXT("LoadNetworkProvider : ITuningSpace::get__NetworkType failed, error 0x%.08X\n"), hr);
		return hr;
	}

	// create the network provider based on the clsid obtained from the tuning space
	hr = m_pNetworkProvider.CoCreateInstance(CLSIDNetworkType);
	if (FAILED(hr))
	{
		log(0, true, m_iTunerNumber, TEXT("LoadNetworkProvider : cannot CoCreate Network Provider, error 0x%.08X\n"), hr);
		return hr;
	}

	//add the Network Provider filter to the graph
	hr = m_pFilterGraph->AddFilter(m_pNetworkProvider, L"Network Provider");

	return hr;
}

// Removes each filter from the graph and un-registers the filter.
HRESULT DVBSFilterGraph::TearDownGraph()
{
	if(m_fGraphBuilt || m_fGraphFailure)
	{
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
	}
	return GenericFilterGraph::TearDownGraph();
}

bool DVBSFilterGraph::GetTunerStatus(BOOLEAN *pLocked, LONG *pQuality, LONG *pStrength)
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

BOOL DVBSFilterGraph::ChangeSetting(void)
{
	HRESULT hr = S_OK;

	if(!g_pConfiguration->getUseNewTuningMethod())
	{
		log(2, true, m_iTunerNumber, TEXT("Using tuning request-based tuning method...\n"));

		// create a tune request to initialize the network provider
		// before connecting other filters
		CComPtr <IDVBTuneRequest> pDVBSTuneRequest;
		if (FAILED(hr = CreateDVBSTuneRequest(&pDVBSTuneRequest)))
		{
			log(0, true, m_iTunerNumber, TEXT("Cannot create tune request, error 0x%.08X\n"), hr);
			BuildGraphError();
			return FALSE;
		}

		//submit the tune request to the network provider
		hr = m_pITuner->put_TuneRequest(pDVBSTuneRequest);
		if (FAILED(hr))
		{
			log(0, true, m_iTunerNumber, TEXT("Cannot submit the tune request, error 0x%.08X\n"), hr);
			BuildGraphError();
			return FALSE;
		}
	}
	else
	{
		log(2, true, m_iTunerNumber, TEXT("Using device control-based tuning method...\n"));

		CComQIPtr<IBDA_DeviceControl> pDeviceControl(m_pTunerDemodDevice);
		if(pDeviceControl == NULL)
		{
			log(0, true, m_iTunerNumber, TEXT("Failed to QI IBDA_DeviceControl on tuner demod device, tuning failed\n"));
			BuildGraphError();
			return FALSE;
		}

		CComQIPtr<IBDA_Topology> pBDATopology(m_pTunerDemodDevice);
		if(pBDATopology == NULL)
		{
			log(0, true, m_iTunerNumber, TEXT("Failed to QI IBDA_Topology on tuner demod device, tuning failed\n"));
			BuildGraphError();
			return FALSE;
		}

		// Get 0 control node from the tuner demod device (tuner)
		CComPtr <IUnknown> pControlNode;
		hr = pBDATopology->GetControlNode(0, 1, 0, &pControlNode);
		if(FAILED(hr))
		{
			log(0, true, m_iTunerNumber, TEXT("Can't obtain the tuner control node, error 0x%.08X, tuning failed\n"), hr);
			BuildGraphError();
			return FALSE;
		}

		CComQIPtr<IBDA_FrequencyFilter> pFreqControl(pControlNode);
		if(pFreqControl == NULL)
		{
			log(0, true, m_iTunerNumber, TEXT("Failed to QI IBDA_FrequencyFilter on the tuner node, tuning failed\n"));
			BuildGraphError();
			return FALSE;
		}

		hr = pDeviceControl->StartChanges();
		if(FAILED(hr))
		{
			log(0, true, m_iTunerNumber, TEXT("Start changes failed on the device control, error 0x%.08X, tuning failed\n"), hr);
			BuildGraphError();
			return FALSE;
		}
				
		hr = pFreqControl->put_Frequency(m_ulCarrierFrequency);
		if(FAILED(hr))
		{
			log(0, true, m_iTunerNumber, TEXT("Failed to set frequency \"%lu\" on the frequency control, error 0x%.08X, tuning failed\n"), m_ulCarrierFrequency, hr);
			BuildGraphError();
			return FALSE;
		}

		hr = pFreqControl->put_Polarity(m_SignalPolarisation);
		if(FAILED(hr))
		{
			log(0, true, m_iTunerNumber, TEXT("Failed to set polarization \"%s\" on the frequency control, error 0x%.08X, tuning failed\n"), printablePolarization(m_SignalPolarisation), hr);
			BuildGraphError();
			return FALSE;
		}
				
		CComQIPtr<IBDA_LNBInfo> pLNB(pControlNode);
		if(pLNB == NULL)
		{
			log(0, true, m_iTunerNumber, TEXT("Failed to QI IBDA_LNBInfo on the tuner node, tuning failed\n"));
			BuildGraphError();
			return FALSE;
		}
		
		hr = pLNB->put_HighLowSwitchFrequency(g_pConfiguration->getLNBSW());
		if(FAILED(hr))
		{
			log(0, true, m_iTunerNumber, TEXT("Failed to set LNB switch frequency to \"%lu\" on the LNB control, error 0x%.08X, tuning failed\n"), g_pConfiguration->getLNBSW(), hr);
			BuildGraphError();
			return FALSE;
		}

		hr = pLNB->put_LocalOscilatorFrequencyLowBand(g_pConfiguration->getLNBLOF1());
		if(FAILED(hr))
		{
			log(0, true, m_iTunerNumber, TEXT("Failed to set LNB low band to \"%lu\" on the LNB control, error 0x%.08X, tuning failed\n"), g_pConfiguration->getLNBLOF1(), hr);
			BuildGraphError();
			return FALSE;
		}

		hr = pLNB->put_LocalOscilatorFrequencyHighBand(g_pConfiguration->getLNBLOF2());
		if(FAILED(hr))
		{
			log(0, true, m_iTunerNumber, TEXT("Failed to set LNB high band to \"%lu\" on the LNB control, error 0x%.08X, tuning failed\n"), g_pConfiguration->getLNBLOF2(), hr);
			BuildGraphError();
			return FALSE;
		}

		pControlNode = NULL;
		hr = pBDATopology->GetControlNode(0, 1, 1, &pControlNode);
		if(FAILED(hr))
		{
			log(0, true, m_iTunerNumber, TEXT("Can't obtain the demod control node, error 0x%.08X, tuning failed\n"), hr);
			BuildGraphError();
			return FALSE;
		}

		CComQIPtr<IBDA_DigitalDemodulator> pDemod(pControlNode);
		if(pDemod == NULL)
		{
			log(0, true, m_iTunerNumber, TEXT("Failed to QI IBDA_DigitalDemodulator on the demod node, tuning failed\n"));
			BuildGraphError();
			return FALSE;
		}

		FECMethod fecMethod = BDA_FEC_VITERBI;
		hr = pDemod->put_InnerFECMethod(&fecMethod);
		if(FAILED(hr))
		{
			log(0, true, m_iTunerNumber, TEXT("Failed to set Inner FEC method to \"BDA_FEC_VITERBI\" on the demod, error 0x%.08X, tuning failed\n"), hr);
			BuildGraphError();
			return FALSE;
		}

		hr = pDemod->put_InnerFECRate(&m_FECRate);
		if(FAILED(hr))
		{
			log(0, true, m_iTunerNumber, TEXT("Failed to set Inner FEC rate to \"%s\" on the demod, error 0x%.08X, tuning failed\n"), printableFEC(m_FECRate), hr);
			BuildGraphError();
			return FALSE;
		}

		hr = pDemod->put_ModulationType(&m_Modulation);
		if(FAILED(hr))
		{
			log(0, true, m_iTunerNumber, TEXT("Failed to set Modulation to \"%s\" on the demod, error 0x%.08X, tuning failed\n"), printableModulation(m_Modulation), hr);
			BuildGraphError();
			return FALSE;
		}

		hr = pDemod->put_SymbolRate(&m_ulSymbolRate);
		if(FAILED(hr))
		{
			log(0, true, m_iTunerNumber, TEXT("Failed to set Symbol Rate to \"%lu\" on the demod, error 0x%.08X, tuning failed\n"), m_ulSymbolRate, hr);
			BuildGraphError();
			return FALSE;
		}

		hr = pDeviceControl->CommitChanges();
		if(FAILED(hr))
		{
			log(0, true, m_iTunerNumber, TEXT("Start changes failed on the device control, error 0x%.08X, tuning failed\n"), hr);
			BuildGraphError();
			return FALSE;
		}
	}

	return TRUE;
}

int DVBSFilterGraph::getNumberOfTuners()
{
	// Get instance of device enumberator
	CComPtr<ICreateDevEnum>	pICreateDevEnum;
	HRESULT hr = pICreateDevEnum.CoCreateInstance(CLSID_SystemDeviceEnum);
	if(FAILED(hr))
	{
		log(0, true, 0, TEXT("getNumberOfTuners(): Cannot CoCreate ICreateDevEnum, error 0x%.08X\n"), hr);
		return 0;
	}

	// Obtain the enumerator
	CComPtr<IEnumMoniker>	pIEnumMoniker;
	hr = pICreateDevEnum->CreateClassEnumerator(KSCATEGORY_BDA_NETWORK_TUNER, &pIEnumMoniker, 0);
	
	// the call can return S_FALSE if no moniker exists, so explicitly check S_OK
	if(hr != S_OK)
	{
		log(0, true, 0, TEXT("getNumberOfTuners(): Cannot CreateClassEnumerator, error 0x%.08X\n"), hr);
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
BOOL DVBSFilterGraph::GetTunerDemodPropertySetInterfaces()
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

HRESULT DVBSFilterGraph::RunGraph()
{
	// Reset the TID
	m_Tid = 0;
	return GenericFilterGraph::RunGraph();
}

HRESULT DVBSFilterGraph::StopGraph()
{
	// Reset the TID
	m_Tid = 0;
	return GenericFilterGraph::StopGraph();
}
