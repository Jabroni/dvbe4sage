#pragma once

#include "mdapi.h"

using namespace std;
using namespace stdext;

#define PACKET_SIZE	184

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
	virtual bool acceptsCAid(const hash_set<USHORT>& caid) { return true; }
};

class ESCAParser;

class PluginsHandler
{
	friend DWORD WINAPI pluginsHandlerWorkerThreadRoutine(LPVOID param);
private:
	struct Client
	{
		ESCAParser*				caller;
		USHORT					sid;
		USHORT					ecmPid;
		USHORT					emmPid;
		hash_set<USHORT>		caids;
		Client() : caller(NULL), sid(0), ecmPid(0xFFFF), emmPid(0xFFFF) {}
	};

	struct Request
	{
		Client*			client;
		bool			isEcmPacket;
		BYTE			packet[PACKET_SIZE];
		Request() : client(0), isEcmPacket(false) { ZeroMemory(packet, sizeof(packet)); }
	};

	// Global data structures
	list<Plugin>							m_Plugins;
	list<Request>							m_RequestQueue;
	hash_map<ESCAParser*, Client>			m_Clients;
	Client*									m_pCurrentClient;
	USHORT									m_CurrentSid;
	TMDAPIFilterProc						m_CurrentEcmCallback;
	TMDAPIFilterProc						m_CurrentEmmCallback;
	USHORT									m_CurrentEcmFilterId;
	USHORT									m_CurrentEmmFilterId;
	CCritSec								m_cs;
	HANDLE									m_WorkerThread;
	bool									m_DeferTuning;
	bool									m_WaitingForResponse;
	bool									m_IsTuningTimeout;
	bool									m_TimerInitialized;
	time_t									m_Time;
	bool									m_ExitWorkerThread;

	// Callbacks
	void startFilter(LPARAM lParam);
	void stopFilter(LPARAM lParam);
	void dvbCommand(LPARAM lParam);

public:
	// Constructor
	PluginsHandler(HINSTANCE hInstance, HWND hWnd, HMENU hParentMenu);
	// Destructor
	virtual ~PluginsHandler();
	// Window procedure for handling messages
	LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
	// CA packets handler
	void putCAPacket(ESCAParser* caller, bool isEcmPacket, const hash_set<USHORT>& caids, USHORT sid, USHORT caPid, USHORT emmPid, const BYTE* const currentPacket);
	// Routine processing packet queue
	void processECMPacketQueue();
	// Remove obsolete caller
	void removeCaller(ESCAParser* caller);
};
