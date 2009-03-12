// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the ENCODER_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// ENCODER_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#pragma once

#ifdef ENCODER_EXPORTS
#define ENCODER_API __declspec(dllexport)
#else
#define ENCODER_API __declspec(dllimport)
#endif

#pragma warning(push)
#pragma warning(disable:4251)

using namespace std;
using namespace stdext;

class PluginsHandler;
class CBDAFilterGraph;
class DVBFilterToParserProxy;
class Tuner;
class Recorder;
class VirtualTuner;
class DVBParser;

// This class is exported from the encoder.dll
class ENCODER_API Encoder
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

	void socketOperation(SOCKET socket, WORD eventType, WORD error);
	Tuner* getTuner(int tunerOrdinal, bool useLogicalTuner, int channel, bool useSid);

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
};

#pragma warning(pop)
