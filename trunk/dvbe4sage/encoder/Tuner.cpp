#include "StdAfx.h"

#include "Tuner.h"
#include "logger.h"
#include "configuration.h"
#include "encoder.h"
#include "extern.h"
#include "GrowlHandler.h"

int DVBSTuner::m_TTBudget2Tuners = 0;
int DVBSTuner::m_USB2Tuners = 0;

DVBSTuner::DVBSTuner(Encoder* const pEncoder,
					 UINT ordinal,
					 ULONG initialFrequency,
					 ULONG initialSymbolRate,
					 Polarisation initialPolarization,
					 ModulationType initialModulation,
					 BinaryConvolutionCodeRate initialFec) :
	m_pEncoder(pEncoder),
	m_WorkerThread(NULL),
	m_InitializationEvent(NULL),
	m_LockStatus(false)
{
	// Create the right filter graph
	m_pFilterGraph = new DVBFilterGraph(ordinal);

	// Tune to the initial parameters
	tune(initialFrequency, initialSymbolRate, initialPolarization, initialModulation, initialFec);

	// Build the graph
	if(FAILED(m_pFilterGraph->BuildGraph()))
	{
		g_pGrowlHandler = new GrowlHandler;
		if(g_pConfiguration->getGrowlNotification())
			g_pGrowlHandler->SendNotificationMessage(NotificationType::NOTIFICATION_ERROR,"Cannot build BDA Graph","ERROR: Could not build BDA filter graph");
		log(0, true, ordinal, TEXT("Error: Could not Build the BDA filter graph\n"));
		m_IsSourceOK = false;
	}

	// Get driver and device info and print it - works only for Twinhan cards
	DEVICE_INFO DevInfo;
	memset(&DevInfo, 0, sizeof(DEVICE_INFO));
	DriverInfo  DrvInfo;
	memset(&DrvInfo, 0, sizeof(DriverInfo));
	if(((DVBFilterGraph*)m_pFilterGraph)->THBDA_IOCTL_GET_DEVICE_INFO_Fun(&DevInfo) && ((DVBFilterGraph*)m_pFilterGraph)->THBDA_IOCTL_GET_DRIVER_INFO_Fun(&DrvInfo))
	{
		// Let's get the MAC in a textual form - for Twinhan
		char MAC[100];
		sprintf_s(MAC, sizeof(MAC) / sizeof(MAC[0]), "%02X:%02X:%02X:%02X:%02X:%02X", 
						(UINT)DevInfo.MAC_ADDRESS[0],
						(UINT)DevInfo.MAC_ADDRESS[1],
						(UINT)DevInfo.MAC_ADDRESS[2],
						(UINT)DevInfo.MAC_ADDRESS[3],
						(UINT)DevInfo.MAC_ADDRESS[4],
						(UINT)DevInfo.MAC_ADDRESS[5]);

		m_MAC = MAC;
		log(0, true, ordinal, TEXT("Device ordinal=%d, MAC=%s,  name=%s, type=%d\n"),
						ordinal,
						(LPCTSTR)CA2T(MAC),
						(LPCTSTR)CA2T(DevInfo.Device_Name),
						DevInfo.Device_TYPE);
			
					
		log(0, true, ordinal, TEXT("Driver Info: Company=%s, Version=%d.%d\n"),
						(LPCTSTR)CA2T(DrvInfo.Company),
						DrvInfo.Version_Major,
						DrvInfo.Version_Minor);

		// Set the LNB data
		((DVBFilterGraph*)m_pFilterGraph)->THBDA_IOCTL_SET_LNB_DATA_Fun(g_pConfiguration->getLNBLOF1(), g_pConfiguration->getLNBLOF2(), g_pConfiguration->getLNBSW());
	}
	// If our device if one of the TechnoTrend kind
	if(((DVBFilterGraph*)m_pFilterGraph)->m_IsTTBDG2 || ((DVBFilterGraph*)m_pFilterGraph)->m_IsTTUSB2)
	{
		// Determine whether it's budget or USB 2.0
		DEVICE_CAT deviceCategory = ((DVBFilterGraph*)m_pFilterGraph)->m_IsTTBDG2 ? BUDGET_2 : USB_2;
		// Get device handle from TT BDA API
		HANDLE hTT = bdaapiOpen(deviceCategory, ((DVBFilterGraph*)m_pFilterGraph)->m_IsTTBDG2 ? m_TTBudget2Tuners++ : m_USB2Tuners++);
		// If the handle is not bogus
		if(hTT != INVALID_HANDLE_VALUE)
		{
			// Get the MAC address
			DWORD high, low;
			bdaapiGetMAC(hTT, &high, &low);
			// Close the handle
			bdaapiClose(hTT);

			// Let's get the MAC in a textual form - for TechnoTrend
			char MAC[100];
			sprintf_s(MAC, sizeof(MAC) / sizeof(MAC[0]), "%02X:%02X:%02X:%02X:%02X:%02X",
						(high & 0xFF0000) >> 16,
						(high & 0xFF00) >> 8,
						high & 0xFF,
						(low & 0xFF0000) >> 16,
						(low & 0xFF00) >> 8,
						low & 0xFF);

			m_MAC = MAC;

			// Print out the MAC address information
			log(0, true, ordinal, TEXT("Device ordinal=%d, MAC=%s,  type=%s\n"),
						ordinal,
						(LPCTSTR)CA2T(MAC),
						((DVBFilterGraph*)m_pFilterGraph)->m_IsTTBDG2 ? TEXT("TechnoTrend Budget") : TEXT("TechnoTrend USB 2.0"));
		}
	}
}

