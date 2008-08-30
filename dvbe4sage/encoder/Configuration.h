#pragma once

using namespace std;
using namespace stdext;

class Configuration
{
	const UINT					m_LogLevel;
	const UINT					m_DCWTimeout;
	const UINT					m_TSPacketsPerBuffer;
	const UINT					m_NumberOfBuffers;
	const UINT					m_TSPacketsPerOutputBuffer;
	const UINT					m_TSPacketsOutputThreshold;
	const UINT					m_TuningTimeout;
	const USHORT				m_ListeningPort;
	const ULONG					m_InitialFrequency;
	const ULONG					m_InitialSymbolRate;
	Polarisation				m_InitialPolarization;
	ModulationType				m_InitialModulation;
	BinaryConvolutionCodeRate	m_InitialFEC;
	const UINT					m_InitialRunningTime;
	const USHORT				m_EMMPid;
	const bool					m_UseSidForTuning;
	const USHORT				m_NumberOfVirtualTuners;
	hash_set<int>				m_ExcludeTuners;
	hash_set<int>				m_DVBS2Tuners;
	string						m_DECSADllName;
	const bool					m_DisableWriteBuffering;
public:
	Configuration();
	
	bool excludeTuner(int tunerOrdinal) const		{ return m_ExcludeTuners.count(tunerOrdinal) > 0; }
	bool isDVBS2Tuner(int tunerOrdinal) const		{ return m_DVBS2Tuners.count(tunerOrdinal) > 0; }
	UINT getLogLevel() const						{ return m_LogLevel; }
	UINT getDCWTimeout() const						{ return m_DCWTimeout; }
	UINT getTSPacketsPerBuffer() const				{ return m_TSPacketsPerBuffer; }
	UINT getNumberOfBuffers() const					{ return m_NumberOfBuffers; }
	UINT getTSPacketsPerOutputBuffer() const		{ return m_TSPacketsPerOutputBuffer; }
	UINT getTSPacketsOutputThreshold() const		{ return m_TSPacketsOutputThreshold; }
	UINT getTuningTimeout() const					{ return m_TuningTimeout; }
	USHORT getListeningPort() const					{ return m_ListeningPort; }
	ULONG getInitialFrequency() const				{ return m_InitialFrequency; }
	ULONG getInitialSymbolRate() const				{ return m_InitialSymbolRate; }
	Polarisation getInitialPolarization() const		{ return m_InitialPolarization; }
	ModulationType getInitialModulation() const		{ return m_InitialModulation; }
	BinaryConvolutionCodeRate getInitialFEC() const	{ return m_InitialFEC; }
	UINT getInitialRunningTime() const				{ return m_InitialRunningTime; }
	USHORT getEMMPid() const						{ return m_EMMPid; }
	bool getUseSidForTuning() const					{ return m_UseSidForTuning; }
	USHORT getNumberOfVirtualTuners() const			{ return m_NumberOfVirtualTuners; }
	LPCTSTR getDECSADllName() const					{ return m_DECSADllName.c_str(); }
	bool getDisableWriteBuffering() const			{ return m_DisableWriteBuffering; }
};

extern Configuration g_Configuration;