#include "StdAfx.h"
#include "GenericFilterGraph.h"
#include "Logger.h"

GenericFilterGraph::GenericFilterGraph() :	
	m_iTunerNumber(0),
	m_pDVBFilter(NULL),
	m_fGraphBuilt(false),
	m_fGraphRunning(false),
	m_fGraphFailure(false)
{
	if(FAILED(InitializeGraphBuilder()))
		m_fGraphFailure = true;
	else
		m_fGraphFailure = false;
}

GenericFilterGraph::~GenericFilterGraph()
{
	StopGraph();

	if(m_fGraphBuilt || m_fGraphFailure)
		TearDownGraph();
	ReleaseInterfaces();
}

// ConnectFilters is called from BuildGraph
// to enumerate and connect pins
HRESULT GenericFilterGraph::ConnectFilters(IBaseFilter* pFilterUpstream,
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
		log(0, true, m_iTunerNumber, TEXT("Cannot Enumerate Upstream Filter's Pins, error 0x%.08X\n"), hr);
		return hr;
	}

	// iterate through upstream filter's pins
	while (pIEnumPinsUpstream->Next (1, &pIPinUpstream, 0) == S_OK)
	{
		hr = pIPinUpstream->QueryPinInfo (&PinInfoUpstream);
		if(FAILED(hr))
		{
			log(0, true, m_iTunerNumber, TEXT("Cannot Obtain Upstream Filter's PIN_INFO, error 0x%.08X\n"), hr);
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
				log(0, true, m_iTunerNumber, TEXT("Cannot enumerate pins on downstream filter, error 0x%.08X\n"), hr);
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
						log(0, true, m_iTunerNumber, TEXT("Failed in pIPinDownstream->ConnectedTo(), error 0x%.08X\n"), hr);
						continue;
					}

					if ((PINDIR_INPUT == PinInfoDownstream.dir) && (pPinUp == NULL))
					{
						if (SUCCEEDED(hr = m_pFilterGraph->ConnectDirect(pIPinUpstream, pIPinDownstream, NULL)))
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

// enumerates through registered filters
// instantiates the the filter object and adds it to the graph
// it checks to see if it connects to upstream filter
// if not,  on to the next enumerated filter
// used for tuner, capture, MPE Data Filters and decoders that
// could have more than one filter object
// if pUpstreamFilter is NULL don't bother connecting
HRESULT GenericFilterGraph::LoadFilter(REFCLSID clsid,
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
			log(0, true, m_iTunerNumber, TEXT("LoadFilter : cannot CoCreate ICreateDevEnum, error 0x%.08X\n"), hr);
			return hr;
		}
	}

	// obtain the enumerator
	hr = m_pICreateDevEnum->CreateClassEnumerator(clsid, &pIEnumMoniker, 0);
	
	// the call can return S_FALSE if no moniker exists, so explicitly check S_OK
	if (FAILED(hr))
	{
		log(0, true, m_iTunerNumber, TEXT("LoadFilter : cannot CreateClassEnumerator, error 0x%.08X\n"), hr);
		return hr;
	}

	if (S_OK != hr) 
	{
		// Class not found
		log(0, true, m_iTunerNumber, TEXT("LoadFilter : class not found, CreateClassEnumerator returned S_FALSE\n"));
		return E_UNEXPECTED;
	}

	UINT counter = 0;
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
			log(0, true, m_iTunerNumber, TEXT("Failed to friendly name for the filter - skipping!\n"));
			pIMoniker = NULL;
			continue;
		}

		pBag = NULL;

		// bind the filter
		CComPtr <IBaseFilter> pFilter;

		hr = pIMoniker->BindToObject(NULL, NULL, IID_IBaseFilter, reinterpret_cast<void**>(&pFilter));

		if (FAILED(hr))
		{
			log(0, true, m_iTunerNumber, TEXT("Failed to get IID_IBaseFilter pointer for the filter \"%S\" to the graph - skipping!\n"), varBSTR.bstrVal);
			pIMoniker = NULL;
			pFilter = NULL;
			continue;
		}

		hr = m_pFilterGraph->AddFilter(pFilter, varBSTR.bstrVal);

		if (FAILED(hr))
		{
			log(0, true, m_iTunerNumber, TEXT("Failed to add filter \"%S\" to the graph - skipping!\n"), varBSTR.bstrVal);
			continue;
		}

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
				log(0, true, m_iTunerNumber, TEXT("Loading filter \"%S\" - succeeded!\n"), varBSTR.bstrVal);
				break;
			}
			else
			{
				fFoundFilter = FALSE;
				// that wasn't the the filter we wanted
				// so unload and try the next one
				hr = m_pFilterGraph->RemoveFilter(pFilter);
				log(3, true, m_iTunerNumber, TEXT("Loading filter \"%S\" - doesn't fit, unloading!\n"), varBSTR.bstrVal);
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

// Instantiate graph object for filter graph building
HRESULT GenericFilterGraph::InitializeGraphBuilder()
{
	HRESULT hr = S_OK;

	// we have a graph already
	if (m_pFilterGraph)
		return S_OK;

	// create the filter graph
	if (FAILED(hr = m_pFilterGraph.CoCreateInstance(CLSID_FilterGraph)))
	{
		log(0, true, m_iTunerNumber, TEXT("Couldn't CoCreate IGraphBuilder, error 0x%.08X\n"), hr);
		m_fGraphFailure = true;
		return hr;
	}

	return hr;
}

// Loads the demux into the FilterGraph
HRESULT GenericFilterGraph::LoadDemux()
{
	HRESULT hr = S_OK;

	hr = m_pDemux.CoCreateInstance(CLSID_MPEG2Demultiplexer);
	if (FAILED(hr))
	{
		log(0, true, m_iTunerNumber, TEXT("Cannot CoCreateInstance CLSID_MPEG2Demultiplexer, error 0x%.08X\n"), hr);
		return hr;
	}

	hr = m_pFilterGraph->AddFilter(m_pDemux, L"Demux");
	if (FAILED(hr))
	{
		log(0, true, m_iTunerNumber, TEXT("Unable to add demux filter to graph, error 0x%.08X\n"), hr);
		return hr;
	}

	log(0, true, m_iTunerNumber, TEXT("Added demux filter to the graph\n"));

	return hr;
}

// Renders demux output pins.
HRESULT GenericFilterGraph::RenderDemux()
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
		log(0, true, m_iTunerNumber, TEXT("Cannot connect demux to capture filter, error 0x%.08X\n"), hr);
		return hr;
	}

	log(0, true, m_iTunerNumber, TEXT("Connected demux to our filter output pin\n"));

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
						log(0, true, m_iTunerNumber, TEXT("Failed to delete a demux pin \"%S\"\n"), pinInfo.achName);
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
			log(0, true, m_iTunerNumber, TEXT("Failed to map PID in the demux, error 0x%.08X\n"), hr);
	}

	// load transform information filter and connect it to the demux
	hr = LoadFilter(KSCATEGORY_BDA_TRANSPORT_INFORMATION, &m_pTIF, m_pDemux, TRUE, FALSE);	
	if (FAILED(hr))
	{
		log(0, true, m_iTunerNumber, TEXT("Cannot load TIF, error 0x%.08X\n"), hr);
		return hr;
	}

	pPinEnum = NULL;
	pDemux.Release();

	pIEnumPins = NULL;
	return hr;
}

