#pragma once

#include "GenericSource.h"
#include "graph.h"

class Encoder;

class DVBSTuner : public GenericSource
{
	friend DWORD WINAPI RunIdleCallback(LPVOID vpTuner);
private:
	HANDLE					m_WorkerThread;
	string					m_MAC;
	Encoder* const			m_pEncoder;
	HANDLE					m_InitializationEvent;
	bool					m_LockStatus;
	static int				m_TTBudget2Tuners;
	static int				m_USB2Tuners;
	USHORT					m_TID;
	USHORT					m_ONID;

	// We disable unsafe constructors
	DVBSTuner();
	DVBSTuner(const DVBSTuner&);
public:
	DVBSTuner(Encoder* const pEncoder,
			  UINT ordinal,
			  ULONG initialFrequency,
			  ULONG initialSymbolRate,
			  Polarisation initialPolarization,
			  ModulationType initialModulation,
			  BinaryConvolutionCodeRate initialFecRate);
	virtual ~DVBSTuner(void);

	virtual bool startPlayback(bool startFullTransponderDump);
	virtual LPCTSTR getSourceFriendlyName() const	{ return ((DVBFilterGraph*)m_pFilterGraph)->getTunerName(); }
	virtual bool getLockStatus();

	const string& getTunerMac() const				{ return m_MAC; }
	void tune(ULONG frequency, ULONG symbolRate, Polarisation polarization, ModulationType modulation, BinaryConvolutionCodeRate fecRate);
	void runIdle();
	USHORT getTID() const							{ return m_TID; }
	void setTID(USHORT tid)							{ m_TID = tid; }
	USHORT getONID() const							{ return m_ONID; }
	void setONID(USHORT onid)						{ m_ONID = onid; }
	HANDLE getInitializationEvent()					{ return m_InitializationEvent; }
	void copyProviderDataAndStopRecording();

	// Tuning data getters
	ULONG getFrequency() const						{ return ((DVBFilterGraph*)m_pFilterGraph)->m_ulCarrierFrequency; }
	ULONG getSymbolRate() const						{ return ((DVBFilterGraph*)m_pFilterGraph)->m_ulSymbolRate; }
	Polarisation getPolarization() const			{ return ((DVBFilterGraph*)m_pFilterGraph)->m_SignalPolarisation; }
	ModulationType getModulation() const			{ return ((DVBFilterGraph*)m_pFilterGraph)->m_Modulation; }
	BinaryConvolutionCodeRate getFECRate() const	{ return ((DVBFilterGraph*)m_pFilterGraph)->m_FECRate; }
};
