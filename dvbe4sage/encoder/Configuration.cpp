#include "StdAfx.h"
#include "Configuration.h"
#include "misc.h"

Configuration g_Configuration;

#define INI_FILE_NAME	TEXT(".\\dvbe4sage.ini")

Configuration::Configuration() :
	m_LogLevel(GetPrivateProfileInt(TEXT("General"), TEXT("LogLevel"), 2, INI_FILE_NAME)),
	m_NumberOfVirtualTuners((USHORT)GetPrivateProfileInt(TEXT("General"), TEXT("NumberOfVirtualTuners"), 3, INI_FILE_NAME)),
	m_DCWTimeout(GetPrivateProfileInt(TEXT("Plugins"), TEXT("DCWTimeout"), 2, INI_FILE_NAME)),
	m_EMMPid((USHORT)GetPrivateProfileInt(TEXT("Plugins"), TEXT("EMMPid"), 0xC0, INI_FILE_NAME)),
	m_InitialFrequency(GetPrivateProfileInt(TEXT("Tuning"), TEXT("InitialFrequency"), 10842000, INI_FILE_NAME)),
	m_InitialSymbolRate(GetPrivateProfileInt(TEXT("Tuning"), TEXT("InitialSymbolRate"), 27500, INI_FILE_NAME)),
	m_TSPacketsPerBuffer(GetPrivateProfileInt(TEXT("Tuning"), TEXT("TSPacketsPerBuffer"), 1024, INI_FILE_NAME)),
	m_NumberOfBuffers(GetPrivateProfileInt(TEXT("Tuning"), TEXT("NumberOfBuffers"), 400, INI_FILE_NAME)),
	m_TuningTimeout(GetPrivateProfileInt(TEXT("Tuning"), TEXT("TuningTimeout"), 20, INI_FILE_NAME)),
	m_InitialRunningTime(GetPrivateProfileInt(TEXT("Tuning"), TEXT("InitialRunningTime"), 20, INI_FILE_NAME)),
	m_UseSidForTuning(GetPrivateProfileInt(TEXT("Tuning"), TEXT("UseSidForTuning"), 0, INI_FILE_NAME) == 0 ? false : true),
	m_TSPacketsPerOutputBuffer(GetPrivateProfileInt(TEXT("Output"), TEXT("TSPacketsPerOutputBuffer"), 160000, INI_FILE_NAME)),
	m_TSPacketsOutputThreshold(GetPrivateProfileInt(TEXT("Output"), TEXT("TSPacketsOutputThreshold"), 70, INI_FILE_NAME)),
	m_DisableWriteBuffering(GetPrivateProfileInt(TEXT("Output"), TEXT("DisableWriteBuffering"), 0, INI_FILE_NAME) == 0 ? false : true),
	m_ListeningPort((USHORT)GetPrivateProfileInt(TEXT("Encoder"), TEXT("ListeningPort"), 6969, INI_FILE_NAME)),
	m_PATDilutionFactor((USHORT)GetPrivateProfileInt(TEXT("Recording"), TEXT("PATDilutionFactor"), 1, INI_FILE_NAME)),
	m_PMTDilutionFactor((USHORT)GetPrivateProfileInt(TEXT("Recording"), TEXT("PMTDilutionFactor"), 1, INI_FILE_NAME))
{	
	// Buffer
	TCHAR buffer[1024];

	// Get the list of tuners to exclude
	GetPrivateProfileString(TEXT("Tuning"), TEXT("ExcludeTuners"), TEXT(""), buffer, sizeof(buffer) / sizeof(buffer[0]), INI_FILE_NAME);
	LPTSTR context;
	for(LPCTSTR token = _tcstok_s(buffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
		m_ExcludeTuners.insert(_ttoi(token));

	// Get the list of DVB-S2 tuners
	GetPrivateProfileString(TEXT("Tuning"), TEXT("DVBS2Tuners"), TEXT(""), buffer, sizeof(buffer) / sizeof(buffer[0]), INI_FILE_NAME);
	for(LPCTSTR token = _tcstok_s(buffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
		m_DVBS2Tuners.insert(_ttoi(token));

	// Get the initial polarization setting
	GetPrivateProfileString(TEXT("Tuning"), TEXT("InitialPolarization"), TEXT("V"), buffer, sizeof(buffer) / sizeof(buffer[0]), INI_FILE_NAME);
	m_InitialPolarization = buffer[0] == TCHAR('V') ? BDA_POLARISATION_LINEAR_V : BDA_POLARISATION_LINEAR_H;

	// Get the initial modulation setting
	GetPrivateProfileString(TEXT("Tuning"), TEXT("InitialModulation"), TEXT("QPSK"), buffer, sizeof(buffer) / sizeof(buffer[0]), INI_FILE_NAME);
	m_InitialModulation = buffer[0] == TCHAR('Q') ? BDA_MOD_QPSK : BDA_MOD_8VSB;

	// Get the initial FEC setting
	GetPrivateProfileString(TEXT("Tuning"), TEXT("InitialFEC"), TEXT("3/4"), buffer, sizeof(buffer) / sizeof(buffer[0]), INI_FILE_NAME);
	m_InitialFEC = getFECFromString(buffer);

	// Get DECSA.dll name
	GetPrivateProfileString(TEXT("General"), TEXT("DECSADllName"), TEXT("FFDeCSA_64_MMX.dll"), buffer, sizeof(buffer) / sizeof(buffer[0]), INI_FILE_NAME);
	m_DECSADllName = buffer;

	// Get the preferred audio language
	GetPrivateProfileString(TEXT("Recording"), TEXT("PreferredAudioLanguage"), TEXT("eng"), buffer, sizeof(buffer) / sizeof(buffer[0]), INI_FILE_NAME);
	m_PreferredAudioLanguage = buffer;
}
