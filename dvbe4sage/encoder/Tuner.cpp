#include "StdAfx.h"
#include "Tuner.h"
#include "logger.h"
#include "configuration.h"

Tuner::Tuner(int ordinal,
			 ULONG initialFrequency,
			 ULONG initialSymbolRate,
			 Polarisation initialPolarization,
			 ModulationType initialModulation,
			 BinaryConvolutionCodeRate initialFec) :
	m_BDAFilterGraph(ordinal, initialFrequency, initialSymbolRate, initialPolarization, initialModulation, initialFec),
	m_isTwinhan(false),
	m_AutodiscoverTransponder(false),
	m_IdleTimer(NULL),
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
	}

	// Create the timer queue
	m_TimerQueue = CreateTimerQueue();

	// Set LNB data for Twinhan
	/*if(!m_BDAFilterGraph.THBDA_IOCTL_SET_LNB_DATA_Fun())
		g_Logger.log(2, true, TEXT("THBDA_IOCTL_SET_LNB_DATA_Fun failed\n"));*/
}

Tuner::~Tuner(void)
{
	// Delete the timer queue
	DeleteTimerQueue(m_TimerQueue);

	if(m_BDAFilterGraph.m_fGraphRunning)
		m_BDAFilterGraph.StopGraph();
	m_BDAFilterGraph.TearDownGraph();
}

bool Tuner::tune(ULONG frequency,
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

	return true;
}

bool Tuner::startRecording(bool bTunerUsed,
						   bool autodiscoverTransponder)
{
	// Change settings only if tuner unused
	if(!bTunerUsed)
	{
		m_BDAFilterGraph.ChangeSetting();

		// Get lock status
		BOOLEAN bLocked = FALSE;
		LONG lSignalQuality = 0;		
		LONG lSignalStrength = 0;

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
		// Set autodiscover transponder
		m_AutodiscoverTransponder = autodiscoverTransponder;

		// Run the graph
		if(FAILED(m_BDAFilterGraph.RunGraph()))
		{
			g_Logger.log(0, true, TEXT("Error: Could not play the filter graph.\n"));
			return false;
		}
	}

	return true;
}

void Tuner::stopRecording()
{
	// Tell the parser to stop processing PSI packets
	m_BDAFilterGraph.getParser().stopPSIPackets();

	// The tuner stops the running graph
	m_BDAFilterGraph.StopGraph();
}

VOID NTAPI RunIdleCallback(PVOID vpTuner,
						   BOOLEAN alwaysTrue)
{
	Tuner* pTuner = (Tuner*)vpTuner;
	pTuner->m_BDAFilterGraph.StopGraph();
}

bool Tuner::runIdle()
{
	bool result = (m_BDAFilterGraph.RunGraph() == S_OK);
	// Create idle timer
	CreateTimerQueueTimer(&m_IdleTimer, m_TimerQueue, RunIdleCallback, this, g_Configuration.getInitialRunningTime() * 1000, 0, WT_EXECUTEDEFAULT | WT_EXECUTEONLYONCE);
	return result;
}