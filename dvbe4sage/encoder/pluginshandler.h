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
	/*bool operator ==(const CAScheme& other) const
	{
		return pid == other.pid && provId == other.provId && caId == other.caId;
	}

	bool operator <(const CAScheme& other) const
	{
		return caId < other.caId;
	}*/

	operator size_t() const
	{
		return (size_t)caId;
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
		Request() : client(0) { ZeroMemory(packet, sizeof(packet)); }
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

	// Key checker
	static bool wrong(const Dcw& dcw);

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
