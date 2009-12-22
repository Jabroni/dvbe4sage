// encoder.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#include "encoder.h"
#include "DVBParser.h"
#include "graph.h"
#include "DVBFilter.h"
#include "Logger.h"
#include "tuner.h"
#include "recorder.h"
#include "configuration.h"
#include "virtualtuner.h"
#include "extern.h"
#include "MDAPIPluginHandler.h"
#include "VGPluginHandler.h"
#include "FileSource.h"

Encoder::Encoder(HINSTANCE hInstance, HWND hWnd, HMENU hParentMenu) :
	m_pPluginsHandler(NULL),
	m_hWnd(hWnd)
{
	// Set the logger level
	if(g_pLogger != NULL)
		g_pLogger->setLogLevel(g_pConfiguration->getLogLevel());

	// Initialize COM stuff
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	// Initialize plugins (either MDAPI or VGCam)
	if (g_pConfiguration->isVGCam())
		m_pPluginsHandler = new VGPluginsHandler();
	else
		m_pPluginsHandler = new MDAPIPluginsHandler(hInstance, hWnd, hParentMenu);

	// Initialize tuners, go from 1 trough N
	for(int i = 1; i <= DVBSFilterGraph::getNumberOfTuners(); i++)
		if(!g_pConfiguration->excludeTuner(i))
		{
			DVBSTuner* tuner = new DVBSTuner(this,
											 i,
											 g_pConfiguration->getInitialFrequency(),
											 g_pConfiguration->getInitialSymbolRate(),
											 g_pConfiguration->getInitialPolarization(),
											 g_pConfiguration->getInitialModulation(),
											 g_pConfiguration->getInitialFEC());

			// Let's see if we work by "included logic" first (rather than "anything is included by default")
			if(g_pConfiguration->includeTuners())
			{
				// Now, let's see if this tuner should be included, either by ordinal or by MAC
				if(!g_pConfiguration->includeTuner(i) && !g_pConfiguration->includeTunersByMAC(tuner->getTunerMac()))
				{
					log(0, true, i, TEXT("Tuner ordinal=%d, MAC=%s, is NOT INCLUDED either by the ordinal or by MAC address\n"), i, tuner->getTunerMac().c_str());
					delete tuner;
					continue;
				}
			}

			// Let's see if we this tuner is excluded by its MAC
			if (g_pConfiguration->excludeTunersByMAC(tuner->getTunerMac()))
			{
				log(0, true, i, TEXT("Tuner ordinal=%d, MAC=%s, is excluded by MAC address\n"), i, tuner->getTunerMac().c_str());
				delete tuner;
				continue;
			}
			
			if(tuner->isSourceOK() && tuner->startPlayback(false))
			{
				// Let's see if this is a DVB-S2 tuner and put it into the list accordingly
				if(g_pConfiguration->isDVBS2Tuner(tuner->getSourceOrdinal()))
					m_Tuners.push_back(tuner);
				else
					m_Tuners.insert(m_Tuners.begin(), tuner);

				// Run the tuner on idle
				tuner->runIdle();
			}
		}
		else
			log(0, true, i, TEXT("Tuner ordinal=%d is excluded by the ordinal\n"), i);

	// Initialize Winsock layer
	WSAData wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	// Create some virtual tuners
	for(USHORT i = 0; i < g_pConfiguration->getNumberOfVirtualTuners(); i++)
	{
		VirtualTuner* const virtualTuner = new VirtualTuner(g_pConfiguration->getListeningPort() + i, hWnd);
		m_VirtualTuners.push_back(virtualTuner);
		m_VirtualTunersMap[virtualTuner->getServerSocket()] = virtualTuner;
	}
}
	

Encoder::~Encoder()
{
	// For all recorders
	while(!m_Recorders.empty())
	{
		// Get the recorder pointer
		Recorder* recorder = m_Recorders.begin()->second;
		// Log the entry
		log(0, true, recorder->getSource()->getSourceOrdinal(), TEXT("Aborting, "));
		// Stop ongoing recordings
		recorder->stopRecording();
		// And delete the recorder
		delete recorder;
	}

	// For all virtual tuners
	for(UINT i = 0; i < m_VirtualTuners.size(); i++)
	{
		// Get the tuner pointer
		VirtualTuner* virtualTuner = m_VirtualTuners[i];
		// And delete it
		delete virtualTuner;
	}

	// For all tuners
	for(UINT i = 0; i < m_Tuners.size(); i++)
	{
		// Get the tuner pointer
		DVBSTuner* tuner = m_Tuners[i];
		// And delete it
		delete tuner;
	}

	// Delete plugins handler
	delete m_pPluginsHandler;	

	// Cleanup Winsock layer
	WSACleanup();

	// Uninitialize COM
	CoUninitialize();

	// Log "stop encoder" entry
	log(0, true, 0, TEXT("Encoder stopped!\n"));
}

