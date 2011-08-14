#pragma once

using namespace std;
using namespace stdext;

enum SetupType {SETUP_DVBS, SETUP_DVBT, SETUP_DVBC };

class Configuration
{
	SetupType					m_SetupType;
	UINT						m_LogLevel;
	UINT						m_DCWTimeout;
	UINT						m_TSPacketsPerBuffer;
	UINT						m_NumberOfBuffers;
	UINT						m_TSPacketsPerOutputBuffer;
	UINT						m_TSPacketsOutputThreshold;
	UINT						m_TuningTimeout;
	UINT						m_TuningLockTimeout;
	USHORT						m_ListeningPort;
	hash_map<int, ULONG>						m_InitialFrequency;
	hash_map<int, ULONG>						m_InitialSymbolRate;
	hash_map<int, Polarisation>					m_InitialPolarization;
	hash_map<int, ModulationType>				m_InitialModulation;
	hash_map<int, BinaryConvolutionCodeRate>	m_InitialFEC;
	UINT						m_InitialRunningTime;
	bool						m_UseSidForTuning;
	USHORT						m_NumberOfVirtualTuners;
	hash_set<UINT>				m_ExcludedTuners;
	hash_set<UINT>				m_IncludedTuners;
	hash_set<string>			m_ExcludedTunersMAC;
	hash_set<string>			m_IncludedTunersMAC;
	hash_set<USHORT>			m_IncludedONIDs;
	hash_set<USHORT>			m_ExcludedONIDs;
	hash_set<UINT>			m_ExcludedBouquets;
	bool						m_ScanAllTransponders;

	hash_set<UINT>				m_DVBS2Tuners;
	bool						m_DisableWriteBuffering;
	string						m_PreferredAudioLanguage;
	string						m_PreferredAudioFormat;
	string						m_PreferredSubtitlesLanguage;
	ULONG						m_LNBSW;
	ULONG						m_LNBLOF1;
	ULONG						m_LNBLOF2;
	bool						m_UseNewTuningMethod;
	hash_set<USHORT>			m_ServedCAIDs;
	hash_set<UINT>				m_ServededProvIds;
	USHORT						m_MaxNumberOfResets;
	bool						m_IsNorthAmerica;
	bool						m_UseDiseqc;
	bool						m_AutoDiscoverONID;
	bool						m_CacheServices;
	bool						m_TrustKeys;
	bool						m_GrowlNotification;
	bool						m_EpgCollection;
	

	// Growl variables
	string						m_GrowlIP;
	string						m_GrowlPassword;
	bool						m_GrowlNotifyOnNewSID;
	bool						m_GrowlNotifyOnNewBouquetCh;
	bool						m_GrowlNotifyOnTune;
	bool						m_GrowlNotifyOnRecordFailure;


	// All advanced stuff goes here
	string						m_InstanceName;
	USHORT						m_PMTDilutionFactor;
	USHORT						m_PATDilutionFactor;
	USHORT						m_PMTThreshold;
	USHORT						m_PSIMaturityTime;
	bool						m_IsVGCam;
	bool						m_SendAllECMs;
	USHORT						m_MaxECMCacheSize;
	USHORT						m_ECMCacheAutodeleteChunkSize;
	bool						m_EnableECMCache;
	string						m_BouquetName;
	bool						m_PreferSDOverHD;
	hash_set<USHORT>			m_ExcludedTIDs;
	hash_set<USHORT>			m_PreferredTIDs;
public:
	Configuration();
	
