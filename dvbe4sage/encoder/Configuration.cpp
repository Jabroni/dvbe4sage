#include "StdAfx.h"

#include "Configuration.h"
#include "extern.h"
#include "logger.h"

Configuration* g_pConfiguration = NULL;

#define INI_FILE_NAME	TEXT("\\dvbe4sage.ini")

Configuration::Configuration()
{
	// Get the current directory
	TCHAR iniFileFullPath[MAXSHORT];
	DWORD iniFileFullPathLen = GetCurrentDirectory(sizeof(iniFileFullPath) / sizeof(iniFileFullPath[0]), iniFileFullPath);
	// Build the INI file name
	_tcscpy_s(&iniFileFullPath[iniFileFullPathLen], sizeof(iniFileFullPath) / sizeof(iniFileFullPath[0]) - iniFileFullPathLen - 1, INI_FILE_NAME);

	// Log info
	log(0, true, 0, TEXT("Loading configuration info from \"%s\"\n"), iniFileFullPath);

	m_LogLevel = GetPrivateProfileInt(TEXT("General"), TEXT("LogLevel"), 2, iniFileFullPath);
	m_NumberOfVirtualTuners = (USHORT)GetPrivateProfileInt(TEXT("General"), TEXT("NumberOfVirtualTuners"), 3, iniFileFullPath);
	m_DCWTimeout = GetPrivateProfileInt(TEXT("Plugins"), TEXT("DCWTimeout"), 5, iniFileFullPath);
	m_LNBSW = GetPrivateProfileInt(TEXT("Tuning"), TEXT("LNB_SW"), 11700000, iniFileFullPath);
	m_LNBLOF1 = GetPrivateProfileInt(TEXT("Tuning"), TEXT("LNB_LOF1"), 9750000, iniFileFullPath);
	m_LNBLOF2 = GetPrivateProfileInt(TEXT("Tuning"), TEXT("LNB_LOF2"), 10600000, iniFileFullPath);
	m_InitialFrequency = GetPrivateProfileInt(TEXT("Tuning"), TEXT("InitialFrequency"), 10842000, iniFileFullPath);
	m_InitialSymbolRate = GetPrivateProfileInt(TEXT("Tuning"), TEXT("InitialSymbolRate"), 27500, iniFileFullPath);
	m_TSPacketsPerBuffer = GetPrivateProfileInt(TEXT("Tuning"), TEXT("TSPacketsPerBuffer"), 1024, iniFileFullPath);
	m_NumberOfBuffers = GetPrivateProfileInt(TEXT("Tuning"), TEXT("NumberOfBuffers"), 400, iniFileFullPath);
	m_TuningTimeout = GetPrivateProfileInt(TEXT("Tuning"), TEXT("TuningTimeout"), 20, iniFileFullPath);
	m_TuningLockTimeout = GetPrivateProfileInt(TEXT("Tuning"), TEXT("TuningLockTimeout"), 5, iniFileFullPath);
	m_InitialRunningTime = GetPrivateProfileInt(TEXT("Tuning"), TEXT("InitialRunningTime"), 20, iniFileFullPath);
	m_UseSidForTuning = GetPrivateProfileInt(TEXT("Tuning"), TEXT("UseSidForTuning"), 0, iniFileFullPath) == 0 ? false : true;
	m_TSPacketsPerOutputBuffer = GetPrivateProfileInt(TEXT("Output"), TEXT("TSPacketsPerOutputBuffer"), 160000, iniFileFullPath);
	m_TSPacketsOutputThreshold = GetPrivateProfileInt(TEXT("Output"), TEXT("TSPacketsOutputThreshold"), 70, iniFileFullPath);
	m_DisableWriteBuffering = GetPrivateProfileInt(TEXT("Output"), TEXT("DisableWriteBuffering"), 0, iniFileFullPath) == 0 ? false : true;
	m_ListeningPort = (USHORT)GetPrivateProfileInt(TEXT("Encoder"), TEXT("ListeningPort"), 6969, iniFileFullPath);
	m_PATDilutionFactor = (USHORT)GetPrivateProfileInt(TEXT("Advanced"), TEXT("PATDilutionFactor"), 1, iniFileFullPath);
	m_PMTDilutionFactor = (USHORT)GetPrivateProfileInt(TEXT("Advanced"), TEXT("PMTDilutionFactor"), 1, iniFileFullPath);
	m_PMTThreshold = (USHORT)GetPrivateProfileInt(TEXT("Advanced"), TEXT("PMTThreshold"), 20, iniFileFullPath);
	m_PSIMaturityTime = (USHORT)GetPrivateProfileInt(TEXT("Advanced"), TEXT("PSIMaturityTime"), 10, iniFileFullPath);
	m_UseNewTuningMethod = GetPrivateProfileInt(TEXT("Tuning"), TEXT("UseNewTuningMethod"), 0, iniFileFullPath) == 0 ? false : true;
	m_DontFixPMT = GetPrivateProfileInt(TEXT("Advanced"), TEXT("DontFixPMT"), 0, iniFileFullPath) == 0 ? false : true;
	m_IsVGCam = GetPrivateProfileInt(TEXT("Plugins"), TEXT("IsVGCam"), 0, iniFileFullPath) == 0 ? false : true;
	m_MaxNumberOfResets = GetPrivateProfileInt(TEXT("Plugins"), TEXT("MaxNumberOfResets"), 10, iniFileFullPath) == 0 ? false : true;
	m_MaxECMCacheSize = (USHORT)GetPrivateProfileInt(TEXT("Advanced"), TEXT("MaxECMCacheSize"), 3000, iniFileFullPath);
	m_ECMCacheAutodeleteChunkSize = (USHORT)GetPrivateProfileInt(TEXT("Advanced"), TEXT("ECMCacheAutodeleteChunkSize"), 300, iniFileFullPath);

	// Buffer
	TCHAR buffer[1024];
	// Context for parsing
	LPTSTR context;

	// List of tuners to be excluded by ordinal
	GetPrivateProfileString(TEXT("Tuning"), TEXT("ExcludeTuners"), TEXT(""), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	for(LPCTSTR token = _tcstok_s(buffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
		m_ExcludedTuners.insert(_ttoi(token));

	// List of tuners to be excluded by their MAC
	GetPrivateProfileString(TEXT("Tuning"), TEXT("ExcludeTunersMAC"), TEXT(""), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	for(LPCTSTR token = _tcstok_s(buffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
		m_ExcludedTunersMAC.insert(token);

	// List of tuners to be included by ordinal
	GetPrivateProfileString(TEXT("Tuning"), TEXT("IncludeTuners"), TEXT(""), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	for(LPCTSTR token = _tcstok_s(buffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
		m_IncludedTuners.insert(_ttoi(token));

	// List of tuners to be included by their MAC
	GetPrivateProfileString(TEXT("Tuning"), TEXT("IncludeTunersMAC"), TEXT(""), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	for(LPCTSTR token = _tcstok_s(buffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
		m_IncludedTunersMAC.insert(token);

	// Get the list of DVB-S2 tuners
	GetPrivateProfileString(TEXT("Tuning"), TEXT("DVBS2Tuners"), TEXT(""), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	for(LPCTSTR token = _tcstok_s(buffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
		m_DVBS2Tuners.insert(_ttoi(token));

	// Get the list of served CAIDs
	GetPrivateProfileString(TEXT("Plugins"), TEXT("ServedCAIDs"), TEXT(""), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	if(buffer[0] != TCHAR(0))
		for(LPCTSTR token = _tcstok_s(buffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
		{
			USHORT caid;
			_stscanf_s(token, TEXT("%hx"), &caid);
			m_ServedCAIDs.insert(caid);
		}
	/*else
		// Default is YES
		m_ServedCAIDs.insert(0x90D);*/

	// Get the list of ignored PMT PIDs
	GetPrivateProfileString(TEXT("Plugins"), TEXT("IgnoreCAPids"), TEXT(""), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	if(buffer[0] != TCHAR(0))
		for(LPCTSTR token = _tcstok_s(buffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
		{
			USHORT caid;
			_stscanf_s(token, TEXT("%hx"), &caid);
			m_IgnoredCAPids.insert(caid);
		}

	// Get the initial polarization setting
	GetPrivateProfileString(TEXT("Tuning"), TEXT("InitialPolarization"), TEXT("V"), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	m_InitialPolarization = getPolarizationFromString(buffer);

	// Get the initial modulation setting
	GetPrivateProfileString(TEXT("Tuning"), TEXT("InitialModulation"), TEXT("QPSK"), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	m_InitialModulation = getModulationFromString(buffer);

	// Get the initial FEC setting
	GetPrivateProfileString(TEXT("Tuning"), TEXT("InitialFEC"), TEXT("3/4"), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	m_InitialFEC = getFECFromString(buffer);

	// Get DECSA.dll name
	GetPrivateProfileString(TEXT("General"), TEXT("DECSADllName"), TEXT("FFDeCSA_64_MMX.dll"), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	m_DECSADllName = buffer;

	// Get the preferred audio language
	GetPrivateProfileString(TEXT("Recording"), TEXT("PreferredAudioLanguage"), TEXT("eng"), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	m_PreferredAudioLanguage = buffer;

	/*ifstream ifs("config.xml");
	try
	{
		auto_ptr<configuration> config(configuration_(ifs));
		for(UINT i = 0; i < config->providers().provider().size(); i++)
		{
			const char* name = config->providers().provider()[i].name().c_str();
			name = name;
		}
	}
	catch(const xml_schema::exception& e)
	{
		ofstream ofs("error.txt");
		ofs << e << endl;
	}*/
}
