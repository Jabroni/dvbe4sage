#pragma once

#include "graph.h"

class Tuner
{
	friend VOID NTAPI RunIdleCallback(PVOID vpTuner, BOOLEAN alwaysTrue);
private:
	// DirectShow graph
	CBDAFilterGraph			m_BDAFilterGraph;
	// True if the card is Twinhan
	bool					m_isTwinhan;
	bool					m_AutodiscoverTransponder;
	HANDLE					m_TimerQueue;
	HANDLE					m_IdleTimer;
	bool					m_IsTunerOK;

	// We disable unsafe constructors
	Tuner();
	Tuner(Tuner&);
public:
	Tuner(int ordinal, ULONG initialFrequency, ULONG initialSymbolRate, Polarisation initialPolarization, ModulationType initialModulation, BinaryConvolutionCodeRate initialFecRate);
	virtual ~Tuner(void);
	bool tune(ULONG frequency, ULONG symbolRate, Polarisation polarization, ModulationType modulation, BinaryConvolutionCodeRate fecRate);
	bool startRecording(bool bTunerUsed, bool autodiscoverTransponder);
	void stopRecording();
	LPCTSTR getTunerFriendlyName() const { return m_BDAFilterGraph.getTunerName(); }
	int getTunerOrdinal() const { return m_BDAFilterGraph.getTunerOrdinal(); }
	DVBParser* getParser() { return &m_BDAFilterGraph.getParser(); }
	BOOL running() { return m_BDAFilterGraph.m_fGraphRunning; }
	bool tuneChannel(int channelNumber, bool useSid, USHORT& sid) { return m_BDAFilterGraph.tuneChannel(channelNumber, useSid, sid, m_AutodiscoverTransponder); }
	bool runIdle();
	bool isTunerOK() const { return m_IsTunerOK; }
	USHORT getTid() const { return m_BDAFilterGraph.m_Tid; }
};