void Encoder::socketOperation(SOCKET socket,
							  WORD eventType,
							  WORD error)
{
	switch(eventType)
	{
		case FD_READ:
		{
			// Buffer for read (this really should be enough)
			char buffer[20000];
			// Read the command, we assume it comes as one chunk
			int received = recv(socket, buffer, sizeof(buffer), 0);
			// If got something
			if(received > 0)
			{
				// Find the right virtual tuner
				hash_map<SOCKET, VirtualTuner*>::iterator it = m_VirtualTunersMap.find(socket);
				if(it != m_VirtualTunersMap.end())
				{
					// Obtain it
					VirtualTuner* virtualTuner = it->second;
					// Get the full command - in UNICODE!
					WCHAR fullCommandUTF16[4096];				
					int fullCommandLength = MultiByteToWideChar(CP_UTF8, 0, buffer, received, fullCommandUTF16, sizeof(fullCommandUTF16) / sizeof(fullCommandUTF16[0]));
					wstring fullCommand(fullCommandUTF16, fullCommandLength);

					// Print the command received
					log(1, true, 0, TEXT("Received command: \"%.*S\"\n"), fullCommand.length() - 2, fullCommand.c_str());

					// Determine the command itself
					wstring command(fullCommandUTF16, fullCommand.find_first_of(L" \r\n"));

					// If the command is START
					if(command == L"START")
					{
							// First we've got the source
						int sourceEndPos = fullCommand.find(L'|');
						wstring source(fullCommandUTF16 + 6, sourceEndPos - 6);

						// Then a security key (added in version 3.0, we ignore it for now)
						int keyEndPos = fullCommand.find(L'|', sourceEndPos + 1);

						// Then the channel number
						int channelEndPos = fullCommand.find(L'|', keyEndPos + 1);
						wstring channel(fullCommandUTF16 + keyEndPos + 1, channelEndPos - keyEndPos - 1);
						int channelInt = _wtoi(channel.c_str());

						// Then the duration of the recording (usually meaningless)
						int durationEndPos = fullCommand.find(L'|', channelEndPos + 1);
						wstring duration(fullCommandUTF16 + channelEndPos + 1, durationEndPos - channelEndPos - 1);
						__int64 durationInt = _wtoi64(duration.c_str()) / 1000;

						// Last is the file name for recording (can use UNICODE characters
						int fileNameEndPos = fullCommand.find(L'|', durationEndPos + 1);
						wstring fileName(fullCommandUTF16 + durationEndPos + 1, fileNameEndPos - durationEndPos - 1);

						// Log the command with all its parameters
						log(0, true, 0, TEXT("Received START command to start recording on source \"%S\", channel=%d, duration=%I64d, file=\"%S\"\n"),
							source.c_str(), channelInt, durationInt, fileName.c_str());

						// Start the recording itself
						startRecording(true, 0, 0, BDA_POLARISATION_NOT_SET, BDA_MOD_NOT_SET, BDA_BCC_RATE_NOT_SET, -1, true, channelInt, g_pConfiguration->getUseSidForTuning(), durationInt, fileName.c_str(), virtualTuner, (__int64)-1, true, false);

						// Report OK code to the client
						send(socket, "OK\r\n", 4, 0);
					}
					else if(command == L"STOP")
					{
						log(0, true, 0, TEXT("Requested by Sage, "));
						// Stop ongoing recording
						Recorder* recorder = virtualTuner->getRecorder();
						if(recorder != NULL)
						{
							recorder->stopRecording();
							// Nullify the recorder in the virtual tuner
							virtualTuner->setRecorder(NULL);
							// And delete the recorder
							delete recorder;
						}
						else
							log(0, false, 0, TEXT(" there is no recorder to stop!\n"));
						send(socket, "OK\r\n", 4, 0);
					}
					else if(command == L"VERSION")
					{
						send(socket, "3.0\r\n", 5, 0);
					}
					else if(command == L"SWITCH")
					{
						send(socket, "OK\r\n", 4, 0);
					}
					else if(command == L"GET_SIZE")
					{
						send(socket, "0\r\n", 3, 0);
					}
					else if(command == L"NOOP")
					{
						send(socket, "OK\r\n", 4, 0);
					}
					else if(command == L"BUFFER")
					{
						// First we've got the source
						int sourceEndPos = fullCommand.find(L'|');
						wstring source(fullCommandUTF16 + 7, sourceEndPos - 7);

						// Then a security key (added in version 3.0, we ignore it for now)
						int keyEndPos = fullCommand.find(L'|', sourceEndPos + 1);

						// Then the channel number
						int channelEndPos = fullCommand.find(L'|', keyEndPos + 1);
						wstring channel(fullCommandUTF16 + keyEndPos + 1, channelEndPos - keyEndPos - 1);
						int channelInt = _wtoi(channel.c_str());

						// The the cyclic buffer size
						int sizeEndPos = fullCommand.find(L'|', channelEndPos + 1);
						wstring size(fullCommandUTF16 + channelEndPos + 1, sizeEndPos - channelEndPos - 1);
						__int64 sizeInt = _wtoi64(size.c_str());

						// Last one is the file name
						int fileNameEndPos = fullCommand.find(L'|', sizeEndPos + 1);
						wstring fileName(fullCommandUTF16 + sizeEndPos + 1, fileNameEndPos - sizeEndPos - 1);

						// Log the command with its parameters
						log(0, true, 0, TEXT("Received BUFFER command to start recording on source \"%S\", channel=%d, size=%I64d, file=\"%S\"\n"),
							source.c_str(), channelInt, sizeInt, fileName.c_str());

						// Start the actual recording
						startRecording(true, 0, 0, BDA_POLARISATION_NOT_SET, BDA_MOD_NOT_SET, BDA_BCC_RATE_NOT_SET, -1, true, channelInt, g_pConfiguration->getUseSidForTuning(), 3600, fileName.c_str(), virtualTuner, sizeInt, true, false);

						// Report OK code to the client
						send(socket, "OK\r\n", 4, 0);
					}
					else if(command == L"BUFFER_SWITCH")
					{
						send(socket, "OK\r\n", 4, 0);
					}
					else if(command == L"GET_FILE_SIZE")
					{
						// Lock the control structures
						CAutoLock lock(&m_cs);
						// Get the file name from the command
						int fileNamePos = fullCommand.find_first_of(L"\r\n");
						wstring fileName(fullCommandUTF16 + 14, fileNamePos - 14);

						// Find the corresponding recorder
						Recorder* recorder = virtualTuner->getRecorder();
						if(recorder != NULL && recorder->getFileName() == fileName)
						{
							// Send the length as a response
							char buffer[100];
							int length = sprintf_s(buffer, sizeof(buffer), "%I64u\r\n", recorder->getFileLength());
							log(0, true, 0, TEXT("Replied %.*s\n"), length - 2, buffer);
							send(socket, buffer, length, 0);
						}
						else
							// Send 0
							send(socket, "0\r\n", 3, 0);
					}
					else if(command == L"TUNE" || command == L"AUTOTUNE")
					{
						send(socket, "OK\r\n", 4, 0);
					}
					else
						// Log an unknown command and do nithing with it
						log(0, true, 0, TEXT("Received unknown command \"%S\", ignoring...\n"), fullCommand.c_str());
				}
				else
					log(0, true, 0, TEXT("Received command from client that already left, ignoring...\n"));
			}
			break;
		}
		case FD_CLOSE:
		{
			closesocket(socket);
			m_VirtualTunersMap.erase(socket);
			break;
		}
		default:
			break;
	}
}