DVBSTuner::~DVBSTuner(void)
{
	// Wait for the running idle thread to return (in most cases should return immediately)
	WaitForSingleObject(m_WorkerThread, INFINITE);

	// Close working thread handle
	CloseHandle(m_WorkerThread);
}

void DVBSTuner::tune(ULONG frequency,
					 ULONG symbolRate,
					 Polarisation polarization,
					 ModulationType modulation,
					 BinaryConvolutionCodeRate fecRate)
{
	// Set the parameters
	((DVBFilterGraph*)m_pFilterGraph)->m_ulCarrierFrequency = frequency;
	((DVBFilterGraph*)m_pFilterGraph)->m_ulSymbolRate = symbolRate;
	((DVBFilterGraph*)m_pFilterGraph)->m_SignalPolarisation = polarization;
	((DVBFilterGraph*)m_pFilterGraph)->m_Modulation = modulation;
	((DVBFilterGraph*)m_pFilterGraph)->m_FECRate = fecRate;


	// Fix the modulation type for S2 tuning of Hauppauge devices ... skip entirely for genpix
	if(!((DVBFilterGraph*)m_pFilterGraph)->m_IsGenpix && !((DVBFilterGraph*)m_pFilterGraph)->m_IsHauppauge && !((DVBFilterGraph*)m_pFilterGraph)->m_IsFireDTV && (modulation == BDA_MOD_8PSK || modulation == BDA_MOD_NBC_QPSK))
	{
		log(3, true, m_pFilterGraph->getTunerOrdinal(), TEXT("DVBSTuner::tune is changing the modulation from %s to BDA_MOD_8VSB\n"), printableModulation(modulation));
		((DVBFilterGraph*)m_pFilterGraph)->m_Modulation = BDA_MOD_8VSB;
	}

	m_LockStatus = false;
}

bool DVBSTuner::startPlayback(USHORT onid, bool startFullTransponderDump)
{

	// Build the graph, but only if needed
	if(!m_pFilterGraph->m_fGraphBuilt)
		if(m_pFilterGraph->BuildGraph() != S_OK || !m_pFilterGraph->m_fGraphBuilt)
			return false;

	// Send diseqc/mptor command(s) if configured to do so ...
	if(g_pConfiguration->getUseDiseqc())
		((DVBFilterGraph*)m_pFilterGraph)->SendDiseqc(onid);

	// Perform the tuning
	((DVBFilterGraph*)m_pFilterGraph)->ChangeSetting();

	// Start full transponder dump if requested
	if(startFullTransponderDump && m_pFilterGraph->getParser() != NULL) 
		m_pFilterGraph->getParser()->startTransponderDump();


	// Run the graph
	HRESULT hr = S_OK;
	if(FAILED(hr = m_pFilterGraph->RunGraph()))
	{
		log(0, true, m_pFilterGraph->getTunerOrdinal(), TEXT("Error: Could not play the DVB filter graph, error 0x%.08X\n"), hr);
		return false;
	}
	else
		return true;
}

