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

	// All advanced stuff goes here
	USHORT						m_PMTDilutionFactor;
	USHORT						m_PATDilutionFactor;
	USHORT						m_PMTThreshold;
	USHORT						m_PSIMaturityTime;
	bool						m_IsVGCam;
	USHORT						m_MaxECMCacheSize;
	USHORT						m_ECMCacheAutodeleteChunkSize;
	bool						m_EnableECMCache;
	string						m_BouquetName;
	bool						m_PreferSDOverHD;
	hash_set<USHORT>			m_ExcludedTIDs;
public:
	Configuration();
	
	SetupType getSetupType() const						{ return m_SetupType; }
	bool excludeTuner(UINT tunerOrdinal) const			{ return m_ExcludedTuners.count(tunerOrdinal) > 0; }
	bool excludeTunersByMAC(const string mac) const		{ return m_ExcludedTunersMAC.count(mac) > 0 ; }
	bool includeTuner(UINT tunerOrdinal) const			{ return m_IncludedTuners.count(tunerOrdinal) > 0; }
	bool includeTunersByMAC(const string mac) const		{ return m_IncludedTunersMAC.count(mac) > 0 ; }
	bool includeTuners() const							{ return !m_IncludedTuners.empty() || !m_IncludedTunersMAC.empty(); }
	bool isDVBS2Tuner(UINT tunerOrdinal) const			{ return m_DVBS2Tuners.count(tunerOrdinal) > 0; }
	bool isCAIDServed(USHORT caid) const				{ return m_ServedCAIDs.empty() || m_ServedCAIDs.count(caid) > 0; }
	bool isPROVIDServed(UINT provid) const				{ return m_ServededProvIds.empty() || m_ServededProvIds.count(provid) > 0; }
	bool isVGCam() const								{ return m_IsVGCam; };
	bool scanAllTransponders() const					{ return m_ScanAllTransponders; }
	bool isNorthAmerica() const							{ return m_IsNorthAmerica; };
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

	// All advanced stuff goes here
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
};

extern Configuration* g_pConfiguration;