LRESULT Encoder::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_ACCEPT_CONNECTION:
		{
			SOCKET newSocket = accept((SOCKET)wParam, NULL, NULL);
			WSAAsyncSelect(newSocket, m_hWnd, WM_SEND_RECEIVE_CLOSE, FD_READ | FD_CLOSE);
			hash_map<SOCKET, VirtualTuner*>::iterator it = m_VirtualTunersMap.find((SOCKET)wParam);
			if(it != m_VirtualTunersMap.end())
				m_VirtualTunersMap[newSocket] = it->second;
			break;
		}
		case WM_SEND_RECEIVE_CLOSE:
			socketOperation((SOCKET)wParam, WSAGETSELECTEVENT(lParam), WSAGETSELECTERROR(lParam));
			break;
		default:
			// Pass the messages to the plugin handler
			if(m_pPluginsHandler != NULL)
				return m_pPluginsHandler->WindowProc(message, wParam, lParam);
	}
	return 0;
}

DVBSTuner* Encoder::getTuner(int tunerOrdinal,
						 bool useLogicalTuner,
						 const Transponder* const pTransponder)
{
	// Initialize with NULL
	DVBSTuner* tuner = NULL;

	// If TID == 0 we assume physical tuner
	if(useLogicalTuner && pTransponder != NULL)
	{
		// Now let's find an appropriate tuner
		// Let's see if there isn't one already tuned to this TID
		for(UINT i = 0; i < m_Tuners.size(); i++)
			if(m_Tuners[i]->running() && m_Tuners[i]->getTid() == pTransponder->tid)
			{
				tuner = m_Tuners[i];
				break;
			}
		// If there is no suitable tuner already in use
		if(tuner == NULL)
			for(UINT i = 0; i < m_Tuners.size(); i++)
				// The tuner we're looking for should be not running and if the program requires DVB-S2
				// we should pick only a compatible tuner
				if(!m_Tuners[i]->running() && (pTransponder->modulation == BDA_MOD_QPSK || 
					g_pConfiguration->isDVBS2Tuner(m_Tuners[i]->getSourceOrdinal())))
					{
						tuner = m_Tuners[i];
						break;
					}
		
	}
	else
		// Here we just pull the right physical tuner
		for(UINT i = 0; i < m_Tuners.size(); i++)
		{
			if(m_Tuners[i]->getSourceOrdinal() == tunerOrdinal)
			{
				tuner = m_Tuners[i];
				break;
			}
		}

	return tuner;
}

