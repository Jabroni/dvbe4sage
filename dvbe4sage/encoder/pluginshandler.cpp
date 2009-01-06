// pluginshandler.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "resource.h"
#include "pluginshandler.h"
#include "dvbparser.h"
#include "logger.h"
#include "configuration.h"

// This is the worker thread main function waits for events
DWORD WINAPI pluginsHandlerWorkerThreadRoutine(LPVOID param)
{
	// Get the pointer to the parser from the parameter
	PluginsHandler* const pluginsHandler = (PluginsHandler* const)param;
	// Do it while not told to exit
	bool shouldExit = false;
	while(!shouldExit)
	{
		// Sleep for some time
		Sleep(100);
		// Do this in critical section
		CAutoLock lock(&(pluginsHandler->m_cs));
		// See if we need to terminate the thread
		if(pluginsHandler->m_ExitWorkerThread)
			shouldExit = true;
		else
			// Process packets pending plugins attention
			pluginsHandler->processECMPacketQueue();
	}
	// Exit
	return 0;
}

// This is the constructor of a class that has been exported.
// see pluginshandler.h for the class definition
PluginsHandler::PluginsHandler(HINSTANCE hInstance,
							   HWND hWnd,
							   HMENU hParentMenu) :
	m_pCurrentClient(NULL),
	m_CurrentEcmCallback(NULL),
	m_CurrentEmmCallback(NULL),
	m_CurrentEcmFilterId(0),
	m_CurrentEmmFilterId(0),
	m_CurrentSid(0),
	m_DeferTuning(false),
	m_WaitingForResponse(false),
	m_TimerInitialized(false),
	m_IsTuningTimeout(false),
	m_ExitWorkerThread(false)
{
	// We look for plugins under "Plugins" directory of the CWD
	_tfinddata_t fileInfo;
	intptr_t handle = _tfindfirst(TEXT(".\\Plugins\\*.dll"), &fileInfo);
	// If there are any DLLs there
	if(handle != -1)
	{
		// Load "Plugins" menu from our own resource
		HMENU hPluginsMenu = LoadMenu(GetModuleHandle(TEXT("encoder")), MAKEINTRESOURCE(IDR_PLUGINS_MENU));
		// Append it to the main menu of the dialog
		AppendMenu(hParentMenu, MF_POPUP, (UINT_PTR)hPluginsMenu, TEXT("&Plugins"));
		// Remove the first entry "Plugins" from there
		RemoveMenu(hPluginsMenu, 0, MF_REMOVE);
		// We need to count plugins for proper initialization
		int counter = 0;
		do
		{
			// Load the plugin DLL itself
			HMODULE hModule = LoadLibrary((string(TEXT(".\\Plugins\\")) + fileInfo.name).c_str());
			// Check for success
			if(hModule != NULL)
			{
				// Create and initialize the new plugin structure
				Plugin plugin;
				plugin.m_hModule = hModule;
				plugin.m_hMenu = LoadMenu(hModule, TEXT("EXTERN"));
				plugin.m_fpInit = (Plugin_Init_Proc)GetProcAddress(hModule, TEXT("On_Start"));
				plugin.m_fpChannelChange = (Plugin_Channel_Change_Proc)GetProcAddress(hModule, TEXT("On_Channel_Change"));
				plugin.m_fpExit = (Plugin_Exit_Proc)GetProcAddress(hModule, TEXT("On_Exit"));
				plugin.m_fpHotKey = (Plugin_Hot_Key_Proc)GetProcAddress(hModule, TEXT("On_Hot_Key"));
				plugin.m_fpOSDKey = (Plugin_OSD_Key_Proc)GetProcAddress(hModule, TEXT("On_Osd_Key"));
				plugin.m_fpMenuCommand = (Plugin_Menu_Cmd_Proc)GetProcAddress(hModule, TEXT("On_Menu_Select"));
				plugin.m_fpGetPluginName = (Plugin_Send_Dll_ID_Name_Proc)GetProcAddress(hModule, TEXT("On_Send_Dll_ID_Name"));
				plugin.m_fpFilterClose = (Plugin_Filter_Close_Proc)GetProcAddress(hModule, TEXT("On_Filter_Close"));
				plugin.m_fpExternRecPlay = (Plugin_Extern_RecPlay_Proc)GetProcAddress(hModule, TEXT("On_Rec_Play"));

				// Only if all components present
				if(plugin.m_hMenu != NULL && plugin.m_fpInit != NULL && plugin.m_fpChannelChange != NULL && plugin.m_fpExit != NULL &&
					/*plugin.m_fpHotKey != NULL && plugin.m_fpOSDKey != NULL &&*/ plugin.m_fpMenuCommand != NULL && plugin.m_fpGetPluginName != NULL /*&&
					plugin.m_fpFilterClose != NULL && plugin.m_fpExternRecPlay != NULL*/)
				{
					// Get plugin name
					char name[30];
					plugin.m_fpGetPluginName(name);

					// Initialize it
					char hotKey;
					char* MDAPIVersion = "MD-API Version 01.03 - 01.06";
					int keepRunning = 1;
					plugin.m_fpInit(hInstance, hWnd, FALSE, counter, &hotKey, MDAPIVersion, &keepRunning);

					// Check if it keeps running
					if(keepRunning == -999)
						// If no, free its DLL
						FreeLibrary(hModule);
					else
					{
						// Append it menu to the "Plugins" menu
						AppendMenu(hPluginsMenu, MF_POPUP, (UINT_PTR)plugin.m_hMenu, name);

						// Store the plugin in the internal list
						m_Plugins.push_back(plugin);

						// And increase the counter
						counter++;
					}
				}
			}
		}
		// Do this for all DLLs
		while(_tfindnext(handle, &fileInfo) == 0);
		_findclose(handle);
	}

	// Create worker thread
	m_WorkerThread = CreateThread(NULL, 0, pluginsHandlerWorkerThreadRoutine, this, 0, NULL);

	return;
}


