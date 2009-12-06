#pragma once

#include "graph.h"

class Encoder;

class Tuner
{
	friend DWORD WINAPI RunIdleCallback(LPVOID vpTuner);
private:
	// DirectShow graph
	CBDAFilterGraph			m_BDAFilterGraph;
	HANDLE					m_WorkerThread;
	bool					m_IsTunerOK;
	string					m_MAC;
	Encoder* const			m_pEncoder;
	HANDLE					m_InitializationEvent;
	bool					m_LockStatus;
	static int				m_TTBudget2Tuners;
	static int				m_USB2Tuners;

	// We disable unsafe constructors
	Tuner();
	Tuner(const Tuner&);
public:
	Tuner(Encoder* const pEncoder, UINT ordinal, ULONG initialFrequency, ULONG initialSymbolRate, Polarisation initialPolarization, ModulationType initialModulation, BinaryConvolutionCodeRate initialFecRate);
	virtual ~Tuner(void);
	void tune(ULONG frequency, ULONG symbolRate, Polarisation polarization, ModulationType modulation, BinaryConvolutionCodeRate fecRate);
	bool startRecording(bool startFullTransponderDump);
	void stopRecording();
	LPCTSTR getTunerFriendlyName() const			{ return m_BDAFilterGraph.getTunerName(); }
	int getTunerOrdinal() const						{ return m_BDAFilterGraph.getTunerOrdinal(); }
	const string& getTunerMac() const				{ return m_MAC; }
	DVBParser* getParser()							{ return &m_BDAFilterGraph.getParser(); }
	BOOL running()									{ return m_BDAFilterGraph.m_fGraphRunning; }
	void runIdle();
	bool isTunerOK() const							{ return m_IsTunerOK; }
	USHORT getTid() const							{ return m_BDAFilterGraph.m_Tid; }
	void setTid(USHORT tid)							{ m_BDAFilterGraph.m_Tid = tid; }
	HANDLE getInitializationEvent()					{ return m_InitializationEvent; }
	bool getLockStatus();
	void copyProviderDataAndStopRecording();

	// Tuning data getters
	ULONG getFrequency() const						{ return m_BDAFilterGraph.m_ulCarrierFrequency; }
	ULONG getSymbolRate() const						{ return m_BDAFilterGraph.m_ulSymbolRate; }
	Polarisation getPolarization() const			{ return m_BDAFilterGraph.m_SignalPolarisation; }
	ModulationType getModulation() const			{ return m_BDAFilterGraph.m_Modulation; }
	BinaryConvolutionCodeRate getFECRate() const	{ return m_BDAFilterGraph.m_FECRate; }
};
