#include "StdAfx.h"

#include "Tuner.h"
#include "logger.h"
#include "configuration.h"
#include "encoder.h"

Tuner::Tuner(Encoder* const pEncoder,
			 int ordinal,
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
		log(0, true, TEXT("Error: Could not Build the DVB-S BDA filter graph\n"));
		m_IsTunerOK = false;
	}

	// Get driver and device info and print it
	DEVICE_INFO DevInfo;
	memset(&DevInfo, 0, sizeof(DEVICE_INFO));
	DriverInfo  DrvInfo;
	memset(&DrvInfo, 0, sizeof(DriverInfo));
	if(m_BDAFilterGraph.THBDA_IOCTL_GET_DEVICE_INFO_Fun(&DevInfo) && m_BDAFilterGraph.THBDA_IOCTL_GET_DRIVER_INFO_Fun(&DrvInfo))
	{
		log(0, true, TEXT("Device Info: Device_Name=%s, Device_TYPE=%d, MAC=%02X-%02X-%02X-%02X-%02X-%02X\n"),
						DevInfo.Device_Name,
						DevInfo.Device_TYPE, 
						(UINT)DevInfo.MAC_ADDRESS[0],
						(UINT)DevInfo.MAC_ADDRESS[1],
						(UINT)DevInfo.MAC_ADDRESS[2],
						(UINT)DevInfo.MAC_ADDRESS[3],
						(UINT)DevInfo.MAC_ADDRESS[4],
						(UINT)DevInfo.MAC_ADDRESS[5]);
		log(0, true, TEXT("Driver Info: Company=%s, Version=%d.%d\n"),
						DrvInfo.Company,
						DrvInfo.Version_Major,
						DrvInfo.Version_Minor);

		// Set the LNB data
		m_BDAFilterGraph.THBDA_IOCTL_SET_LNB_DATA_Fun(g_Configuration.getLNBLOF1(), g_Configuration.getLNBLOF2(), g_Configuration.getLNBSW());
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

bool Tuner::startRecording(bool ignoreSignalLock)
{
	// Build the graph
	m_BDAFilterGraph.BuildGraph();

	// Perform the tuning
	m_BDAFilterGraph.ChangeSetting();

	// Get lock status for the first time
	//getLockStatus();

	// Run the graph
	if(FAILED(m_BDAFilterGraph.RunGraph()))
	{
		log(0, true, TEXT("Error: Could not play the filter graph.\n"));
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
			log(0, true, TEXT("Signal locked, quality=%d, strength=%d\n"), lSignalQuality, lSignalStrength);
			m_LockStatus = true;
		}
	}

	return m_LockStatus;
}

void Tuner::stopRecording()
{
	// Tell the parser to stop processing PSI packets
	m_BDAFilterGraph.getParser().stopPSIPackets();

	// The tuner stops the running graph
	m_BDAFilterGraph.StopGraph();

	// Destory the graph
	m_BDAFilterGraph.TearDownGraph();
}

DWORD WINAPI RunIdleCallback(LPVOID vpTuner)
{
	// Sleep for the initial running time
	Sleep(g_Configuration.getInitialRunningTime() * 1000);

	// Get the tuner
	Tuner* pTuner = (Tuner*)vpTuner;

	// Get the encoder parser
	DVBParser* const pEncoderParser = pTuner->m_pEncoder->getParser();

	// Get the tuner parser
	DVBParser* const pTunerParser = pTuner->getParser();

	// If both are valid, copy the info
	if(pEncoderParser != NULL && pTunerParser != NULL && pTunerParser->getTimeStamp() != 0)
	{
		// Lock the encoder and the tuner parsers
		pEncoderParser->lock();
		pTunerParser->lock();

		// Copy the contents of the encoders parser from the current tuner's parser
		pEncoderParser->copy(*pTunerParser);

		// Unlock both parsers
		pTunerParser->unlock();
		pEncoderParser->unlock();
	}

	// Tell the tuner to stop the recording
	pTuner->stopRecording();

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