bool Encoder::startRecording(bool autodiscoverTransponder,
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
							 bool startFullTransponderDump)
{
	// Use the internal parser
	// Lock it
	m_Provider.lock();

	// First, let's obtain the SID for the channel
	USHORT sid = 0;

	// The channel name will be held here
	TCHAR channelName[256];
	channelName[0] = TCHAR('\0');

	// If provided with a SID, just use it
	if(useSid)
	{
		sid = (USHORT)channel;
		// Get the channel name
		m_Provider.getServiceName(sid, channelName, sizeof(channelName) / sizeof(channelName[0]));
		
		// Make a log entry
		log(2, true, 0, TEXT("Service SID=%hu has Name=\"%s\"\n"), sid, channelName);
	}
	// Otherwise, find it from the mapping
	else if(!m_Provider.getSidForChannel((USHORT)channel, sid))
	{
		// If not found, report an error
		log(0, true, 0, TEXT("Cannot obtain SID number for the channel \"%d\", no recording done!\n"), channel);

		// Unlock the parser
		m_Provider.unlock();

		// And fail the recording
		return false;
	}
	else
	{
		// Get the channel name
		m_Provider.getServiceName(sid, channelName, sizeof(channelName) / sizeof(channelName[0]));

		// Make a log entry
		log(2, true, 0, TEXT("Channel=%d was successfully mapped to SID=%hu, Name=\"%s\"\n"), channel, sid, channelName);
	}

	// Now search for the tuner
	DVBSTuner* tuner = NULL;

	// TID of the transponder (will be initialized only if autodiscovery was requested)
	USHORT tid = 0;

	// Now, let's retrieve the transponder info if required
	if(autodiscoverTransponder)
	{
		// Get the transponder
		Transponder transponder;
		if(m_Provider.getTransponderForSid(sid, transponder))
		{
			// Override the tuning parameters
			frequency = transponder.frequency;
			symbolRate = transponder.symbolRate;
			polarization = transponder.polarization;
			modulation = transponder.modulation;
			fecRate = transponder.fec;

			// Make a log entry
			log(2, true, 0, TEXT("Autodiscovery results for SID=%hu: TID=%hu, Frequency=%lu, Symbol Rate=%lu, Polarization=%s, Modulation=%s, FEC=%s\n"), sid,
				transponder.tid, frequency, symbolRate, printablePolarization(polarization),	printableModulation(modulation), printableFEC(fecRate));

			// And get the tuner according to these
			tuner = getTuner(tunerOrdinal, useLogicalTuner, &transponder);

			// Save the TID
			tid = transponder.tid;
		}
		else
		{
			// If transponder is not found, report an error
			log(0, true, 0, TEXT("Autodiscovery requested, but cannot find the transponder for SID \"%hu\" (\"%s\"), no recording done!\n"), sid, channelName);

			// Unlock the parser
			m_Provider.unlock();

			// And fail the recording
			return false;
		}
	}
	else
		// No auto discovery is requested, tuner can only be physical
		tuner = getTuner(tunerOrdinal, false, NULL);

	// Unlock the parser
	m_Provider.unlock();

	// Boil out if an appropriate tuner is not found
	if(tuner == NULL)
	{
		log(0, true, 0, TEXT("Cannot start recording for %s=%d (\"%s\"), no sutable source found!\n"), useSid ? TEXT("SID") : TEXT("Channel"), channel, channelName);
		return false;
	}
	
	// Make a log entry
	if(useSid)
		log(0, true, tuner->getSourceOrdinal(), TEXT("Starting recording on tuner=\"%s\", Ordinal=%d, SID=%hu (\"%s\"), Autodiscovery=%s, Duration=%I64d, Frequency=%lu, Symbol Rate=%lu, Polarization=%s, Modulation=%s, FEC=%s\n"),
			tuner->getSourceFriendlyName(), tuner->getSourceOrdinal(), sid, channelName, autodiscoverTransponder ? TEXT("TRUE") : TEXT("FALSE"), duration,
			frequency, symbolRate, printablePolarization(polarization),	printableModulation(modulation), printableFEC(fecRate));
	else
		log(0, true, tuner->getSourceOrdinal(), TEXT("Starting recording on tuner=\"%s\", Ordinal=%d, Channel=%d (\"%s\"), SID=%hu, Autodiscovery=%s, Duration=%I64d, Frequency=%lu, Symbol Rate=%lu, Polarization=%s, Modulation=%s, FEC=%s\n"),
			tuner->getSourceFriendlyName(), tuner->getSourceOrdinal(), channel, channelName, sid, autodiscoverTransponder ? TEXT("TRUE") : TEXT("FALSE"), duration,
			frequency, symbolRate, printablePolarization(polarization),	printableModulation(modulation), printableFEC(fecRate));

	// If we found the tuner
	// Create the recorder
	Recorder* recorder = new Recorder(m_pPluginsHandler, tuner, outFileName, useSid, channel, sid, channelName, duration, this, size, bySage);

	// Let's see if the recorder has an error, just delete it and exit
	if(recorder->hasError())
	{
		log(0, true, tuner->getSourceOrdinal(), TEXT("Cannot start recording!\n")),
		delete recorder;
		return false;
	}

	// Lock access to global structure
	m_cs.Lock();

	// Let's see if the tuner is already used
	bool bTunerUsed = false;
	for(hash_multimap<USHORT, Recorder*>::iterator it = m_Recorders.begin(); it != m_Recorders.end(); it++)
		if(it->second->getSource() == tuner)
		{
			bTunerUsed = true;
			break;
		}

	// Assign this recorder to the virtual tuner if needed
	if(virtualTuner != NULL)
		virtualTuner->setRecorder(recorder);

	// Set the flag to succeeded
	bool succeeded = true;

	// Let's see if we need to do anything on the tuner
	if(!bTunerUsed)
	{
		// Tune to the right parameters
		tuner->tune(frequency, symbolRate, polarization, modulation, fecRate);

		// Now, start the recording
		if(tuner->startPlayback(startFullTransponderDump))
		{
			// Set the TID if recording was started successfully
			tuner->setTid(tid);
			// And add the recorder to the global structure
			m_Recorders.insert(pair<USHORT, Recorder*>((USHORT)tunerOrdinal, recorder));
		}
		else
		{
			// Unset succeeded flag
			succeeded = false;
			// And delete the recorder
			delete recorder;
		}
	}
	else
		// Just add the recorder to the global structure
		m_Recorders.insert(pair<USHORT, Recorder*>((USHORT)tunerOrdinal, recorder));

	// Unlock access to the global structure
	m_cs.Unlock();

	// If succeeded, try to start recording
	if(succeeded)
	{
		// Start the recording
		recorder->startRecording();
		// All is good
		return true;
	}
	else
	{
		// Make the log entry
		log(0, true, tuner->getSourceOrdinal(), TEXT("Cannot start recording!\n"));
		// And return false
		return false;
	}
}