// Cleanup plugins
PluginsHandler::~PluginsHandler()
{
	// Shutdown the worker thread 
	// Make worker thread exit
	m_ExitWorkerThread = true;
	// Wait for it to exit
	WaitForSingleObject(m_WorkerThread, INFINITE);
	// And close its handle
	CloseHandle(m_WorkerThread);

	// Do this in the critical section, just in case some threads still do something with this object
	CAutoLock lock(&m_cs);

	// For all loaded plugins
	while(!m_Plugins.empty())
	{
		// Get the one from the back
		Plugin& plugin = m_Plugins.back();
		// Perform grace exit on it
		plugin.m_fpExit();
		// Free its library
		FreeLibrary(plugin.m_hModule);
		// And remove it from the list
		m_Plugins.pop_back();
	}

	// Remove everything from the requests queue
	m_RequestQueue.clear();

	// Remove all the clients
	m_Clients.clear();

	// Nullify everything else
	m_pCurrentClient = NULL;
	m_CurrentEcmCallback = NULL;
	m_CurrentEmmCallback = NULL;
	m_CurrentEcmFilterId = 0;
	m_CurrentEmmFilterId = 0;
}

// Message handler
LRESULT PluginsHandler::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	// Handle message
	switch(message)
	{
		case WM_COMMAND:
		{
			// Handle menu clicks
			int wmId  = LOWORD(wParam);
			for(list<Plugin>::iterator it = m_Plugins.begin(); it != m_Plugins.end(); it++)
				it->m_fpMenuCommand(wmId);
			break;
		}
		case WM_USER:
			// Handle callbacks from the plugins
			switch(wParam)
			{
				case MDAPI_START_FILTER:
					startFilter(lParam);
					break;
				case MDAPI_STOP_FILTER:
					stopFilter(lParam);
					break;
				case MDAPI_DVB_COMMAND:
					dvbCommand(lParam);
					break;
				default:
					break;
			}
		default:
			break;
	}
	return 0;
}

// This is the callback called for each newly started filter
void PluginsHandler::startFilter(LPARAM lParam)
{
	// Do this in critical section
	CAutoLock lock(&m_cs);

	// Get the start filter structure from the lParam
	TSTART_FILTER* startFilter = (TSTART_FILTER*)lParam;
	// We really can do anything with it only when there is current client
	if(m_pCurrentClient != NULL)
	{
		// Initialize callback and ID for ECM/EMM respectively
		if(m_pCurrentClient->ecmPid == startFilter->Pid)
		{
			m_CurrentEcmCallback = (TMDAPIFilterProc)startFilter->Irq_Call_Adresse;
			m_CurrentEcmFilterId = startFilter->Filter_ID;
			g_Logger.log(2, true, TEXT("Start ECM filter for SID=%hu received\n"), m_CurrentSid);
		}
		else if(m_pCurrentClient->emmPid == startFilter->Pid)
		{
			m_CurrentEmmCallback = (TMDAPIFilterProc)startFilter->Irq_Call_Adresse;
			m_CurrentEmmFilterId = startFilter->Filter_ID;
			g_Logger.log(2, true, TEXT("Start EMM filter for SID=%hu received\n"), m_CurrentSid);
		}
		// Put the callback address into filter's running ID
		startFilter->Running_ID = (UINT)(startFilter->Irq_Call_Adresse);
	}
	else
		g_Logger.log(0, true, TEXT("Catastrophic error, no current client for a filter!\n"));
}

