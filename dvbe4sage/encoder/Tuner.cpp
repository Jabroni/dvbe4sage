#include "StdAfx.h"

#include "Tuner.h"
#include "logger.h"
#include "configuration.h"
#include "encoder.h"

int Tuner::m_TTBudget2Tuners = 0;
int Tuner::m_USB2Tuners = 0;

Tuner::Tuner(Encoder* const pEncoder,
			 UINT ordinal,
			 ULONG initialFrequency,
			 ULONG initialSymbolRate,
			 Polarisation initialPolarization,
			 ModulationType initialModulation,
			 BinaryConvolutionCodeRate initialFec) :
	m_pEncoder(pEncoder),
	m_BDAFilterGraph(ordinal),
	m_WorkerThread(NULL),
	m_IsTunerOK(true),
	m_InitializationEvent(NULL),
	m_LockStatus(false)
{
	// Tune to the initial parameters
	tune(initialFrequency, initialSymbolRate, initialPolarization, initialModulation, initialFec);

	// Build the graph
	if(FAILED(m_BDAFilterGraph.BuildGraph()))
	{
		log(0, true, ordinal, TEXT("Error: Could not Build the DVB-S BDA filter graph\n"));
		m_IsTunerOK = false;
	}

	// Get driver and device info and print it - works only for Twinhan cards
	DEVICE_INFO DevInfo;
	memset(&DevInfo, 0, sizeof(DEVICE_INFO));
	DriverInfo  DrvInfo;
	memset(&DrvInfo, 0, sizeof(DriverInfo));
	if(m_BDAFilterGraph.THBDA_IOCTL_GET_DEVICE_INFO_Fun(&DevInfo) && m_BDAFilterGraph.THBDA_IOCTL_GET_DRIVER_INFO_Fun(&DrvInfo))
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
		m_BDAFilterGraph.THBDA_IOCTL_SET_LNB_DATA_Fun(g_pConfiguration->getLNBLOF1(), g_pConfiguration->getLNBLOF2(), g_pConfiguration->getLNBSW());
	}
	// If our device if one of the TechnoTrend kind
	if(m_BDAFilterGraph.m_IsTTBDG2 || m_BDAFilterGraph.m_IsTTUSB2)
	{
		// Determine whether it's budget or USB 2.0
		DEVICE_CAT deviceCategory = m_BDAFilterGraph.m_IsTTBDG2 ? BUDGET_2 : USB_2;
		// Get device handle from TT BDA API
		HANDLE hTT = bdaapiOpen(deviceCategory, m_BDAFilterGraph.m_IsTTBDG2 ? m_TTBudget2Tuners++ : m_USB2Tuners++);
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
						m_BDAFilterGraph.m_IsTTBDG2 ? TEXT("TechnoTrend Budget") : TEXT("TechnoTrend USB 2.0"));
		}
	}
}

Tuner::~Tuner(void)
{
	// Wait for the running idle thread to return (in most cases should return immediately
	WaitForSingleObject(m_WorkerThread, INFINITE);

	// Close working thread handle
	CloseHandle(m_WorkerThread);

	// Stop the graph if it's running
	if(m_BDAFilterGraph.m_fGraphRunning)
		m_BDAFilterGraph.StopGraph();

	// Destory the graph
	m_BDAFilterGraph.TearDownGraph();
}

void Tuner::tune(ULONG frequency,
				 ULONG symbolRate,
				 Polarisation polarization,
				 ModulationType modulation,
				 BinaryConvolutionCodeRate fecRate)
{
	// Set the parameters
	m_BDAFilterGraph.m_ulCarrierFrequency = frequency;
	m_BDAFilterGraph.m_ulSymbolRate = symbolRate;
	m_BDAFilterGraph.m_SignalPolarisation = polarization;
	m_BDAFilterGraph.m_Modulation = modulation;
	m_BDAFilterGraph.m_FECRate = fecRate;

	// Fix the modulation type for S2 tuning of Hauppauge devices
	if(!m_BDAFilterGraph.m_IsHauppauge && !m_BDAFilterGraph.m_IsFireDTV && (modulation == BDA_MOD_8PSK || modulation == BDA_MOD_NBC_QPSK))
		m_BDAFilterGraph.m_Modulation = BDA_MOD_8VSB;

	m_LockStatus = false;
}