bool Encoder::stopRecording(Recorder* recorder)
{
	// Lock access to global structure
	CAutoLock lock(&m_cs);

	// Get the source of the recorder
	GenericSource* source = recorder->getSource();

	// Remove the recorder
	for(hash_map<USHORT, Recorder*>::iterator it = m_Recorders.begin(); it != m_Recorders.end(); it++)
		if(it->second == recorder)
		{
			m_Recorders.erase(it);
			break;
		}

	// Get the parser from the recorder and tell plugins handler to start ignoring it
	m_pPluginsHandler->removeCaller(recorder->getParser());

	// Let's see if the source is still used
	bool bSourceUsed = false;
	for(hash_map<USHORT, Recorder*>::iterator it = m_Recorders.begin(); it != m_Recorders.end(); it++)
		if(it->second->getSource() == source)
		{
			bSourceUsed = true;
			break;
		}

	// If no, stop running graph on it
	if(!bSourceUsed)
		source->stopPlayback();

	return true;
}

int Encoder::getNumberOfTuners() const
{
	return m_Tuners.size();
}

LPCTSTR Encoder::getTunerFriendlyName(int i) const
{
	return m_Tuners[i]->getSourceFriendlyName();
}

int Encoder::getTunerOrdinal(int i) const
{
	return m_Tuners[i]->getSourceOrdinal();
}