// This is the callback called for each filter about to be stopped
void PluginsHandler::stopFilter(LPARAM lParam)
{
	// Do this in critical section
	CAutoLock lock(&m_cs);

	// Nullify callbacks respectively
	UINT runningId = (UINT)lParam;
	if(runningId == (UINT)m_CurrentEcmCallback)
	{
		m_CurrentEcmFilterId = 0;
		m_CurrentEcmCallback = NULL;
		g_Logger.log(2, true, TEXT("Stop ECM filter received\n"));
	}
	else if(runningId == (UINT)m_CurrentEmmCallback)
	{
		m_CurrentEmmCallback = NULL;
		m_CurrentEmmFilterId = 0;
		g_Logger.log(2, true, TEXT("Stop EMM filter received\n"));
	}
}

// This is the callback called for each newly arrived dvb command
void PluginsHandler::dvbCommand(LPARAM lParam)
{
	// Do this in critical section
	CAutoLock lock(&m_cs);

	// Get the DVB command structure from the lParam
	TDVB_COMMAND dvbCommand = *(TDVB_COMMAND*)lParam;
	bool isOddKey = dvbCommand.buffer[4] ? true : false;

	// Let's check the state of the current client
	if(m_pCurrentClient != NULL)
	{
		// If OK, copy the key from the DVB command buffer
		BYTE key[8];
		for(int i = 0; i < 4; i++)
		{
			key[i * 2] = dvbCommand.buffer[6 + i * 2 + 1];
			key[i * 2 + 1] = dvbCommand.buffer[6 + i * 2];
		}
		// And set the key to the parser which called us
		m_pCurrentClient->caller->setKey(isOddKey, key);

		// Cancel deferred tuning
		m_DeferTuning = false;

		g_Logger.log(2, true, TEXT("Response for SID=%hu received, passing to the parser...\n"), m_CurrentSid);

		// Indicate we're no longer waiting for response
		m_WaitingForResponse = false;

		// Indicate we're no longer waiting on timer
		m_TimerInitialized = false;
	}
	else
		g_Logger.log(0, true, TEXT("Key received but the client has already left\n"));

}

// CA packets handler
void PluginsHandler::putCAPacket(ESCAParser* caller,
								 bool isEcmPacket,
								 const hash_set<USHORT>& caids,
								 USHORT sid,
								 USHORT caPid,
								 const BYTE* const currentPacket)
{
	// Do this in critical section
	CAutoLock lock(&m_cs);
	
	// See if we need to create a new client
	Client newClient;
	hash_map<ESCAParser*, Client>::iterator it = m_Clients.find(caller);
	if(it == m_Clients.end())
		m_Clients[caller] = newClient;
		
	// Initialize the client fields
	Client& currentClient = m_Clients[caller];
	currentClient.caids = caids;
	currentClient.caller = caller;
	currentClient.sid = sid;
	if(isEcmPacket)
	{
		g_Logger.log(2, true, TEXT("A new ECM packet for SID=%hu received and put to the queue\n"), sid);
		currentClient.ecmPid = caPid;
	}
	else
	{
		currentClient.emmPid = caPid;
		g_Logger.log(3, true, TEXT("A new EMM packet for SID=%hu received and put to the queue\n"), sid);
	}
	
	// Make a new request
	Request request;
	// Put the client into it
	request.client = &currentClient;
	// Make sure ECM and EMM packets are distinguished
	request.isEcmPacket = isEcmPacket;
	// Copy the packet itself
	memcpy(request.packet, currentPacket, sizeof(request.packet));

	// Handle ECM packets
	if(isEcmPacket)
	{
		// Put the request at the end of the queue
		m_RequestQueue.push_back(request);
		// Process the queue
		processECMPacketQueue();
	}
	else
	{
		// Here we handle EMM packets, just send them to the filter when it's ready
		if(m_CurrentEmmCallback != NULL)
			// Process it
			m_CurrentEmmCallback(m_CurrentEmmFilterId, PACKET_SIZE, request.packet);
		else
			g_Logger.log(3, true, TEXT("EMM callback hasn't been established yet, dropping the packet...\n"));
	}
}