	SetupType getSetupType() const						{ return m_SetupType; }
	bool excludeTuner(UINT tunerOrdinal) const			{ return m_ExcludedTuners.count(tunerOrdinal) > 0; }
	bool excludeTunersByMAC(const string mac) const		{ return m_ExcludedTunersMAC.count(mac) > 0 ; }
	bool includeTuner(UINT tunerOrdinal) const			{ return m_IncludedTuners.count(tunerOrdinal) > 0; }
	bool excludeBouquet(UINT bouquetId) const			{ return m_ExcludedBouquets.count(bouquetId) > 0 ; }
	bool includeTunersByMAC(const string mac) const		{ return m_IncludedTunersMAC.count(mac) > 0 ; }
	bool includeTuners() const							{ return !m_IncludedTuners.empty() || !m_IncludedTunersMAC.empty(); }
	bool includeONID(const USHORT onid) const			{ return m_IncludedONIDs.count(onid) > 0 ; }
	bool excludeONID(const USHORT onid) const			{ return m_ExcludedONIDs.count(onid) > 0 ; }
	bool includeONIDs() const							{ return !m_IncludedONIDs.empty(); }
	bool isDVBS2Tuner(UINT tunerOrdinal) const			{ return m_DVBS2Tuners.count(tunerOrdinal) > 0; }
	bool isCAIDServed(USHORT caid) const				{ return m_ServedCAIDs.empty() || m_ServedCAIDs.count(caid) > 0; }
	bool isPROVIDServed(UINT provid) const				{ return m_ServededProvIds.empty() || m_ServededProvIds.count(provid) > 0; }
	bool isVGCam() const								{ return m_IsVGCam; };
	bool sendAllECMs() const							{ return m_SendAllECMs; };
	bool scanAllTransponders() const					{ return m_ScanAllTransponders; }
	bool isNorthAmerica() const							{ return m_IsNorthAmerica; };
	bool trustKeys() const								{ return m_TrustKeys; };
	UINT getLogLevel() const							{ return m_LogLevel; }
	UINT getDCWTimeout() const							{ return m_DCWTimeout; }
	UINT getTSPacketsPerBuffer() const					{ return m_TSPacketsPerBuffer; }
	UINT getNumberOfBuffers() const						{ return m_NumberOfBuffers; }
	UINT getTSPacketsPerOutputBuffer() const			{ return m_TSPacketsPerOutputBuffer; }
	UINT getTSPacketsOutputThreshold() const			{ return m_TSPacketsOutputThreshold; }
	UINT getTuningTimeout() const						{ return m_TuningTimeout; }
	UINT getTuningLockTimeout() const					{ return m_TuningLockTimeout; }
	USHORT getListeningPort() const						{ return m_ListeningPort; }
	ULONG getInitialFrequency(int onidIndex = 0) const;
	ULONG getInitialSymbolRate(int onidIndex = 0) const;
	Polarisation getInitialPolarization(int onidIndex = 0) const;
	ModulationType getInitialModulation(int onidIndex = 0) const;
	BinaryConvolutionCodeRate getInitialFEC(int onidIndex = 0) const;
	UINT getInitialRunningTime() const					{ return m_InitialRunningTime; }
	bool getUseSidForTuning() const						{ return m_UseSidForTuning; }
	USHORT getNumberOfVirtualTuners() const				{ return m_NumberOfVirtualTuners; }
	bool getDisableWriteBuffering() const				{ return m_DisableWriteBuffering; }
	const string& getPreferredAudioLanguage() const		{ return m_PreferredAudioLanguage; }
	const string& getPreferredAudioFormat() const		{ return m_PreferredAudioFormat; }
	const string& getPreferredSubtitlesLanguage() const	{ return m_PreferredSubtitlesLanguage; }
	ULONG getLNBSW() const								{ return m_LNBSW; }
	ULONG getLNBLOF1() const							{ return m_LNBLOF1; }
	ULONG getLNBLOF2() const							{ return m_LNBLOF2; }
	void setLNBSW(ULONG val)							{ m_LNBSW = val; }
	void setLNBLOF1(ULONG val)							{ m_LNBLOF1 = val; }
	void setLNBLOF2(ULONG val)							{ m_LNBLOF2 = val; }
	bool getUseNewTuningMethod() const					{ return m_UseNewTuningMethod; }
	USHORT getMaxNumberOfResets() const					{ return m_MaxNumberOfResets; }
	bool getUseDiseqc() const							{ return m_UseDiseqc; };
	bool getAutoDiscoverONID() const					{ return m_AutoDiscoverONID; };
	bool getCacheServices() const						{ return m_CacheServices; };
	bool getGrowlNotification() const					{ return m_GrowlNotification; };
	bool getEpgCollection() const						{ return m_EpgCollection; };
	const string& getGrowlIP() const					{ return m_GrowlIP; };
	const string& getGrowlPassword() const				{ return m_GrowlPassword; };
	bool getGrowlNotifyOnNewSID() const					{ return m_GrowlNotifyOnNewSID; };
	bool getGrowlNotifyOnNewBouqeuet() const			{ return m_GrowlNotifyOnNewBouquetCh; };
	bool getGrowlNotifyOnTune() const					{ return m_GrowlNotifyOnTune; };
	bool getGrowlNotifyOnRecordFailure() const			{ return m_GrowlNotifyOnRecordFailure; };

	// All advanced stuff goes here
	const string& getInstanceName() const						{ return m_InstanceName; }
	USHORT getPMTDilutionFactor() const					{ return m_PMTDilutionFactor; }
	USHORT getPATDilutionFactor() const					{ return m_PATDilutionFactor; }
	USHORT getPMTThreshold() const						{ return m_PMTThreshold; }
	USHORT getPSIMaturityTime() const					{ return m_PSIMaturityTime; }
	USHORT getMaxECMCacheSize() const					{ return m_MaxECMCacheSize; }
	USHORT getECMCacheAutodeleteChunkSize() const		{ return m_ECMCacheAutodeleteChunkSize; }
	bool getEnableECMCache() const						{ return m_EnableECMCache; }
	const string& getBouquetName() const				{ return m_BouquetName; }
	bool getPreferSDOverHD() const						{ return m_PreferSDOverHD; }
	bool isExcludedTID(USHORT tid) const				{ return m_ExcludedTIDs.count(tid) > 0; }
	bool isPreferredTID(USHORT tid) const				{ return m_PreferredTIDs.count(tid) > 0; }
};

extern Configuration* g_pConfiguration;