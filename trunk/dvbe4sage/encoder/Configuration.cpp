#include "StdAfx.h"

#include "..\\svnrevision\\svnrevision.h"
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
	log(0, true, 0, TEXT("DVBE4SAGE Encoder SVN revision %d\n"), SVN_REVISION);
	log(0, true, 0, TEXT("Loading configuration info from \"%s\"\n"), iniFileFullPath);

	// Buffer
	TCHAR buffer[1024];
	// Context for parsing
	LPTSTR context;

	log(0, false, 0, TEXT("=========================================================\n"));
	log(0, false, 0, TEXT("Configuration file dump:\n\n"));

	// General section
	log(0, false, 0, TEXT("[General]\n"));
	m_LogLevel = GetPrivateProfileInt(TEXT("General"), TEXT("LogLevel"), 2, iniFileFullPath);
	log(0, false, 0, TEXT("LogLevel=%u\n"), m_LogLevel);

	GetPrivateProfileString(TEXT("General"), TEXT("SetupType"), TEXT("DVBS"), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	if(_tcsicmp(buffer, TEXT("DVBS")) == 0)
	{
		m_SetupType = SETUP_DVBS;
		log(0, false, 0, TEXT("SetupType=DVBS\n"), m_LogLevel);
	}
	else if(_tcsicmp(buffer, TEXT("DVBC")) == 0)
	{
		m_SetupType = SETUP_DVBC;
		log(0, false, 0, TEXT("SetupType=DVBC\n"), m_LogLevel);
	}
	else if(_tcsicmp(buffer, TEXT("DVBT")) == 0)
	{
		m_SetupType = SETUP_DVBT;
		log(0, false, 0, TEXT("SetupType=DVBT\n"), m_LogLevel);
	}
	
	m_NumberOfVirtualTuners = (USHORT)GetPrivateProfileInt(TEXT("General"), TEXT("NumberOfVirtualTuners"), 1, iniFileFullPath);
	log(0, false, 0, TEXT("NumberOfVirtualTuners=%hu\n"), m_NumberOfVirtualTuners);

	m_IsNorthAmerica = GetPrivateProfileInt(TEXT("General"), TEXT("NorthAmerica"), 0, iniFileFullPath) == 0 ? false : true;
	log(0, false, 0, TEXT("NorthAmerica=%u\n"), m_IsNorthAmerica ? 1 : 0);

	log(0, false, 0, TEXT("\n"));

	// Plugins section
	log(0, false, 0, TEXT("[Plugins]\n"));
	m_DCWTimeout = GetPrivateProfileInt(TEXT("Plugins"), TEXT("DCWTimeout"), 5, iniFileFullPath);
	log(0, false, 0, TEXT("DCWTimeout=%u\n"), m_DCWTimeout);

	m_IsVGCam = GetPrivateProfileInt(TEXT("Plugins"), TEXT("IsVGCam"), 0, iniFileFullPath) == 0 ? false : true;
	log(0, false, 0, TEXT("IsVGCam=%u\n"), m_IsVGCam ? 1 : 0);

	m_MaxNumberOfResets = GetPrivateProfileInt(TEXT("Plugins"), TEXT("MaxNumberOfResets"), 5, iniFileFullPath) == 0 ? false : true;
	log(0, false, 0, TEXT("MaxNumberOfResets=%hu\n"), m_MaxNumberOfResets);

	// Get the list of served CAIDs
	GetPrivateProfileString(TEXT("Plugins"), TEXT("ServedCAIDs"), TEXT(""), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	if(buffer[0] != TCHAR(0))
		for(LPCTSTR token = _tcstok_s(buffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
		{
			USHORT caid;
			_stscanf_s(token, TEXT("%hx"), &caid);
			m_ServedCAIDs.insert(caid);
		}
	log(0, false, 0, TEXT("ServedCAIDs="));
	for(hash_set<USHORT>::const_iterator it = m_ServedCAIDs.begin(); it != m_ServedCAIDs.end();)
	{
		log(0, false, 0, TEXT("%hX"), *it);
		if(++it != m_ServedCAIDs.end())
			log(0, false, 0, TEXT(","));
	}
	log(0, false, 0, TEXT("\n"));

	// Get the list of served PROVIDs
	GetPrivateProfileString(TEXT("Plugins"), TEXT("ServedPROVIds"), TEXT(""), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	if(buffer[0] != TCHAR(0))
		for(LPCTSTR token = _tcstok_s(buffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
		{
			UINT provid;
			_stscanf_s(token, TEXT("%x"), &provid);
			m_ServededProvIds.insert(provid);
		}

	log(0, false, 0, TEXT("ServedPROVIds="));
	for(hash_set<UINT>::const_iterator it = m_ServededProvIds.begin(); it != m_ServededProvIds.end();)
	{
		log(0, false, 0, TEXT("%X"), *it);
		if(++it != m_ServededProvIds.end())
			log(0, false, 0, TEXT(","));
	}
	log(0, false, 0, TEXT("\n"));

	log(0, false, 0, TEXT("\n"));

	// Tuning section
	log(0, false, 0, TEXT("[Tuning]\n"));
	m_LNBSW = GetPrivateProfileInt(TEXT("Tuning"), TEXT("LNB_SW"), 11700000, iniFileFullPath);
	log(0, false, 0, TEXT("LNBSW=%lu\n"), m_LNBSW);

	m_LNBLOF1 = GetPrivateProfileInt(TEXT("Tuning"), TEXT("LNB_LOF1"), 9750000, iniFileFullPath);
	log(0, false, 0, TEXT("LNBLOF1=%lu\n"), m_LNBLOF1);

	m_LNBLOF2 = GetPrivateProfileInt(TEXT("Tuning"), TEXT("LNB_LOF2"), 10600000, iniFileFullPath);
	log(0, false, 0, TEXT("LNBLOF2=%lu\n"), m_LNBLOF2);

	// Get the initial frequency setting
	GetPrivateProfileString(TEXT("Tuning"), TEXT("InitialFrequency"), TEXT(""), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	if(buffer[0] != TCHAR(0))
	{
		int idx = 0;
		for(LPCTSTR token = _tcstok_s(buffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
		{
			ULONG val;
			_stscanf_s(token, TEXT("%lu"), &val);
			m_InitialFrequency[idx++] = val;
		}
	}
	log(0, false, 0, TEXT("InitialFrequency="));
	for(hash_map<int, ULONG>::iterator it = m_InitialFrequency.begin(); it != m_InitialFrequency.end();)
	{
		log(0, false, 0, TEXT("%lu"), it->second);
		if(++it != m_InitialFrequency.end())
			log(0, false, 0, TEXT(","));
	}
	log(0, false, 0, TEXT("\n"));

	// Get the initial symbol rate setting
	GetPrivateProfileString(TEXT("Tuning"), TEXT("InitialSymbolRate"), TEXT(""), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	if(buffer[0] != TCHAR(0))
	{
		int idx = 0;
		for(LPCTSTR token = _tcstok_s(buffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
		{
			ULONG val;
			_stscanf_s(token, TEXT("%lu"), &val);
			m_InitialSymbolRate[idx++] = val;
		}
	}
	log(0, false, 0, TEXT("InitialSymbolRate="));
	for(hash_map<int, ULONG>::iterator it = m_InitialSymbolRate.begin(); it != m_InitialSymbolRate.end();)
	{
		log(0, false, 0, TEXT("%lu"), it->second);
		if(++it != m_InitialSymbolRate.end())
			log(0, false, 0, TEXT(","));
	}
	log(0, false, 0, TEXT("\n"));

	// Get the initial polarization setting
	GetPrivateProfileString(TEXT("Tuning"), TEXT("InitialPolarization"), TEXT(""), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	if(buffer[0] != TCHAR(0))
	{
		int idx = 0;
		for(LPCTSTR token = _tcstok_s(buffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
		{
			Polarisation val = getPolarizationFromString(token);			
			m_InitialPolarization[idx++] = val;
		}
	}
	log(0, false, 0, TEXT("InitialPolarization="));
	for(hash_map<int, Polarisation>::iterator it = m_InitialPolarization.begin(); it != m_InitialPolarization.end();)
	{
		log(0, false, 0, TEXT("%s"), printablePolarization(it->second));
		if(++it != m_InitialPolarization.end())
			log(0, false, 0, TEXT(","));
	}
	log(0, false, 0, TEXT("\n"));

	// Get the initial modulation setting
	GetPrivateProfileString(TEXT("Tuning"), TEXT("InitialModulation"), TEXT(""), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	if(buffer[0] != TCHAR(0))
	{
		int idx = 0;
		for(LPCTSTR token = _tcstok_s(buffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
		{
			ModulationType val = getModulationFromString(token);			
			m_InitialModulation[idx++] = val;
		}
	}
	log(0, false, 0, TEXT("InitialModulation="));
	for(hash_map<int, ModulationType>::iterator it = m_InitialModulation.begin(); it != m_InitialModulation.end();)
	{
		log(0, false, 0, TEXT("%s"), printableModulation(it->second));
		if(++it != m_InitialModulation.end())
			log(0, false, 0, TEXT(","));
	}
	log(0, false, 0, TEXT("\n"));

	// Get the initial FEC setting
	GetPrivateProfileString(TEXT("Tuning"), TEXT("InitialFEC"), TEXT(""), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	if(buffer[0] != TCHAR(0))
	{
		int idx = 0;
		for(LPCTSTR token = _tcstok_s(buffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
		{
			BinaryConvolutionCodeRate val = getFECFromString(token);			
			m_InitialFEC[idx++] = val;
		}
	}
	log(0, false, 0, TEXT("InitialFEC="));
	for(hash_map<int, BinaryConvolutionCodeRate>::iterator it = m_InitialFEC.begin(); it != m_InitialFEC.end();)
	{
		log(0, false, 0, TEXT("%s"), printableFEC(it->second));
		if(++it != m_InitialFEC.end())
			log(0, false, 0, TEXT(","));
	}
	log(0, false, 0, TEXT("\n"));

	// Get the list of DVB-S2 tuners
	GetPrivateProfileString(TEXT("Tuning"), TEXT("DVBS2Tuners"), TEXT(""), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	for(LPCTSTR token = _tcstok_s(buffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
		m_DVBS2Tuners.insert(_ttoi(token));

	log(0, false, 0, TEXT("DVBS2Tuners="));
	for(hash_set<UINT>::const_iterator it = m_DVBS2Tuners.begin(); it != m_DVBS2Tuners.end();)
	{
		log(0, false, 0, TEXT("%u"), *it);
		if(++it != m_DVBS2Tuners.end())
			log(0, false, 0, TEXT(","));
	}
	log(0, false, 0, TEXT("\n"));

	m_TSPacketsPerBuffer = GetPrivateProfileInt(TEXT("Tuning"), TEXT("TSPacketsPerBuffer"), 1024, iniFileFullPath);
	log(0, false, 0, TEXT("TSPacketsPerBuffer=%u\n"), m_TSPacketsPerBuffer);

	m_NumberOfBuffers = GetPrivateProfileInt(TEXT("Tuning"), TEXT("NumberOfBuffers"), 400, iniFileFullPath);
	log(0, false, 0, TEXT("NumberOfBuffers=%u\n"), m_NumberOfBuffers);

	m_TuningTimeout = GetPrivateProfileInt(TEXT("Tuning"), TEXT("TuningTimeout"), 20, iniFileFullPath);
	log(0, false, 0, TEXT("TuningTimeout=%u\n"), m_TuningTimeout);

	m_TuningLockTimeout = GetPrivateProfileInt(TEXT("Tuning"), TEXT("TuningLockTimeout"), 5, iniFileFullPath);
	log(0, false, 0, TEXT("TuningLockTimeout=%u\n"), m_TuningLockTimeout);

	m_InitialRunningTime = GetPrivateProfileInt(TEXT("Tuning"), TEXT("InitialRunningTime"), 20, iniFileFullPath);
	log(0, false, 0, TEXT("InitialRunningTime=%u\n"), m_InitialRunningTime);

	m_UseSidForTuning = GetPrivateProfileInt(TEXT("Tuning"), TEXT("UseSidForTuning"), 0, iniFileFullPath) == 0 ? false : true;
	log(0, false, 0, TEXT("UseSidForTuning=%u\n"), m_UseSidForTuning ? 1: 0);

	m_UseNewTuningMethod = GetPrivateProfileInt(TEXT("Tuning"), TEXT("UseNewTuningMethod"), 0, iniFileFullPath) == 0 ? false : true;
	log(0, false, 0, TEXT("UseNewTuningMethod=%u\n"), m_UseNewTuningMethod ? 1 : 0);

	m_ScanAllTransponders = GetPrivateProfileInt(TEXT("Tuning"), TEXT("ScanAllTransponders"), 0, iniFileFullPath) == 0 ? false : true;
	log(0, false, 0, TEXT("ScanAllTransponders=%hu\n"), m_ScanAllTransponders);

	// List of tuners to be excluded by ordinal
	GetPrivateProfileString(TEXT("Tuning"), TEXT("ExcludeTuners"), TEXT(""), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	for(LPCTSTR token = _tcstok_s(buffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
		m_ExcludedTuners.insert(_ttoi(token));

	log(0, false, 0, TEXT("ExcludeTuners="));
	for(hash_set<UINT>::const_iterator it = m_ExcludedTuners.begin(); it != m_ExcludedTuners.end();)
	{
		log(0, false, 0, TEXT("%X"), *it);
		if(++it != m_ExcludedTuners.end())
			log(0, false, 0, TEXT(","));
	}
	log(0, false, 0, TEXT("\n"));

	// List of tuners to be excluded by their MAC
	GetPrivateProfileString(TEXT("Tuning"), TEXT("ExcludeTunersMAC"), TEXT(""), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	for(LPCTSTR token = _tcstok_s(buffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
		m_ExcludedTunersMAC.insert(token);

	log(0, false, 0, TEXT("ExcludeTunersMAC="));
	for(hash_set<string>::const_iterator it = m_ExcludedTunersMAC.begin(); it != m_ExcludedTunersMAC.end();)
	{
		log(0, false, 0, TEXT("%s"), (*it).c_str());
		if(++it != m_ExcludedTunersMAC.end())
			log(0, false, 0, TEXT(","));
	}
	log(0, false, 0, TEXT("\n"));

	// List of tuners to be included by ordinal
	GetPrivateProfileString(TEXT("Tuning"), TEXT("IncludeTuners"), TEXT(""), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	for(LPCTSTR token = _tcstok_s(buffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
		m_IncludedTuners.insert(_ttoi(token));

	log(0, false, 0, TEXT("IncludeTuners="));
	for(hash_set<UINT>::const_iterator it = m_IncludedTuners.begin(); it != m_IncludedTuners.end();)
	{
		log(0, false, 0, TEXT("%X"), *it);
		if(++it != m_IncludedTuners.end())
			log(0, false, 0, TEXT(","));
	}
	log(0, false, 0, TEXT("\n"));

	// List of tuners to be included by their MAC
	GetPrivateProfileString(TEXT("Tuning"), TEXT("IncludeTunersMAC"), TEXT(""), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	for(LPCTSTR token = _tcstok_s(buffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
		m_IncludedTunersMAC.insert(token);

	log(0, false, 0, TEXT("IncludeTunersMAC="));
	for(hash_set<string>::const_iterator it = m_IncludedTunersMAC.begin(); it != m_IncludedTunersMAC.end();)
	{
		log(0, false, 0, TEXT("%s"), (*it).c_str());
		if(++it != m_IncludedTunersMAC.end())
			log(0, false, 0, TEXT(","));
	}
	log(0, false, 0, TEXT("\n"));

	m_UseDiseqc = GetPrivateProfileInt(TEXT("Tuning"), TEXT("UseDiseqc"), 0, iniFileFullPath) == 0 ? false : true;
	log(0, false, 0, TEXT("UseDiseqc=%u\n"), m_UseDiseqc ? 1 : 0);

	m_AutoDiscoverONID = GetPrivateProfileInt(TEXT("Tuning"), TEXT("AutoDiscoverONID"), 0, iniFileFullPath) == 0 ? false : true;
	log(0, false, 0, TEXT("AutoDiscoverONID=%u\n"), m_AutoDiscoverONID ? 1 : 0);

	// List of network IDs to be included 
	GetPrivateProfileString(TEXT("Tuning"), TEXT("IncludeONIDs"), TEXT(""), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	for(LPCTSTR token = _tcstok_s(buffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
	{
		USHORT onid;
		_stscanf_s(token, TEXT("%hu"), &onid);
		m_IncludedONIDs.insert(onid);
	}

	log(0, false, 0, TEXT("IncludeONIDs="));
	for(hash_set<USHORT>::const_iterator it = m_IncludedONIDs.begin(); it != m_IncludedONIDs.end();)
	{
		log(0, false, 0, TEXT("%hu"), *it);
		if(++it != m_IncludedONIDs.end())
			log(0, false, 0, TEXT(","));
	}
	log(0, false, 0, TEXT("\n"));

	// List of network IDs to be excluded 
	GetPrivateProfileString(TEXT("Tuning"), TEXT("ExcludeONIDs"), TEXT(""), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	for(LPCTSTR token = _tcstok_s(buffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
	{
		USHORT onid;
		_stscanf_s(token, TEXT("%hu"), &onid);
		m_ExcludedONIDs.insert(onid);
	}

	log(0, false, 0, TEXT("ExcludeONIDs="));
	for(hash_set<USHORT>::const_iterator it = m_ExcludedONIDs.begin(); it != m_ExcludedONIDs.end();)
	{
		log(0, false, 0, TEXT("%hu"), *it);
		if(++it != m_ExcludedONIDs.end())
			log(0, false, 0, TEXT(","));
	}
	log(0, false, 0, TEXT("\n"));

	log(0, false, 0, TEXT("\n"));

	// Output section
	log(0, false, 0, TEXT("[Output]\n"));
	m_TSPacketsPerOutputBuffer = GetPrivateProfileInt(TEXT("Output"), TEXT("TSPacketsPerOutputBuffer"), 160000, iniFileFullPath);
	log(0, false, 0, TEXT("TSPacketsPerOutputBuffer=%u\n"), m_TSPacketsPerOutputBuffer);

	m_TSPacketsOutputThreshold = GetPrivateProfileInt(TEXT("Output"), TEXT("TSPacketsOutputThreshold"), 135, iniFileFullPath);
	log(0, false, 0, TEXT("TSPacketsOutputThreshold=%u\n"), m_TSPacketsOutputThreshold);

	m_DisableWriteBuffering = GetPrivateProfileInt(TEXT("Output"), TEXT("DisableWriteBuffering"), 0, iniFileFullPath) == 0 ? false : true;
	log(0, false, 0, TEXT("DisableWriteBuffering=%u\n"), m_DisableWriteBuffering ? 1 : 0);

	log(0, false, 0, TEXT("\n"));

	// Encoder section
	log(0, false, 0, TEXT("[Encoder]\n"));
	m_ListeningPort = (USHORT)GetPrivateProfileInt(TEXT("Encoder"), TEXT("ListeningPort"), 6969, iniFileFullPath);
	log(0, false, 0, TEXT("ListeningPort=%hu\n"), m_ListeningPort);

	log(0, false, 0, TEXT("\n"));

	// Recording section
	log(0, false, 0, TEXT("[Recording]\n"));
	// Get the preferred audio language
	GetPrivateProfileString(TEXT("Recording"), TEXT("PreferredAudioLanguage"), TEXT("eng"), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	m_PreferredAudioLanguage = buffer;
	log(0, false, 0, TEXT("PreferredAudioLanguage=%s\n"), m_PreferredAudioLanguage.c_str());

	// Get the preferred audio format
	GetPrivateProfileString(TEXT("Recording"), TEXT("PreferredAudioFormat"), TEXT("ac3"), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	m_PreferredAudioFormat = buffer;
	log(0, false, 0, TEXT("PreferredAudioFormat=%s\n"), m_PreferredAudioFormat.c_str());

	// Get the preferred subtitles language
	GetPrivateProfileString(TEXT("Recording"), TEXT("PreferredSubtitlesLanguage"), TEXT("eng"), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	m_PreferredSubtitlesLanguage = buffer;
	log(0, false, 0, TEXT("PreferredSubtitlesLanguage=%s\n"), m_PreferredSubtitlesLanguage.c_str());

	log(0, false, 0, TEXT("\n"));

	// Advanced section
	log(0, false, 0, TEXT("[Advanced]\n"));

	m_PMTDilutionFactor = (USHORT)GetPrivateProfileInt(TEXT("Advanced"), TEXT("PMTDilutionFactor"), 1, iniFileFullPath);
	log(0, false, 0, TEXT("PMTDilutionFactor=%hu\n"), m_PMTDilutionFactor);

	m_PATDilutionFactor = (USHORT)GetPrivateProfileInt(TEXT("Advanced"), TEXT("PATDilutionFactor"), 1, iniFileFullPath);
	log(0, false, 0, TEXT("PATDilutionFactor=%hu\n"), m_PATDilutionFactor);

	m_PMTThreshold = (USHORT)GetPrivateProfileInt(TEXT("Advanced"), TEXT("PMTThreshold"), 20, iniFileFullPath);
	log(0, false, 0, TEXT("PMTThreshold=%hu\n"), m_PMTThreshold);

	m_PSIMaturityTime = (USHORT)GetPrivateProfileInt(TEXT("Advanced"), TEXT("PSIMaturityTime"), 10, iniFileFullPath);
	log(0, false, 0, TEXT("PSIMaturityTime=%hu\n"), m_PSIMaturityTime);

	m_MaxECMCacheSize = (USHORT)GetPrivateProfileInt(TEXT("Advanced"), TEXT("MaxECMCacheSize"), 3000, iniFileFullPath);
	log(0, false, 0, TEXT("MaxECMCacheSize=%hu\n"), m_MaxECMCacheSize);

	m_EnableECMCache = GetPrivateProfileInt(TEXT("Advanced"), TEXT("EnableECMCache"), 1, iniFileFullPath) == 0 ? false : true;
	log(0, false, 0, TEXT("EnableECMCache=%u\n"), m_EnableECMCache ? 1 : 0);

	m_ECMCacheAutodeleteChunkSize = (USHORT)GetPrivateProfileInt(TEXT("Advanced"), TEXT("ECMCacheAutodeleteChunkSize"), 300, iniFileFullPath);
	log(0, false, 0, TEXT("ECMCacheAutodeleteChunkSize=%hu\n"), m_ECMCacheAutodeleteChunkSize);

	GetPrivateProfileString(TEXT("Advanced"), TEXT("BouquetName"), TEXT("BSkyB Bouquet 1 - DTH England"), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	m_BouquetName = buffer;
	log(0, false, 0, TEXT("BouquetName=%s\n"), m_BouquetName.c_str());

	m_PreferSDOverHD = GetPrivateProfileInt(TEXT("Advanced"), TEXT("PreferSDOverHD"), 0, iniFileFullPath) == 0 ? false : true;
	log(0, false, 0, TEXT("PreferSDOverHD=%u\n"), m_PreferSDOverHD ? 1 : 0);

	// List of transponders to be excluded by their TIDs
	GetPrivateProfileString(TEXT("Advanced"), TEXT("ExcludeTIDs"), TEXT(""), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	for(LPCTSTR token = _tcstok_s(buffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
		m_ExcludedTIDs.insert((USHORT)_ttoi(token));

	log(0, false, 0, TEXT("ExcludeTIDs="));
	for(hash_set<USHORT>::const_iterator it = m_ExcludedTIDs.begin(); it != m_ExcludedTIDs.end();)
	{
		log(0, false, 0, TEXT("%hu"), (USHORT)*it);
		if(++it != m_ExcludedTIDs.end())
			log(0, false, 0, TEXT(","));
	}

	// List of preferred transponders by their TID
	GetPrivateProfileString(TEXT("Advanced"), TEXT("PreferredTIDs"), TEXT(""), buffer, sizeof(buffer) / sizeof(buffer[0]), iniFileFullPath);
	for(LPCTSTR token = _tcstok_s(buffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
		m_PreferredTIDs.insert((USHORT)_ttoi(token));

	log(0, false, 0, TEXT("PreferredTIDs="));
	for(hash_set<USHORT>::const_iterator it = m_PreferredTIDs.begin(); it != m_PreferredTIDs.end();)
	{
		log(0, false, 0, TEXT("%hu"), (USHORT)*it);
		if(++it != m_PreferredTIDs.end())
			log(0, false, 0, TEXT(","));
	}

	log(0, false, 0, TEXT("\n"));

	log(0, false, 0, TEXT("End of configuration file dump\n"));
	log(0, false, 0, TEXT("=========================================================\n"));
}

ULONG Configuration::getInitialFrequency(int onidIndex) const
{ 
	hash_map<int, ULONG>::const_iterator it = m_InitialFrequency.find(onidIndex); 
	if(it != m_InitialFrequency.end())
		return it->second;
	else 
		return 0;
}

ULONG Configuration::getInitialSymbolRate(int onidIndex) const
{ 
	hash_map<int, ULONG>::const_iterator it = m_InitialSymbolRate.find(onidIndex); 
	if(it != m_InitialSymbolRate.end())
		return it->second;
	else 
		return 0;
}

Polarisation Configuration::getInitialPolarization(int onidIndex) const
{ 
	hash_map<int, Polarisation>::const_iterator it = m_InitialPolarization.find(onidIndex); 
	if(it != m_InitialPolarization.end())
		return it->second;
	else 
		return BDA_POLARISATION_NOT_SET;
}

ModulationType Configuration::getInitialModulation(int onidIndex) const
{ 
	hash_map<int, ModulationType>::const_iterator it = m_InitialModulation.find(onidIndex); 
	if(it != m_InitialModulation.end())
		return it->second;
	else 
		return BDA_MOD_NOT_SET;
}

BinaryConvolutionCodeRate Configuration::getInitialFEC(int onidIndex) const
{ 
	hash_map<int, BinaryConvolutionCodeRate>::const_iterator it = m_InitialFEC.find(onidIndex); 
	if(it != m_InitialFEC.end())
		return it->second;
	else 
		return BDA_BCC_RATE_NOT_SET;
}
