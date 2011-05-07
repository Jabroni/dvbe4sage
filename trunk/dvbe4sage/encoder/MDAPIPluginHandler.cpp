#include "stdafx.h"

#include "resource.h"
#include "MDAPIPluginHandler.h"
#include "dvbparser.h"
#include "logger.h"
#include "configuration.h"
#include "recorder.h"

char const MDAPIVersion[] = "MD-API Version 01.03 - 01.06";

// This is the constructor of a class that has been exported.
// see MDAPIPluginsHandler.h for the class definition
MDAPIPluginsHandler::MDAPIPluginsHandler(HINSTANCE hInstance,
										 HWND hWnd,
										 HMENU hParentMenu) :
	m_CurrentEcmCallback(NULL),
	m_CurrentEmmCallback(NULL),
	m_CurrentPmtCallback(NULL),
	m_CurrentCatCallback(NULL),
	m_CurrentEcmFilterId(0),
	m_CurrentEmmFilterId(0),
	m_CurrentPmtFilterId(0),
	m_CurrentCatFilterId(0)
{
	// Log the entry
	log(0, true, 0, TEXT("Working with MDAPI plugins\n"));

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
					int keepRunning = 1;
					plugin.m_fpInit(hInstance, hWnd, FALSE, counter, &hotKey, (char*)&MDAPIVersion[0], &keepRunning);

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

	return;
}


// Cleanup plugins
MDAPIPluginsHandler::~MDAPIPluginsHandler()
{
	// Make sure the working thread is finished
	EndWorkerThread();

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

	// Nullify everything else
	m_CurrentEcmCallback = NULL;
	m_CurrentEmmCallback = NULL;
	m_CurrentPmtCallback = NULL;
	m_CurrentCatCallback = NULL;
	m_CurrentEcmFilterId = 0;
	m_CurrentEmmFilterId = 0;
	m_CurrentPmtFilterId = 0;
	m_CurrentCatFilterId = 0;
}

void MDAPIPluginsHandler::processECMPacket(BYTE* packet)
{
	m_CurrentEcmCallback(m_CurrentEcmFilterId, PACKET_SIZE/*(ULONG)packet[3] + 4*/, packet);  
}

void MDAPIPluginsHandler::processEMMPacket(BYTE* packet)
{
	// Here we handle EMM packets, just send them to the filter when it's ready
	if(m_CurrentEmmCallback != NULL)
		// Process it
		m_CurrentEmmCallback(m_CurrentEmmFilterId, PACKET_SIZE/*(ULONG)packet[3] + 4*/, packet);
	else
		log(4, true, 0, TEXT("EMM callback hasn't been established yet, dropping the packet...\n"));
}
void MDAPIPluginsHandler::processPMTPacket(BYTE *packet)
{
	// Here we handle PMT packets, just send them to the filter when it's ready
	if(m_CurrentPmtCallback != NULL)
		// Process it
		m_CurrentPmtCallback(m_CurrentPmtFilterId, PACKET_SIZE, packet);
	else
		log(4, true, 0, TEXT("PMT callback hasn't been established yet, dropping the packet...\n"));
}
void MDAPIPluginsHandler::processCATPacket(BYTE *packet)
{
	// Here we handle CAT packets, just send them to the filter when it's ready
	if(m_CurrentCatCallback != NULL)
		// Process it
		m_CurrentCatCallback(m_CurrentCatFilterId, PACKET_SIZE, packet);
	else
		log(4, true, 0, TEXT("CAT callback hasn't been established yet, dropping the packet...\n"));
}