// Routine processing packet queue
void PluginsHandler::processECMPacketQueue()
{
	// Let's see we aren't past the timeout
	// Let's check the indicator first
	if(m_TimerInitialized)
	{
		// Get the current time
		time_t now;
		time(&now);

		// Calculate the differentce
		if(difftime(now, m_Time) > g_Configuration.getDCWTimeout())
		{
			g_Logger.log(2, true, TEXT("Timeout for SID=%hu, resetting plugins...Do you have subscription for this channel?\n"), m_CurrentSid);
			// Reset the parser for this client
			if(m_pCurrentClient != NULL && m_pCurrentClient->caller != NULL)
				m_pCurrentClient->caller->reset();
			// Remove the problematic request from the beginning of the queue (only for tuning timeouts)
			if(m_IsTuningTimeout && !m_RequestQueue.empty())
				m_RequestQueue.pop_front();
			// Let's nullify everything
			m_CurrentSid = 0;
			m_pCurrentClient = NULL;
			m_CurrentEcmFilterId = 0;
			m_CurrentEcmCallback = NULL;
			m_CurrentEmmFilterId = 0;
			m_CurrentEmmCallback = NULL;
			m_DeferTuning = false;
			m_TimerInitialized = false;
			m_IsTuningTimeout = false;
			m_WaitingForResponse = false;
		}
	}

	// Boil out if the request queue is empty
	if(m_RequestQueue.empty())
		return;

	// Get the first request from the queue
	Request& request = m_RequestQueue.front();
	
	// If we have the same client and same SID
	if(m_pCurrentClient == request.client && m_CurrentSid == request.client->sid)
	{
		// If we can process the packet
		if(m_CurrentEcmCallback != NULL)
		{
			if(!m_WaitingForResponse)
			{
				g_Logger.log(2, true, TEXT("A new ECM packet for SID=%hu received and sent to processing\n"), m_CurrentSid);
				// Defer further tuning
				m_DeferTuning = true;
				// Process it
				m_CurrentEcmCallback(m_CurrentEcmFilterId, PACKET_SIZE, request.packet);
				// Remove request from the queue
				m_RequestQueue.pop_front();
				// Indicate we're waiting for response
				m_WaitingForResponse = true;
				// Get the current time to handle timeouts
				time(&m_Time);
				// And mark the time is initialized
				m_TimerInitialized = true;
				// This is packet processing timeout
				m_IsTuningTimeout = false;
			}
			else
				g_Logger.log(2, true, TEXT("A new ECM packet for SID=%hu received while the previous packet for the same SID was being processed, waiting...\n"), m_CurrentSid);
		}
		else
			g_Logger.log(2, true, TEXT("ECM callback hasn't been established yet for SID=%hu, waiting...\n"), m_CurrentSid);
	}
	else
	{
		g_Logger.log(2, true, TEXT("A tuning request came in for SID=%hu\n"), request.client->sid);
		if(!m_DeferTuning)
		{
			// Invalidate current ECM callback
			m_CurrentEcmCallback = NULL;
			// Defer further tuning
			m_DeferTuning = true;
			// Here The client or the SID are different
			// Very important: we don't send the packet to the filter yet (the filter hasn't started yet)
			// and don't remove it from the queue!
			// Update current client
			m_pCurrentClient = request.client;
			// And current sid
			m_CurrentSid = request.client->sid;
			// This client doesn't have any keys yet
			// For each plugin, check if it handles this particular CA
			for(list<Plugin>::iterator pit = m_Plugins.begin(); pit != m_Plugins.end(); pit++)
				if(pit->acceptsCAid(m_pCurrentClient->caids))
				{
					// And then finally tune to the program
					TPROGRAM82 tp;
					memset(&tp, 0, sizeof(tp));
					int i = 0;
					for(hash_set<USHORT>::const_iterator it = m_pCurrentClient->caids.begin(); it != m_pCurrentClient->caids.end(); it++, i++)
					{
						tp.CA[i].dwProviderId = *it;
						tp.CA[i].wCA_Type = *it;
						tp.CA[i].wECM = m_pCurrentClient->ecmPid;
						tp.CA[i].wEMM = g_Configuration.getEMMPid();
					}
					tp.wCACount = (WORD)i;
					tp.wSID = m_pCurrentClient->sid;
					pit->m_fpChannelChange(tp);
					// Get the current time to handle timeouts
					time(&m_Time);
					// And mark the time is initialized
					m_TimerInitialized = true;
					// This is the tuning timeout
					m_IsTuningTimeout = true;
				}
		}
		else
			g_Logger.log(2, true, TEXT("A tunung request came while an old one hasn't been satisfied yet, ignoring...\n"));
	}
}

// Remove obsolete caller
void PluginsHandler::removeCaller(ESCAParser* caller)
{
	// Do this in critical section
	CAutoLock lock(&m_cs);

	// Nullify current client if necessary
	if(m_pCurrentClient != NULL && m_pCurrentClient->caller == caller)
		m_pCurrentClient = NULL;

	// And remove all pending requests of this client
	for(list<Request>::iterator it = m_RequestQueue.begin(); it != m_RequestQueue.end();)
	{
		Request& request = *it;
		if(request.client != NULL && request.client->caller == caller)
			it = m_RequestQueue.erase(it);
		else
			it++;
	}

	// Finally, erase the client from the map
	m_Clients.erase(caller);
}
