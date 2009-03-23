#pragma once

#include "logger.h"

using namespace std;
using namespace stdext;

class PluginsHandler;
class CBDAFilterGraph;
class DVBFilterToParserProxy;
class Tuner;
class Recorder;
class VirtualTuner;
class DVBParser;
struct Transponder;

class Encoder
{
	struct Client
	{
		SOCKET							socket;
		string							lastFileName;
		VirtualTuner*					virtualTuner;
	};

	PluginsHandler*						m_pPluginsHandler;
	vector<Tuner*>						m_Tuners;
	vector<VirtualTuner*>				m_VirtualTuners;
	hash_map<SOCKET, VirtualTuner*>		m_VirtualTunersMap;
	hash_multimap<USHORT, Recorder*>	m_Recorders;			// Map from logical tuner to recorder
	CCritSec							m_cs;
	hash_map<SOCKET, Client>			m_Clients;
	const HWND							m_hWnd;
	DVBParser*							m_pParser;				// DVBParser - to be copied from one of the tuners
	Logger								m_Logger;

	void socketOperation(SOCKET socket, WORD eventType, WORD error);
	Tuner* getTuner(int tunerOrdinal, bool useLogicalTuner, const Transponder* const pTransponder);

public:
	Encoder(HINSTANCE hInstance, HWND hWnd, HMENU hParentMenu);
	LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
	bool startRecording(bool autodiscoverTransponder,
						ULONG frequency,
						ULONG symbolRate,
						Polarisation polarization,
						ModulationType modulation,
						BinaryConvolutionCodeRate fecRate,
						int tunerOrdinal,
						bool useLogicalTuner,
						int channel,
						bool useSid,
						__int64 duration,
						LPCWSTR outFileName,
						VirtualTuner* virtualTuner,
						__int64 size,
						bool bySage);
	bool stopRecording(Recorder* recorder);
	virtual ~Encoder();
	int getNumberOfTuners() const;
	LPCTSTR getTunerFriendlyName(int i) const;
	int getTunerOrdinal(int i) const;
	DVBParser* getParser()								{ return m_pParser; }
	void waitForFullInitialization();
	void valog(UINT logLevel, bool timeStamp, LPCTSTR format, va_list argList)
	{
		m_Logger.valog(logLevel, timeStamp, format, argList);
	}
	LPCTSTR getLogFileName() const						{ return m_Logger.getLogFileName(); }
};