// Message handler
LRESULT MDAPIPluginsHandler::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
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
				case MDAPI_GET_VERSION:
					strcpy_s((char*)lParam, sizeof(MDAPIVersion) / sizeof(MDAPIVersion[0]), MDAPIVersion);
					break;
				case MDAPI_CHANNELCHANGE:
					log(2, true, 0, TEXT("MDAPI_CHANNELCHANGE has been called\n"));
					break;
				case MDAPI_GET_PROGRAMM:
				{
					LPTPROGRAM82 tp = (LPTPROGRAM82)lParam;
					if(tp != NULL)
					{
						m_cs.Lock();
						fillTPStructure(tp);
						m_cs.Unlock();
					}
					break;
				}
				case MDAPI_GET_PROGRAMM_NUMMER:
				{
					LPTPROGRAMNUMBER pn = (LPTPROGRAMNUMBER)lParam;
					if(pn != NULL)
					{
						pn->RealNumber = 1;
						pn->VirtNumber = 1;
					}
					break;
				}
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
void MDAPIPluginsHandler::startFilter(LPARAM lParam)
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
			log(2, true, 0, TEXT("Start ECM filter for SID=%hu received\n"), m_CurrentSid);
		}
		else if(m_pCurrentClient->pmtPid == startFilter->Pid)
		{
			m_CurrentPmtCallback = (TMDAPIFilterProc)startFilter->Irq_Call_Adresse;
			m_CurrentPmtFilterId = startFilter->Filter_ID;
			log(2, true, 0, TEXT("Start PMT filter for SID=%hu received\n"), m_CurrentSid);
		}
		else if(startFilter->Pid == 1)
		{
			m_CurrentCatCallback = (TMDAPIFilterProc)startFilter->Irq_Call_Adresse;
			m_CurrentCatFilterId = startFilter->Filter_ID;
			log(2, true, 0, TEXT("Start CAT filter for SID=%hu received\n"), m_CurrentSid);
		}
		else if(m_pCurrentClient->emmCaids.hasPid(startFilter->Pid))
		{
			m_CurrentEmmCallback = (TMDAPIFilterProc)startFilter->Irq_Call_Adresse;
			m_CurrentEmmFilterId = startFilter->Filter_ID;
			log(2, true, 0, TEXT("Start EMM filter for SID=%hu received\n"), m_CurrentSid);
		}
		// Put the callback address into filter's running ID
		startFilter->Running_ID = (UINT)(startFilter->Filter_ID);
	}
	else
		log(0, true, 0, TEXT("Catastrophic error, no current client for a filter!\n"));
}

// This is the callback called for each filter about to be stopped
void MDAPIPluginsHandler::stopFilter(LPARAM lParam)
{
	// Do this in critical section
	CAutoLock lock(&m_cs);

	// Nullify callbacks respectively
	UINT runningId = (UINT)lParam;
	if(runningId == (UINT)m_CurrentEcmFilterId)
	{
		m_CurrentEcmFilterId = 0;
		m_CurrentEcmCallback = NULL;
		log(2, true, 0, TEXT("Stop ECM filter received\n"));
	}
	else if(runningId == (UINT)m_CurrentEmmFilterId)
	{
		m_CurrentEmmCallback = NULL;
		m_CurrentEmmFilterId = 0;
		log(2, true, 0, TEXT("Stop EMM filter received\n"));
	}
	else if(runningId == (UINT)m_CurrentPmtFilterId)
	{
		m_CurrentPmtCallback = NULL;
		m_CurrentPmtFilterId = 0;
		log(2, true, 0, TEXT("Stop PMT filter received\n"));
	}
	else if(runningId == (UINT)m_CurrentCatFilterId)
	{
		m_CurrentCatCallback = NULL;
		m_CurrentCatFilterId = 0;
		log(2, true, 0, TEXT("Stop CAT filter received\n"));
	}
}

// This is the callback called for each newly arrived dvb command
void MDAPIPluginsHandler::dvbCommand(LPARAM lParam)
{
	// Get the DVB command structure from the lParam
	TDVB_COMMAND dvbCommand = *(TDVB_COMMAND*)lParam;
	bool isOddKey = dvbCommand.buffer[4] ? true : false;

	// Do this in critical section
	CAutoLock lock(&m_cs);

	// Let's check the state of the current client
	if(m_pCurrentClient != NULL)
	{
		// Here we put the key
		Dcw dcw;

		// If OK, copy the key from the DVB command buffer
		for(int i = 0; i < 4; i++)
		{
			dcw.key[i * 2] = dvbCommand.buffer[6 + i * 2 + 1];
			dcw.key[i * 2 + 1] = dvbCommand.buffer[6 + i * 2];
		}

		// Indicate that ECM request has been completed and add it to the cache
		ECMRequestComplete(m_pCurrentClient, m_pCurrentClient->ecmPacket, dcw, isOddKey, true);
	}
	else
		log(0, true, 0, TEXT("Key received but the client has already left\n"));

}

