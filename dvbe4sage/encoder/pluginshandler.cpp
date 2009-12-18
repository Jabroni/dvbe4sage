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
PluginsHandler::PluginsHandler() :
	m_pCurrentClient(NULL),
	m_CurrentSid(0),
	m_DeferTuning(false),
	m_WaitingForResponse(false),
	m_TimerInitialized(false),
	m_IsTuningTimeout(false),
	m_ExitWorkerThread(false),
	m_ECMCache(g_pConfiguration->getMaxECMCacheSize(), g_pConfiguration->getECMCacheAutodeleteChunkSize())
{

	// Create worker thread
	m_WorkerThread = CreateThread(NULL, 0, pluginsHandlerWorkerThreadRoutine, this, 0, NULL);

	return;
}


// Cleanup plugins
PluginsHandler::~PluginsHandler()
{
	// Do this in the critical section, just in case some threads still do something with this object
	CAutoLock lock(&m_cs);

	// Remove everything from the requests queue
	m_RequestQueue.clear();

	// Remove all the clients
	m_Clients.clear();

	// Nullify everything else
	m_pCurrentClient = NULL;
}

bool PluginsHandler::wrong(const Dcw& dcw)
{
	return dcw.number == 0;
}

// CA packets handler
void PluginsHandler::putCAPacket(ESCAParser* caller,
								 CAPacketType packetType,
								 const hash_set<CAScheme>& ecmCaids,
								 const EMMInfo& emmCaids,
								 USHORT sid,
								 LPCTSTR channelName,
								 USHORT caPid,
								 USHORT pmtPid,
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
	currentClient.ecmCaids = ecmCaids;
	currentClient.emmCaids = emmCaids;
	currentClient.caller = caller;
	currentClient.sid = sid;
	currentClient.channelName = channelName;
	currentClient.pmtPid = pmtPid;
	if(packetType == TYPE_ECM)
	{
		log(2, true, 0, TEXT("A new ECM packet for SID=%hu received and put to the queue\n"), sid);
		currentClient.ecmPid = caPid;
		memcpy(currentClient.ecmPacket, currentPacket, PACKET_SIZE);
	}
	
	// Make a new request
	Request request;
	// Put the client into it
	request.client = &currentClient;
	// Copy the packet itself
	memcpy(request.packet, currentPacket, sizeof(request.packet));

	// Handle incoming packets differently
	switch(packetType)
	{
		case TYPE_ECM:
			// Put the request at the end of the queue
			m_RequestQueue.push_back(request);
			// Process the queue - no need for that, will be picked up by the worker thread anyway
			//processECMPacketQueue();
			break;
		case TYPE_EMM:
			processEMMPacket(request.packet);
			break;
		case TYPE_PMT:
			processPMTPacket(request.packet);
			break;
		case TYPE_CAT:
			processCATPacket(request.packet);
			break;
		default:
			log(0, true, 0, TEXT("Received unknown kind of packet, dropping...\n"));
			break;
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
		time_t now = 0;
		time(&now);

		// Calculate the differentce
		if(difftime(now, m_Time) > g_pConfiguration->getDCWTimeout())
		{
			log(2, true, 0, TEXT("Timeout for SID=%hu, resetting plugins...Do you have subscription for this channel?\n"), m_CurrentSid);
			// Reset the parser for this client
			if(m_pCurrentClient != NULL && m_pCurrentClient->caller != NULL)
				m_pCurrentClient->caller->reset();
			// Remove the problematic request from the beginning of the queue (only for tuning timeouts)
			// Updated  on 20/11/2009 - temporarily removed, I'll see whether this plays nicely with some of the plugins
			/*if(m_IsTuningTimeout && !m_RequestQueue.empty())
				m_RequestQueue.pop_front();*/
			// Let's nullify everything
			m_CurrentSid = 0;
			m_pCurrentClient = NULL;
			resetProcessState();
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

	// Let's see if we already have a DCW for this very ECM in the cache
	bool isOddKey = false;
	const Dcw& dcw = m_ECMCache.find(request.packet, isOddKey);
	if(dcw.number != 0)
	{
		// Update current client
		m_pCurrentClient = request.client;
		// And current sid
		m_CurrentSid = request.client->sid;
		// Remove request from the queue
		m_RequestQueue.pop_front();
		// Mark ECM request as complete WITHOUT adding it to the cache
		ECMRequestComplete(NULL, dcw, isOddKey, false);
	}
	else
	{
		// If we have the same client and same SID
		if(m_pCurrentClient == request.client && m_CurrentSid == request.client->sid)
		{
			// If we can process the packet
			if(canProcessECM())
			{
				if(!m_WaitingForResponse)
				{
					log(2, true, 0, TEXT("A new ECM packet for SID=%hu received and sent to processing\n"), m_CurrentSid);
					// Defer further tuning
					m_DeferTuning = true;
					// Process it
					processECMPacket(request.packet);
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
					log(2, true, 0, TEXT("A new ECM packet for SID=%hu received while the previous packet for the same SID was being processed, waiting...\n"), m_CurrentSid);
			}
			else
				log(2, true, 0, TEXT("ECM callback hasn't been established yet for SID=%hu, waiting...\n"), m_CurrentSid);
		}
		else
		{
			log(2, true, 0, TEXT("A tuning request came in for SID=%hu\n"), request.client->sid);
			if(!m_DeferTuning)
			{
				// Defer further tuning
				m_DeferTuning = true;
				// Here The client or the SID are different
				// Very important: we don't send the packet to the filter yet (the filter hasn't started yet)
				// and don't remove it from the queue!
				// Update current client
				m_pCurrentClient = request.client;
				// And current sid
				m_CurrentSid = request.client->sid;
				// Retrieve new DCW using plugins
  				// Get the current time to handle timeouts
	  			time(&m_Time);
				// And mark the time is initialized
				m_TimerInitialized = true;
				// This is the tuning timeout
				m_IsTuningTimeout = true;
		 		// Handle the tuning request
				handleTuningRequest();
			}
			else
				log(2, true, 0, TEXT("A tunung request came while an old one hasn't been satisfied yet, ignoring...\n"));
		}
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

bool EMMInfo::hasPid(USHORT pid) const
{
	for(hash_set<CAScheme>::const_iterator it = begin(); it != end(); it++)
		if(it->pid == pid)
			return true;
	return false;
}

void PluginsHandler::ECMRequestComplete(const BYTE* ecmPacket,
										const Dcw& dcw,
										bool isOddKey,
										bool addToCache)
{
	// Set the key to the parser which called us
	if(wrong(dcw) || !m_pCurrentClient->caller->setKey(isOddKey, dcw.key))
	{
		// Log the key
		log(2, true, 0, TEXT("Received %s DCW = %.02hX%.02hX%.02hX%.02hX%.02hX%.02hX%.02hX%.02hX (from the %s) - wrong key, discarded!\n"), isOddKey ? TEXT("ODD") : TEXT("EVEN"),
			(USHORT)dcw.key[0], (USHORT)dcw.key[1], (USHORT)dcw.key[2], (USHORT)dcw.key[3], (USHORT)dcw.key[4], (USHORT)dcw.key[5], (USHORT)dcw.key[6], (USHORT)dcw.key[7],
			addToCache ? TEXT("plugin") : TEXT("cache"));
		return;
	}
	else
	{
		// Add the data to the cache, if needed
		if(addToCache)
			m_ECMCache.add(ecmPacket, dcw, isOddKey);
		// Log the key
		log(2, true, 0, TEXT("Received %s DCW = %.02hX%.02hX%.02hX%.02hX%.02hX%.02hX%.02hX%.02hX (from the %s) - accepted and %sadded to the cache!\n"), isOddKey ? TEXT("ODD") : TEXT("EVEN"),
			(USHORT)dcw.key[0], (USHORT)dcw.key[1], (USHORT)dcw.key[2], (USHORT)dcw.key[3], (USHORT)dcw.key[4], (USHORT)dcw.key[5], (USHORT)dcw.key[6], (USHORT)dcw.key[7],
			addToCache ? TEXT("plugin") : TEXT("cache"), addToCache ? TEXT("") : TEXT("NOT "));
	}

	log(2, true, 0, TEXT("Response for SID=%hu received, passing to the parser...\n"), m_CurrentSid);

	// Cancel deferred tuning
	m_DeferTuning = false;

	// Indicate we're no longer waiting for response
	m_WaitingForResponse = false;

	// Indicate we're no longer waiting on timer
	m_TimerInitialized = false;
}