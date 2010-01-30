#pragma once

#include "mdapi.h"
#include "ecmcache.h"

using namespace std;
using namespace stdext;

// CAID/PROVID structure
struct CAScheme
{
	USHORT								pid;
	DWORD								provId;
	WORD								caId;

	operator size_t() const
	{
		return (size_t)(provId << 16) | (size_t)caId;
	}
};

class EMMInfo : public hash_set<CAScheme>
{
public:
	bool hasPid(USHORT pid) const;
};

class ESCAParser;

enum CAPacketType { TYPE_ECM, TYPE_EMM, TYPE_PMT, TYPE_CAT };

class PluginsHandler
{
	friend DWORD WINAPI pluginsHandlerWorkerThreadRoutine(LPVOID param);
protected:
	struct Client
	{
		ESCAParser*						caller;
		LPCTSTR							channelName;
		USHORT							sid;
		USHORT							ecmPid;
		USHORT							pmtPid;
		hash_set<CAScheme>				ecmCaids;
		EMMInfo							emmCaids;
		BYTE							ecmPacket[PACKET_SIZE];
		Client() : caller(NULL), sid(0), ecmPid(0xFFFF) {}
	};

	struct Request
	{
		Client*			client;
		BYTE			packet[PACKET_SIZE];
		Request() : client(0) { memset(packet, (int)'\xFF', sizeof(packet)); }
	};

	struct EMM
	{
		BYTE		packet[PACKET_SIZE];
		time_t		timeStamp;
		EMM() : timeStamp(0) { memset(packet, (int)'\xFF', sizeof(packet)); }
		EMM(const EMM& other) : timeStamp(other.timeStamp)
		{
			memcpy(packet, other.packet, (size_t)other.packet[3] + 4);
		}
	};

	// Global data structures
	list<Request>							m_RequestQueue;
	hash_map<ESCAParser*, Client>			m_Clients;
	Client*									m_pCurrentClient;
	USHORT									m_CurrentSid;
	CCritSec								m_cs;
	HANDLE									m_WorkerThread;
	bool									m_DeferTuning;
	bool									m_WaitingForResponse;
	bool									m_IsTuningTimeout;
	bool									m_TimerInitialized;
	time_t									m_Time;
	bool									m_ExitWorkerThread;
	ECMCache								m_ECMCache;
	hash_multimap<unsigned __int32, EMM>	m_EMMCache;

	// Key checker
	static bool wrong(const Dcw& dcw);

	bool inEMMCache(const BYTE* const packet, unsigned __int32& newPacketCRC) const;
	void addToEMMCache(const BYTE* const packet, const unsigned __int32 newPacketCRC);

protected:
	virtual void processECMPacket(BYTE* packet) = 0;
	virtual void processEMMPacket(BYTE* packet) = 0;
	virtual void processPMTPacket(BYTE* packet) = 0;
	virtual void processCATPacket(BYTE* packet) = 0;
	virtual void resetProcessState() = 0;
	virtual bool canProcessECM() = 0;
	virtual bool handleTuningRequest()					{ return true; }

	void EndWorkerThread()
	{
		// Shutdown the worker thread 
		// Make worker thread exit
		m_ExitWorkerThread = true;
		// Wait for it to exit
		WaitForSingleObject(m_WorkerThread, INFINITE);
		// And close its handle
		CloseHandle(m_WorkerThread);
	}
	
	void ECMRequestComplete(Client* client, const BYTE* ecmPacket, const Dcw& dcw, bool isOddKey, bool fromPlugin);

public:
	// Constructor
	PluginsHandler();
	// Destructor
	virtual ~PluginsHandler();
	// Window procedure for handling messages
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
	{
		return 0;
	}

	// CA packets handler
	void putCAPacket(ESCAParser* caller,
					 CAPacketType packetType,
					 const hash_set<CAScheme>& ecmCaids,
					 const EMMInfo& emmCaids,
					 USHORT sid,
					 LPCTSTR channelName,
					 USHORT caPid,
					 USHORT pmtPid,
					 const BYTE* const currentPacket);
	// Routine processing packet queue
	bool processECMPacketQueue();
	// Remove obsolete caller
	void removeCaller(ESCAParser* caller);
	bool dumpECMCache(LPCTSTR fileName, LPTSTR reason) const		{ return m_ECMCache.DumpToFile(fileName, reason); }
	bool loadECMCache(LPCTSTR fileName, LPTSTR reason)				{ return m_ECMCache.ReadFromFile(fileName, reason); }
};
