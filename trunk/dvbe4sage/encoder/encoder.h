#pragma once

#include "NetworkProvider.h"

using namespace std;
using namespace stdext;

class PluginsHandler;
class DVBSFilterGraph;
class DVBFilterToParserProxy;
class DVBSTuner;
class Recorder;
class VirtualTuner;
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
	vector<DVBSTuner*>					m_Tuners;
	vector<VirtualTuner*>				m_VirtualTuners;
	hash_map<SOCKET, VirtualTuner*>		m_VirtualTunersMap;
	hash_multimap<USHORT, Recorder*>	m_Recorders;			// Map from logical tuner to recorder
	CCritSec							m_cs;
	hash_map<SOCKET, Client>			m_Clients;
	const HWND							m_hWnd;
	NetworkProvider						m_Provider;				// Network Provider info - to be copied from one of the tuners

	void socketOperation(SOCKET socket, WORD eventType, WORD error);
	DVBSTuner* getTuner(int tunerOrdinal, bool useLogicalTuner, const Transponder* const pTransponder);

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
						bool bySage,
						bool startFullTransponderDump);
	bool stopRecording(Recorder* recorder);
	bool dumpECMCache(LPCTSTR fileName, LPTSTR reason) const;
	bool loadECMCache(LPCTSTR fileName, LPTSTR reason);
	bool dumpChannels(LPCTSTR fileName, LPTSTR reason) const;
	bool dumpServices(LPCTSTR fileName, LPTSTR reason) const;
	bool dumpTransponders(LPCTSTR fileName, LPTSTR reason) const;
	bool dumpNetworkProvider(LPTSTR reason) const;
	bool loadNetworkProvider(LPTSTR reason) const;
	bool logEPG(LPTSTR reason) const;
	virtual ~Encoder();
	int getNumberOfTuners() const;
	LPCTSTR getTunerFriendlyName(int i) const;
	int getTunerOrdinal(int i) const;
	NetworkProvider& getNetworkProvider()							{ return m_Provider; }
	void waitForFullInitialization();
	bool startRecordingFromFile(LPCWSTR inFileName,
								int sid,
								__int64 duration,
								LPCWSTR outFileName);
};