// RunGraph checks to see if a graph has been built
// if not it calls BuildGraph
// RunGraph then calls MediaCtrl-Run
HRESULT GenericFilterGraph::RunGraph()
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
		}
		else
		{
			// stop parts of the graph that ran
			m_pIMediaControl->Stop();
			log(0, true, m_iTunerNumber, TEXT("Cannot run the graph, error 0x%.08X\n"), hr);
		}
	}

	return hr;
}


// StopGraph calls MediaCtrl - Stop
HRESULT GenericFilterGraph::StopGraph()
{
	// check to see if the graph is already stopped
	if(m_fGraphRunning == false)
		return S_OK;

	if(m_pIMediaControl == NULL)
	{
		log(2, true, m_iTunerNumber, TEXT("Graph already deleted\n"));
		return S_OK;
	}

	HRESULT hr = S_OK;

	// pause before stopping
	hr = m_pIMediaControl->Pause();

	// stop the graph
	hr = m_pIMediaControl->Stop();

	// Set the tunning flag according to the result of the stop operation
	m_fGraphRunning = (FAILED (hr))?true:false;

	if(!m_fGraphRunning)
		log(2, true, m_iTunerNumber, TEXT("Graph successfully stopped\n"));
	else
		log(0, true, m_iTunerNumber, TEXT("Cannot stop the graph, error 0x%.08X\n"), hr);

	return hr;
}

// Returns a pointer address of the name of a Pin on a filter.
IPin* GenericFilterGraph::FindPinOnFilter(IBaseFilter* pBaseFilter,
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

// Makes call to tear down the graph and sets graph failure flag to true
void GenericFilterGraph::BuildGraphError()
{
	TearDownGraph();
	m_fGraphFailure = true;
}

void GenericFilterGraph::ReleaseInterfaces()
{
	if(m_pFilterGraph)
		m_pFilterGraph = NULL;

	if(m_pIMediaControl)
		m_pIMediaControl = NULL;

	if(m_pICreateDevEnum)
		m_pICreateDevEnum = NULL;
}

HRESULT GenericFilterGraph::TearDownGraph()
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
		
		if(m_pDVBFilter)
		{
			m_pFilterGraph->RemoveFilter(m_pDVBFilter);
			m_pDVBFilter = NULL;
		}

		// now go unload rendered filters
		hr = m_pFilterGraph->EnumFilters(&pIFilterEnum);

		if(FAILED(hr))
		{
			log(0, true, m_iTunerNumber, TEXT("TearDownGraph: cannot EnumFilters, error 0x%.08X\n"), hr);
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

	m_fGraphBuilt = false;
	return S_OK;
}