bool DVBSTuner::getLockStatus()
{
	// Get the lock status from the tuner only if not locked yet
	if(!m_LockStatus)
	{
		// Get lock status
		BOOLEAN bLocked = FALSE;
		long lSignalQuality = 0;		
		long lSignalStrength = 0;

		// Get the tuner status
		((DVBFilterGraph*)m_pFilterGraph)->GetTunerStatus(bLocked, lSignalQuality, lSignalStrength);

		// If lock succeeded, print the log message and set the flag
		if(bLocked)
		{
			log(0, true, m_pFilterGraph->getTunerOrdinal(), TEXT("Signal locked, quality=%d, strength=%d\n"), lSignalQuality, lSignalStrength);
			m_LockStatus = true;
		}
	}

	return m_LockStatus;
}

void DVBSTuner::copyProviderDataAndStopRecording()
{
	// Get the encoder network provider
	NetworkProvider& encoderProvider = m_pEncoder->getNetworkProvider();

	// Get the tuner parser
	DVBParser* const tunerParser = getParser();

	// If the tuner parser is valid
	if(tunerParser != NULL && tunerParser->getTimeStamp() != 0)
	{
		// Get the tuner network provider
		const NetworkProvider& tunerNetworkProvider = tunerParser->getNetworkProvider();

		// Lock the encoder and the tuner parsers
		encoderProvider.lock();
		tunerParser->lock();

		// Copy the contents of the encoders parser from the current tuner's parser
		encoderProvider.copy(tunerNetworkProvider);

		// Unlock both parsers
		tunerParser->unlock();
		encoderProvider.unlock();
	}

	// Tell the tuner to stop the recording
	stopPlayback();
}