void Encoder::waitForFullInitialization()
{
	// Make a log entry that the setup is complete
	log(0, true, 0, TEXT("Waiting for finishing encoder initialization\n"));

	// Allocate handles array, the maximum size is equal to the size of tuners array
	HANDLE* handles = new HANDLE[m_Tuners.size()];
	int count = 0;

	// Collect all the tuners initialization handles into the array
	for(UINT i = 0; i < m_Tuners.size(); i++)
		if(m_Tuners[i] != NULL && m_Tuners[i]->getInitializationEvent())
			handles[count++] = m_Tuners[i]->getInitializationEvent();
			
	// Wait on all the handles
	WaitForMultipleObjects(count, handles, TRUE, INFINITE);

	// Make a log entry that the setup is complete
	log(0, true, 0, TEXT("Encoder initialization is complete\n"));
}

bool Encoder::dumpECMCache(LPCTSTR fileName,
						   LPTSTR reason) const
{
	return m_pPluginsHandler != NULL ? m_pPluginsHandler->dumpECMCache(fileName, reason) : false;
}

bool Encoder::loadECMCache(LPCTSTR fileName,
						   LPTSTR reason)
{
	return m_pPluginsHandler != NULL ? m_pPluginsHandler->loadECMCache(fileName, reason) : false;
}

bool Encoder::startRecordingFromFile(LPCWSTR inFileName,
									 int sid,
									 __int64 duration,
									 LPCWSTR outFileName)
{
	// Create a file source
	FileSource* source = new FileSource(inFileName);

	// Create the recorder
	Recorder* recorder = new Recorder(m_pPluginsHandler, source, outFileName, true, sid, (USHORT)sid, TEXT(""), duration, this, -1, false);

	// Let's see if the recorder has an error, just delete it and exit
	if(recorder->hasError())
	{
		log(0, true, 0, TEXT("Cannot start recording!\n")),
			delete recorder;
		return false;
	}

	// Lock access to global structure
	m_cs.Lock();

	// Let's try to start playback
	bool succeeded = true;
	if(source->startPlayback(false))
		// If succeeded, add the recorder to the global structure
		m_Recorders.insert(pair<USHORT, Recorder*>(0, recorder));
	else
	{
		// Otherwise, delete everything
		succeeded = false;
		delete recorder;
		delete source;
	}

	// Unlock access to the global structure
	m_cs.Unlock();

	// If succeeded, try to start recording
	if(succeeded)
	{
		// Start the recording
		recorder->startRecording();
		// All is good
		return true;
	}
	else
	{
		// Make the log entry
		log(0, true, 0, TEXT("Cannot start recording!\n"));
		// And return false
		return false;
	}
}
