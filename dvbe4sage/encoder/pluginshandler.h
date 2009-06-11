#pragma once

#include "mdapi.h"

using namespace std;
using namespace stdext;

#define PACKET_SIZE	184

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

class Plugin
{
	friend class PluginsHandler;
	HMODULE								m_hModule;
	HMENU								m_hMenu;
	Plugin_Init_Proc					m_fpInit;
	Plugin_Channel_Change_Proc			m_fpChannelChange;
	Plugin_Exit_Proc					m_fpExit;
	Plugin_Hot_Key_Proc					m_fpHotKey;
	Plugin_OSD_Key_Proc					m_fpOSDKey;
	Plugin_Menu_Cmd_Proc				m_fpMenuCommand;
	Plugin_Send_Dll_ID_Name_Proc		m_fpGetPluginName;
	Plugin_Filter_Close_Proc			m_fpFilterClose;
	Plugin_Extern_RecPlay_Proc			m_fpExternRecPlay;
public:
	virtual bool acceptsCAid(const hash_set<CAScheme>& caid) { return true; }
};

class ESCAParser;

enum CAPacketType { TYPE_ECM, TYPE_EMM, TYPE_PMT, TYPE_CAT };

class PluginsHandler
{
	friend DWORD WINAPI pluginsHandlerWorkerThreadRoutine(LPVOID param);
private:
	struct Client
	{
		ESCAParser*						caller;
		LPCTSTR							channelName;
		USHORT							sid;
		USHORT							ecmPid;
		USHORT							pmtPid;
		hash_set<CAScheme>				ecmCaids;
		EMMInfo							emmCaids;
		Client() : caller(NULL), sid(0), ecmPid(0xFFFF) {}
	};

	struct Request
	{
		Client*			client;
		BYTE			packet[PACKET_SIZE];
		Request() : client(0) { ZeroMemory(packet, sizeof(packet)); }
	};

	// Global data structures
	list<Plugin>							m_Plugins;
	list<Request>							m_RequestQueue;
	hash_map<ESCAParser*, Client>			m_Clients;
	Client*									m_pCurrentClient;
	USHORT									m_CurrentSid;
	TMDAPIFilterProc						m_CurrentEcmCallback;
	TMDAPIFilterProc						m_CurrentEmmCallback;
	TMDAPIFilterProc						m_CurrentPmtCallback;
	TMDAPIFilterProc						m_CurrentCatCallback;
	USHORT									m_CurrentEcmFilterId;
	USHORT									m_CurrentEmmFilterId;
	USHORT									m_CurrentPmtFilterId;
	USHORT									m_CurrentCatFilterId;
	CCritSec								m_cs;
	HANDLE									m_WorkerThread;
	bool									m_DeferTuning;
	bool									m_WaitingForResponse;
	bool									m_IsTuningTimeout;
	bool									m_TimerInitialized;
	time_t									m_Time;
	bool									m_ExitWorkerThread;

	// Fill the TP structure function
	void fillTPStructure(LPTPROGRAM82 tp) const;

	// Callbacks
	void startFilter(LPARAM lParam);
	void stopFilter(LPARAM lParam);
	void dvbCommand(LPARAM lParam);

	union Dcw
	{
		BYTE key[8];
		__int64 number;
	};

	// Key checker
	static bool wrong(const Dcw& dcw);

public:
	// Constructor
	PluginsHandler(HINSTANCE hInstance, HWND hWnd, HMENU hParentMenu);
	// Destructor
	virtual ~PluginsHandler();
	// Window procedure for handling messages
	LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
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
	void processECMPacketQueue();
	// Remove obsolete caller
	void removeCaller(ESCAParser* caller);
};