bool Tuner::startRecording(bool startFullTransponderDump)
{
	// Build the graph
	m_BDAFilterGraph.BuildGraph();

	// Perform the tuning
	m_BDAFilterGraph.ChangeSetting();

	// Get lock status for the first time
	//getLockStatus();
	if(startFullTransponderDump)
		m_BDAFilterGraph.getParser().startTransponderDump();

	// Run the graph
	if(FAILED(m_BDAFilterGraph.RunGraph()))
	{
		log(0, true, m_BDAFilterGraph.getTunerOrdinal(), TEXT("Error: Could not play the filter graph.\n"));
		return false;
	}
	else
		return true;
}

bool Tuner::getLockStatus()
{
	// Get the lock status from the tuner only if not locked yet
	if(!m_LockStatus)
	{
		// Get lock status
		BOOLEAN bLocked = FALSE;
		LONG lSignalQuality = 0;		
		LONG lSignalStrength = 0;

		// Get the tuner status
		m_BDAFilterGraph.GetTunerStatus(&bLocked, &lSignalQuality, &lSignalStrength);

		// If lock succeeded, print the log message and set the flag
		if(bLocked)
		{
			log(0, true, m_BDAFilterGraph.getTunerOrdinal(), TEXT("Signal locked, quality=%d, strength=%d\n"), lSignalQuality, lSignalStrength);
			m_LockStatus = true;
		}
	}

	return m_LockStatus;
}

void Tuner::stopRecording()
{
	// Stop the full transponder dump
	m_BDAFilterGraph.getParser().stopTransponderDump();

	// Tell the parser to stop processing PSI packets
	m_BDAFilterGraph.getParser().stopPSIPackets();

	// The tuner stops the running graph
	m_BDAFilterGraph.StopGraph();

	// Destory the graph
	m_BDAFilterGraph.TearDownGraph();
}

void Tuner::copyProviderDataAndStopRecording()
{
	// Get the encoder network provider
	NetworkProvider& encoderProvider = m_pEncoder->getNetworkProvider();

	// Get the tuner parser
	DVBParser* const pTunerParser = getParser();

	// If the tuner parser is valid
	if(pTunerParser != NULL && pTunerParser->getTimeStamp() != 0)
	{
		// Get the tuner network provider
		const NetworkProvider& tunerNetworkProvider = pTunerParser->getNetworkProvider();

		// Lock the encoder and the tuner parsers
		encoderProvider.lock();
		pTunerParser->lock();

		// Copy the contents of the encoders parser from the current tuner's parser
		encoderProvider.copy(tunerNetworkProvider);

		// Unlock both parsers
		pTunerParser->unlock();
		encoderProvider.unlock();
	}

	// Tell the tuner to stop the recording
	stopRecording();
}

DWORD WINAPI RunIdleCallback(LPVOID vpTuner)
{
	// Sleep for the initial running time
	Sleep(g_pConfiguration->getInitialRunningTime() * 1000);

	// Get the tuner
	Tuner* pTuner = (Tuner*)vpTuner;

	// Copy provider data and stop recording, for the first time
	pTuner->copyProviderDataAndStopRecording();

	// Get current transponder TID
	USHORT currentTid = pTuner->getParser() != NULL ? pTuner->getParser()->getCurrentTid() : 0;

	// Get the list of transponders from the encoder network providers
	const hash_map<USHORT, Transponder> transponders = pTuner->m_pEncoder->getNetworkProvider().getTransponders();

	// If asked for, loop through all the transponders
	if(g_pConfiguration->scanAllTransponders())
		for(hash_map<USHORT, Transponder>::const_iterator it = transponders.begin(); it != transponders.end(); it++)
			// For any transponder other than the initial one
			if(it->first != currentTid)
			{
				// Tune to its parameters
				pTuner->tune(it->second.frequency, it->second.symbolRate, it->second.polarization, it->second.modulation, it->second.fec);
				// Start the recording
				pTuner->startRecording(false);
				// Sleep for the same time as the initial running time
				Sleep(g_pConfiguration->getInitialRunningTime() * 1000);
				// And, finally, copy the provider data and stop the recording
				pTuner->copyProviderDataAndStopRecording();
			}

	// Now we can set the initialization event, as the initialization of the tuner is fully complete
	SetEvent(pTuner->m_InitializationEvent);

	return 0;
}

void Tuner::runIdle()
{
	// Create the initialization event that need to be waited on to be sure the initialization is complete
	m_InitializationEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	// Create the worker thread for idle run
	m_WorkerThread = CreateThread(NULL, 0, RunIdleCallback, this, 0, NULL);
}