void MDAPIPluginsHandler::resetProcessState()
{
	m_CurrentEcmFilterId = 0;
	m_CurrentEcmCallback = NULL;
	m_CurrentEmmFilterId = 0;
	m_CurrentEmmCallback = NULL;
	m_CurrentPmtFilterId = 0;
	m_CurrentPmtCallback = NULL;
	m_CurrentCatFilterId = 0;
	m_CurrentCatCallback = NULL;
}

bool MDAPIPluginsHandler::handleTuningRequest()
{
	// Invalidate current ECM callback
	m_CurrentEcmCallback = NULL;

	// Fill the structure
	TPROGRAM82 tp;
	fillTPStructure(&tp);
	
	// Unlock the critical section, since some of the ####### plugins do SendMessage and not PostMessage
	m_cs.Unlock();

	// This client doesn't have any keys yet
	// For each plugin, check if it handles this particular CA
	for(list<Plugin>::iterator pit = m_Plugins.begin(); pit != m_Plugins.end(); pit++)
		//if(pit->acceptsCAid(m_pCurrentClient->ecmCaids))
		{
			// And then finally tune to the program
			pit->m_fpChannelChange(tp);
			// Boil out (only one plugin is supported)
			break;
		}
	
	return false;
}

void MDAPIPluginsHandler::fillTPStructure(LPTPROGRAM82 tp)
{
	// Zero memory
	memset(tp, 0, sizeof(TPROGRAM82));

	// Copy the channel name
	CT2A name(m_pCurrentClient->channelName);
	strcpy_s(tp->szName, sizeof(tp->szName) / sizeof(tp->szName[0]), name);

	USHORT onid = m_pCurrentClient->caller->getRecorder()->getOnid();

	log(3, true, 0, TEXT("Initializing plugin structure with Channel Name %s\n"), tp->szName);

	// Loop through all CAIDs
	int i = 0;
	int index = 0;
	for(hash_set<CAScheme>::const_iterator it = m_pCurrentClient->ecmCaids.begin(); it != m_pCurrentClient->ecmCaids.end(); it++, i++)
	{
		tp->CA[i].dwProviderId = onid;
		tp->CA[i].wCA_Type = it->caId;
		tp->CA[i].wECM = (WORD)it->pid;
		for(EMMInfo::const_iterator it1 = m_pCurrentClient->emmCaids.begin(); it1 != m_pCurrentClient->emmCaids.end(); it1++)
			if(it1->caId == tp->CA[i].wCA_Type)
			{
				tp->CA[i].wEMM = it1->pid;
				break;
			}
		log(3, false, 0, TEXT("CA[%d]: ProviderID=%d, CAID=%.04hX, ECM=%.04hX, EMM=%.04hX\n"), i, tp->CA[i].dwProviderId, tp->CA[i].wCA_Type, tp->CA[i].wECM, tp->CA[i].wEMM);
		if(tp->CA[i].wECM == m_pCurrentClient->ecmPid)
			index = i;
	}
	log(3, false, 0, TEXT("Number of CAIDs is %d\n"), i);
	tp->byCA = (BYTE)index;
	tp->wCACount = (WORD)i;
	tp->wECM = m_pCurrentClient->ecmPid;
	tp->wSID = m_pCurrentClient->sid;
	tp->wPMT = m_pCurrentClient->pmtPid;
	log(3, true, 0, TEXT("byCA=%d, wCACount=%d, wECM=%.04hX, wSID=%d, wPMT=%.04hX\n"), tp->byCA, tp->wCACount, tp->wECM, tp->wSID, tp->wPMT);

	// Set the satellite orbital location for whatever plugins (N3XT) can make use of it
	UINT orbitalLocation;
	bool east;
	bool found = m_pCurrentClient->caller->m_pParent->getNetworkProvider().getSatelliteOrbitalLocation(onid, orbitalLocation, east);
	if(found)
	{		
		if(east == false)
		{
			double adjustedLocation = double(orbitalLocation) / 10.0;
			orbitalLocation = (UINT)((360.0 - adjustedLocation) * 10.0);
		}

		log(3, true, 0, TEXT("Setting plugin handler orbitalLocation to %d for onid %hu\n"), orbitalLocation, onid);
		UINT *pLocation = (UINT *)&tp->szBuffer[0];
		*pLocation = orbitalLocation;
	}
	else
	{
		log(3, true, 0, TEXT("Could not find orbitalLocation for onid %hu\n"), onid);
	}
}
