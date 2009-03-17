#include "StdAfx.h"
#include "Tuner.h"
#include "logger.h"
#include "configuration.h"

Tuner::Tuner(Encoder* const pEncoder,
			 int ordinal,
			 ULONG initialFrequency,
			 ULONG initialSymbolRate,
			 Polarisation initialPolarization,
			 ModulationType initialModulation,
			 BinaryConvolutionCodeRate initialFec) :
	m_pEncoder(pEncoder),
	m_BDAFilterGraph(ordinal),
	m_isTwinhan(false),
	m_isMantis(false),
	m_WorkerThread(NULL),
	m_IsTunerOK(true)
{
	// Build the graph
	if(FAILED(m_BDAFilterGraph.BuildGraph()))
	{
		g_Logger.log(0, true, TEXT("Error: Could not Build the DVB-S BDA filter graph.\n"));
		m_IsTunerOK = false;
	}

	// Get driver and device info and print it
	DEVICE_INFO DevInfo;
	memset(&DevInfo, 0, sizeof(DEVICE_INFO));
	DriverInfo  DrvInfo;
	memset(&DrvInfo, 0, sizeof(DriverInfo));
	if(m_BDAFilterGraph.THBDA_IOCTL_GET_DEVICE_INFO_Fun(&DevInfo) && m_BDAFilterGraph.THBDA_IOCTL_GET_DRIVER_INFO_Fun(&DrvInfo))
	{
		g_Logger.log(0, true, TEXT("Device Info: Device_Name=%s, Device_TYPE=%d, MAC=%02X-%02X-%02X-%02X-%02X-%02X\n"),
						DevInfo.Device_Name,
						DevInfo.Device_TYPE, 
						(UINT)DevInfo.MAC_ADDRESS[0],
						(UINT)DevInfo.MAC_ADDRESS[1],
						(UINT)DevInfo.MAC_ADDRESS[2],
						(UINT)DevInfo.MAC_ADDRESS[3],
						(UINT)DevInfo.MAC_ADDRESS[4],
						(UINT)DevInfo.MAC_ADDRESS[5]);
		g_Logger.log(0, true, TEXT("Driver Info: Company=%s, Version=%d.%d\n"),
						DrvInfo.Company,
						DrvInfo.Version_Major,
						DrvInfo.Version_Minor);
		if(_tcsstr(m_BDAFilterGraph.getTunerName(), TEXT("878")) != NULL)
			m_isTwinhan = true;
		if(_tcsstr(m_BDAFilterGraph.getTunerName(), TEXT("Mantis")) != NULL)
			m_isMantis = true;
	}

	// Tune to the initial parameters
	tune(initialFrequency, initialSymbolRate, initialPolarization, initialModulation, initialFec);
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
	if(m_BDAFilterGraph.m_IsHauppauge && modulation == BDA_MOD_8VSB)
		m_BDAFilterGraph.m_Modulation = BDA_MOD_8PSK;

	// Set LNB power
	m_BDAFilterGraph.THBDA_IOCTL_SET_TUNER_POWER_Fun(TRUE);
}

bool Tuner::startRecording()
{
	if(m_isMantis)
		m_BDAFilterGraph.BuildGraph();

	// Perform the tuning
	m_BDAFilterGraph.ChangeSetting();

	// Get lock status
	BOOLEAN bLocked = FALSE;
	LONG lSignalQuality = 0;		
	LONG lSignalStrength = 0;

	// Get the tuner status
	m_BDAFilterGraph.GetTunerStatus(&bLocked, &lSignalQuality, &lSignalStrength);
	if(bLocked)
		g_Logger.log(0, true, TEXT("Signal locked, quality=%d, strength=%d\n"), lSignalQuality, lSignalStrength);
	else
	{
		if(m_isTwinhan)
			g_Logger.log(0, true, TEXT("Signal not locked, trying anyway...!\n"));
		else
		{
			g_Logger.log(0, true, TEXT("Signal not locked, no recording done...!\n"));
			return false;
		}
	}
		
	// Run the graph
	if(FAILED(m_BDAFilterGraph.RunGraph()))
	{
		g_Logger.log(0, true, TEXT("Error: Could not play the filter graph.\n"));
		return false;
	}
	else
		return true;
}

void Tuner::stopRecording()
{
	// Tell the parser to stop processing PSI packets
	m_BDAFilterGraph.getParser().stopPSIPackets();

	// The tuner stops the running graph
	m_BDAFilterGraph.StopGraph();

	// Destory the graph (only for Mantis)
	if(m_isMantis)
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

	return 0;
}

void Tuner::runIdle()
{
	// Create the worker thread for idle run
	m_WorkerThread = CreateThread(NULL, 0, RunIdleCallback, this, 0, NULL);
}