DWORD WINAPI RunIdleCallback(LPVOID vpTuner)
{
	// Get the tuner
	DVBSTuner* pTuner = (DVBSTuner*)vpTuner;

	DiSEqC *pDiseqc = ((DVBFilterGraph*)pTuner->m_pFilterGraph)->getDiseqc();

	// Loop for all configured network IDs
	for(int onidIndex = 0; onidIndex < (pDiseqc != NULL ? pDiseqc->GetInitialONIDCount() : 1); onidIndex++)
	{

		bool autoDiscover = true;

		
		// We first check if we need to load the services and transponders from file, if not, continue with autodiscover
		if(g_pConfiguration->getCacheServices() && pTuner->getParser() != NULL ? pTuner->getParser()->readTransponderServicesFromFile() : 0) {
				
				autoDiscover = false;
			
				// If yes, get the encoder's network provider
				NetworkProvider& encoderNetworkProvider = pTuner->m_pEncoder->getNetworkProvider();
			
				// Lock network provider
				encoderNetworkProvider.lock();
				pTuner->getParser()->lock();
				// Copy the contents of the network provider
				encoderNetworkProvider.copy(pTuner->getParser()->getNetworkProvider());
				pTuner->getParser()->unlock();
				// And unlock the encoder provider
				encoderNetworkProvider.unlock();
				
				pTuner->getParser()->setProviderInfoHasBeenCopied();
				
				
				log(3,true,0,TEXT("Network Provider has been copied from parser to encoder\n"));
		}
		
		//if(autoDiscover) { This was removed to always to autodiscover, just it would be less time when cache is available
		// Tune to the configured transponder		
		pTuner->tune(g_pConfiguration->getInitialFrequency(onidIndex),
					 g_pConfiguration->getInitialSymbolRate(onidIndex),
					 g_pConfiguration->getInitialPolarization(onidIndex),
					 g_pConfiguration->getInitialModulation(onidIndex),
					 g_pConfiguration->getInitialFEC(onidIndex));

		if(pTuner->startPlayback((pDiseqc != NULL ? pDiseqc->GetInitialONID(onidIndex) : 0), false))
		{
			
			// Log entry
			log(0, true, pTuner->getSourceOrdinal(), TEXT("Starting initial run for autodiscovery, transponder data: Frequency=%lu, Symbol Rate=%lu, Polarization=%s, Modulation=%s, FEC=%s\n"),
				pTuner->getFrequency(), pTuner->getSymbolRate(), printablePolarization(pTuner->getPolarization()), printableModulation(pTuner->getModulation()), printableFEC(pTuner->getFECRate()));
	

			// Sleep for the initial running time
			if(autoDiscover)
				Sleep(g_pConfiguration->getInitialRunningTime() * 1000);


			// Log entry
			log(0, true, pTuner->getSourceOrdinal(), TEXT("The initial run for autodiscovery finished\n"));
			// verify

			// Log tuner lock status
			if(!pTuner->getLockStatus())
				log(0, true, pTuner->getSourceOrdinal(), TEXT("The tuner failed to acquire the signal\n"));

			// Get current transponder TID and ONID
			const USHORT currentTID = pTuner->getParser() != NULL ? pTuner->getParser()->getCurrentTID() : 0;
			const USHORT currentONID = pTuner->getParser() != NULL ? pTuner->getParser()->getCurrentONID() : 0;
			const UINT32 utid = NetworkProvider::getUniqueSID(currentONID, currentTID);

			// Copy provider data and stop recording, for the first time
			pTuner->copyProviderDataAndStopRecording();
	
			// After the initial autodiscovery, if enabled save the services and transponders to cache file
			if(g_pConfiguration->getCacheServices()) {
				TCHAR reason[MAX_ERROR_MESSAGE_SIZE];
				pTuner->m_pEncoder->dumpNetworkProvider(reason);
			}


			// Get the list of transponders from the encoder network providers
			const hash_map<UINT32, Transponder> transponders = pTuner->m_pEncoder->getNetworkProvider().getTransponders();

			// If asked for, loop through all the transponders
			if(g_pConfiguration->scanAllTransponders())
			{
				// Log entry
				log(0, true, pTuner->getSourceOrdinal(), TEXT("Full transponder scan at initialization requested, starting...\n"));

				for(hash_map<UINT32, Transponder>::const_iterator it = transponders.begin(); it != transponders.end(); it++)
				{
					// Log transponder data
					log(0, true, pTuner->getSourceOrdinal(), TEXT("Transponder with ONID=%hu, TID=%hu, Frequency=%lu, Symbol Rate=%lu, Polarization=%s, Modulation=%s, FEC=%s - %s"),
						NetworkProvider::getONIDFromUniqueSID(it->first), NetworkProvider::getSIDFromUniqueSID(it->first), it->second.frequency, it->second.symbolRate,
						printablePolarization(it->second.polarization), printableModulation(it->second.modulation), printableFEC(it->second.fec),
						it->first != utid ? TEXT("scanning started!\n") : TEXT("skipped, because it's the same transponder as the initial one!\n"));
					// For any transponder other than the initial one
					if(it->first != utid)
					{
						
						// Tune to its parameters
						pTuner->tune(it->second.frequency, it->second.symbolRate, it->second.polarization, it->second.modulation, it->second.fec);
						// Start the recording
						pTuner->startPlayback(NetworkProvider::getONIDFromUniqueSID(it->first), false);
						// Sleep for the same time as the initial running time
						Sleep(g_pConfiguration->getInitialRunningTime() * 1000);
						// Log tuner lock status
						if(!pTuner->getLockStatus())
							log(0, true, pTuner->getSourceOrdinal(), TEXT("The tuner failed to acquire the signal\n"));
						// And, finally, copy the provider data and stop the recording
						pTuner->copyProviderDataAndStopRecording();
					} //if(it
				} // for(
				// Log entry
				log(0, true, pTuner->getSourceOrdinal(), TEXT("Full transponder scan at initialization finished!\n"));
			}//if(g_pConfig
		} // if start playback
		else
		{
			log(0, true, pTuner->getSourceOrdinal(), TEXT("Autodiscovery of ONID %hu failed.\n"), onidIndex);
		} //else ifstart playback
	
		//} else { //if autodiscover
		//	log(0, true, pTuner->getSourceOrdinal(), TEXT("The initial run for autodiscovery was skipped\n"));
		//}
	

	} // for

	// Now we can set the initialization event, as the initialization of the tuner is fully complete
	SetEvent(pTuner->m_InitializationEvent);

	return 0;
}

void DVBSTuner::runIdle()
{
	// Create the initialization event that need to be waited on to be sure the initialization is complete
	m_InitializationEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	// Create the worker thread for idle run
	m_WorkerThread = CreateThread(NULL, 0, RunIdleCallback, this, 0, NULL);
}
