#include "StdAfx.h"

#include "Configuration.h"
#include "extern.h"

Configuration g_Configuration;

#define INI_FILE_NAME	TEXT(".\\dvbe4sage.ini")

Configuration::Configuration() :
	m_LogLevel(GetPrivateProfileInt(TEXT("General"), TEXT("LogLevel"), 2, INI_FILE_NAME)),
	m_NumberOfVirtualTuners((USHORT)GetPrivateProfileInt(TEXT("General"), TEXT("NumberOfVirtualTuners"), 3, INI_FILE_NAME)),
	m_DCWTimeout(GetPrivateProfileInt(TEXT("Plugins"), TEXT("DCWTimeout"), 5, INI_FILE_NAME)),
	m_LNBSW(GetPrivateProfileInt(TEXT("Tuning"), TEXT("LNB_SW"), 11700000, INI_FILE_NAME)),
	m_LNBLOF1(GetPrivateProfileInt(TEXT("Tuning"), TEXT("LNB_LOF1"), 9750000, INI_FILE_NAME)),
	m_LNBLOF2(GetPrivateProfileInt(TEXT("Tuning"), TEXT("LNB_LOF2"), 10600000, INI_FILE_NAME)),
	m_InitialFrequency(GetPrivateProfileInt(TEXT("Tuning"), TEXT("InitialFrequency"), 10842000, INI_FILE_NAME)),
	m_InitialSymbolRate(GetPrivateProfileInt(TEXT("Tuning"), TEXT("InitialSymbolRate"), 27500, INI_FILE_NAME)),
	m_TSPacketsPerBuffer(GetPrivateProfileInt(TEXT("Tuning"), TEXT("TSPacketsPerBuffer"), 1024, INI_FILE_NAME)),
	m_NumberOfBuffers(GetPrivateProfileInt(TEXT("Tuning"), TEXT("NumberOfBuffers"), 400, INI_FILE_NAME)),
	m_TuningTimeout(GetPrivateProfileInt(TEXT("Tuning"), TEXT("TuningTimeout"), 20, INI_FILE_NAME)),
	m_TuningLockTimeout(GetPrivateProfileInt(TEXT("Tuning"), TEXT("TuningLockTimeout"), 5, INI_FILE_NAME)),
	m_InitialRunningTime(GetPrivateProfileInt(TEXT("Tuning"), TEXT("InitialRunningTime"), 20, INI_FILE_NAME)),
	m_UseSidForTuning(GetPrivateProfileInt(TEXT("Tuning"), TEXT("UseSidForTuning"), 0, INI_FILE_NAME) == 0 ? false : true),
	m_TSPacketsPerOutputBuffer(GetPrivateProfileInt(TEXT("Output"), TEXT("TSPacketsPerOutputBuffer"), 160000, INI_FILE_NAME)),
	m_TSPacketsOutputThreshold(GetPrivateProfileInt(TEXT("Output"), TEXT("TSPacketsOutputThreshold"), 70, INI_FILE_NAME)),
	m_DisableWriteBuffering(GetPrivateProfileInt(TEXT("Output"), TEXT("DisableWriteBuffering"), 0, INI_FILE_NAME) == 0 ? false : true),
	m_ListeningPort((USHORT)GetPrivateProfileInt(TEXT("Encoder"), TEXT("ListeningPort"), 6969, INI_FILE_NAME)),
	m_PATDilutionFactor((USHORT)GetPrivateProfileInt(TEXT("Advanced"), TEXT("PATDilutionFactor"), 1, INI_FILE_NAME)),
	m_PMTDilutionFactor((USHORT)GetPrivateProfileInt(TEXT("Advanced"), TEXT("PMTDilutionFactor"), 1, INI_FILE_NAME)),
	m_PMTThreshold((USHORT)GetPrivateProfileInt(TEXT("Advanced"), TEXT("PMTThreshold"), 20, INI_FILE_NAME)),
	m_PSIMaturityTime((USHORT)GetPrivateProfileInt(TEXT("Advanced"), TEXT("PSIMaturityTime"), 10, INI_FILE_NAME)),
	m_UseNewTuningMethod(GetPrivateProfileInt(TEXT("Tuning"), TEXT("UseNewTuningMethod"), 0, INI_FILE_NAME) == 0 ? false : true),
	m_DontFixPMT(GetPrivateProfileInt(TEXT("Advanced"), TEXT("DontFixPMT"), 0, INI_FILE_NAME) == 0 ? false : true),
	m_IsVGCam(GetPrivateProfileInt(TEXT("Plugins"), TEXT("IsVGCam"), 0, INI_FILE_NAME) == 0 ? false : true),
	m_MaxNumberOfResets(GetPrivateProfileInt(TEXT("Plugins"), TEXT("MaxNumberOfResets"), 10, INI_FILE_NAME) == 0 ? false : true)
{	

	// Buffer
	TCHAR buffer[1024];
	// Context for parsing
	LPTSTR context;

	// Get the list of tuners to exclude
	GetPrivateProfileString(TEXT("Tuning"), TEXT("ExcludeTuners"), TEXT(""), buffer, sizeof(buffer) / sizeof(buffer[0]), INI_FILE_NAME);
	for(LPCTSTR token = _tcstok_s(buffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
		m_ExcludedTuners.insert(_ttoi(token));

	GetPrivateProfileString(TEXT("Tuning"), TEXT("ExcludeTunersMAC"), TEXT(""), buffer, sizeof(buffer) / sizeof(buffer[0]), INI_FILE_NAME);
	for(LPCTSTR token = _tcstok_s(buffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
		m_ExcludedTunersMAC.insert(token);

	// Get the list of DVB-S2 tuners
	GetPrivateProfileString(TEXT("Tuning"), TEXT("DVBS2Tuners"), TEXT(""), buffer, sizeof(buffer) / sizeof(buffer[0]), INI_FILE_NAME);
	for(LPCTSTR token = _tcstok_s(buffer, TEXT(",|"), &context); token != NULL; token = _tcstok_s(NULL, TEXT(",|"), &context))
		m_DVBS2Tuners.insert(_ttoi(token));

	// Get the list of served CAIDs
	GetPrivateProfileString(TEXT("Plugins"), TEXT("ServedCAIDs"), TEXT(""), buffer, sizeof(buffer) / sizeof(buffer[0]), INI_FILE_NAME);
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

	// Get the initial polarization setting
	GetPrivateProfileString(TEXT("Tuning"), TEXT("InitialPolarization"), TEXT("V"), buffer, sizeof(buffer) / sizeof(buffer[0]), INI_FILE_NAME);
	m_InitialPolarization = getPolarizationFromString(buffer);

	// Get the initial modulation setting
	GetPrivateProfileString(TEXT("Tuning"), TEXT("InitialModulation"), TEXT("QPSK"), buffer, sizeof(buffer) / sizeof(buffer[0]), INI_FILE_NAME);
	m_InitialModulation = getModulationFromString(buffer);

	// Get the initial FEC setting
	GetPrivateProfileString(TEXT("Tuning"), TEXT("InitialFEC"), TEXT("3/4"), buffer, sizeof(buffer) / sizeof(buffer[0]), INI_FILE_NAME);
	m_InitialFEC = getFECFromString(buffer);

	// Get DECSA.dll name
	GetPrivateProfileString(TEXT("General"), TEXT("DECSADllName"), TEXT("FFDeCSA_64_MMX.dll"), buffer, sizeof(buffer) / sizeof(buffer[0]), INI_FILE_NAME);
	m_DECSADllName = buffer;

	// Get the preferred audio language
	GetPrivateProfileString(TEXT("Recording"), TEXT("PreferredAudioLanguage"), TEXT("eng"), buffer, sizeof(buffer) / sizeof(buffer[0]), INI_FILE_NAME);
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
