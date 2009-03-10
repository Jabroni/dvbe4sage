// encoder.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "encoder.h"

#include <share.h>
#include "DVBParser.h"
#include "graph.h"
#include "DVBFilter.h"
#include "Logger.h"
#include "tuner.h"
#include "recorder.h"
#include "configuration.h"
#include "virtualtuner.h"
#include "misc.h"

Encoder::Encoder(HINSTANCE hInstance, HWND hWnd, HMENU hParentMenu) :
	m_pPluginsHandler(NULL),
	m_hWnd(hWnd),
	m_pParser(new DVBParser)
{
	// Set the logger level
	g_Logger.setLogLevel(g_Configuration.getLogLevel());

	// Initialize COM stuff
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	// Initialize plugins (to be flag dependent)
	m_pPluginsHandler = new PluginsHandler(hInstance, hWnd, hParentMenu);

	// Initialize tuners
	for(int i = 0; i < CBDAFilterGraph::getNumberOfTuners(); i++)
		if(!g_Configuration.excludeTuner(i + 1))
		{
			Tuner* tuner = new Tuner(this,
									 i + 1,
									 g_Configuration.getInitialFrequency(),
									 g_Configuration.getInitialSymbolRate(),
									 g_Configuration.getInitialPolarization(),
									 g_Configuration.getInitialModulation(),
									 g_Configuration.getInitialFEC());
			if(tuner->isTunerOK() && tuner->runIdle())
				if(g_Configuration.isDVBS2Tuner(tuner->getTunerOrdinal()))
					m_Tuners.push_back(tuner);
				else
					m_Tuners.insert(m_Tuners.begin(), tuner);
		}

	// Initialize Winsock layer
	WSAData wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	// Create some virtual tuners
	for(USHORT i = 0; i < g_Configuration.getNumberOfVirtualTuners(); i++)
	{
		VirtualTuner* const virtualTuner = new VirtualTuner(g_Configuration.getListeningPort() + i, hWnd);
		m_VirtualTuners.push_back(virtualTuner);
		m_VirtualTunersMap[virtualTuner->getServerSocket()] = virtualTuner;
	}
}
	

Encoder::~Encoder()
{
	// For all recorders
	while(!m_Recorders.empty())
	{
		g_Logger.log(0, true, TEXT("Aborting, "));
		// Get the recorder pointer
		Recorder* recorder = m_Recorders.begin()->second;
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
		Tuner* tuner = m_Tuners[i];
		// And delete it
		delete tuner;
	}

	// Delete the parser
	delete m_pParser;

	// Delete plugins handler
	delete m_pPluginsHandler;	

	// Cleanup Winsock layer
	WSACleanup();

	// Uninitialize COM
	CoUninitialize();
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
					g_Logger.log(1, true, TEXT("Received command: \"%.*S\"\n"), fullCommand.length() - 2, fullCommand.c_str());

					// Determine the command itsels
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
						g_Logger.log(0, true, TEXT("Received START command to start recording on source \"%S\", channel=%d, duration=%I64d, file=\"%S\"\n"),
							source.c_str(), channelInt, durationInt, fileName.c_str());

						// Start the recording itself
						startRecording(true, 0, 0, BDA_POLARISATION_NOT_SET, BDA_MOD_NOT_SET, BDA_BCC_RATE_NOT_SET, -1, true, channelInt, g_Configuration.getUseSidForTuning(), durationInt, fileName.c_str(), virtualTuner, (__int64)-1);

						// Report OK code to the client
						send(socket, "OK\r\n", 4, 0);
					}
					else if(command == L"STOP")
					{
						g_Logger.log(0, true, TEXT("Requested by Sage, "));
						// Stop ongoing recording
						Recorder* recorder = virtualTuner->getRecorder();
						if(recorder != NULL)
						{
							recorder->stopRecording();
							// And delete the recorder
							delete recorder;
						}
						else
							g_Logger.log(0, false, TEXT(" there is no recorder to stop!\n"));
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
						g_Logger.log(0, true, TEXT("Received BUFFER command to start recording on source \"%S\", channel=%d, size=%I64d, file=\"%S\"\n"),
							source.c_str(), channelInt, sizeInt, fileName.c_str());

						// Start the actual recording
						startRecording(true, 0, 0, BDA_POLARISATION_NOT_SET, BDA_MOD_NOT_SET, BDA_BCC_RATE_NOT_SET, -1, true, channelInt, g_Configuration.getUseSidForTuning(), 3600, fileName.c_str(), virtualTuner, sizeInt);

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
							g_Logger.log(0, true, TEXT("Replied %.*s\n"), length - 2, buffer);
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
						g_Logger.log(0, true, TEXT("Received unknown command \"%S\", ignoring...\n"), fullCommand.c_str());
				}
				else
					g_Logger.log(0, true, TEXT("Received command from client that already left, ignoring...\n"));
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

Tuner* Encoder::getTuner(int tunerOrdinal,
						 bool useLogicalTuner,
						 int channel,
						 bool useSid)
{
	Tuner* tuner = NULL;
	if(useLogicalTuner)
	{
		// Use internal parser
		// Lock it
		m_pParser->lock();
		
		// Get SID
		USHORT sid = 0;
		if(useSid)
			sid = (USHORT)channel;

		if(useSid || m_pParser->getSidForChannel((USHORT)channel, sid))
		{
			// Get transponder
			Transponder transponder;
			if(m_pParser->getTransponderForSid(sid, transponder))
			{
				// Now let's find an appropriate tuner
				// Let's see if there isn't one already tuned to this TID
				for(UINT i = 0; i < m_Tuners.size(); i++)
					if(m_Tuners[i]->running() && m_Tuners[i]->getTid() == transponder.tid)
					{
						tuner = m_Tuners[i];
						break;
					}
				// If there is no sutable tuner already in use
				if(tuner == NULL)
					for(UINT i = 0; i < m_Tuners.size(); i++)
						// The tuner we're looking for should be not running and if the program requires DVB-S2
						// we should pick only a compatible tuner
						if(!m_Tuners[i]->running() && (transponder.modulation == BDA_MOD_QPSK || 
							g_Configuration.isDVBS2Tuner(m_Tuners[i]->getTunerOrdinal())))
							{
								tuner = m_Tuners[i];
								break;
							}
			}
		}
		
		// Unlock the parser
		m_pParser->unlock();
	}
	else
		// Here we just pull the right physical tuner
		for(UINT i = 0; i < m_Tuners.size(); i++)
		{
			if(m_Tuners[i]->getTunerOrdinal() == tunerOrdinal)
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
							 __int64 size)
{
	// Get the right tuner object
	Tuner* tuner = getTuner(tunerOrdinal, useLogicalTuner, channel, useSid);

	// Boil out if an appropriate tuner is not found
	if(tuner == NULL)
	{
		g_Logger.log(0, true, TEXT("Cannot start recording for %s=%d, no sutable tuner found!\n"),
			useSid ? TEXT("SID") : TEXT("Channel"), channel);
		return false;
	}
	
	// Make a log entry
	if(autodiscoverTransponder)
		g_Logger.log(0, true, TEXT("Starting recording on tuner=\"%s\", Ordinal=%d, %s=%d, Transponder Autodiscovery=TRUE, Duration=%I64d\n"),
			tuner->getTunerFriendlyName(), tuner->getTunerOrdinal(), useSid ? TEXT("SID") : TEXT("Channel"), channel, duration);
	else
		g_Logger.log(0, true, TEXT("Starting recording on tuner=\"%s\", Ordinal=%d, %s=%d, Transponder Autodiscovery=FALSE, Duration=%I64d, Frequency=%lu, Symbol Rate=%lu, Polarization=%s, Modulation=%s, FEC=%s\n"),
			tuner->getTunerFriendlyName(), tuner->getTunerOrdinal(), useSid ? TEXT("SID") : TEXT("Channel"), channel, duration,
			frequency, symbolRate, printablePolarization(polarization),	printableModulation(modulation), printableFEC(fecRate));

	// If we found the tuner
	if(tuner != NULL)
	{
		// Create the recorder
		Recorder* recorder = new Recorder(m_pPluginsHandler, tuner, (USHORT)tunerOrdinal, outFileName, useSid, channel, duration, this, size);

		// Let's see if the recorder has an error, just delete it and exit
		if(recorder->hasError())
		{
			g_Logger.log(0, true, TEXT("Cannot start recording!\n")),
			delete recorder;
			return false;
		}

		// Lock access to glbal structure
		m_cs.Lock();

		// Let's see if the tuner is already used
		bool bTunerUsed = false;
		for(hash_multimap<USHORT, Recorder*>::iterator it = m_Recorders.begin(); it != m_Recorders.end(); it++)
			if(it->second->getTuner() == tuner)
			{
				bTunerUsed = true;
				break;
			}

		// Add it to the list
		m_Recorders.insert(pair<USHORT, Recorder*>((USHORT)tunerOrdinal, recorder));

		// Assign this recorder to the virtual tuner if needed
		if(virtualTuner != NULL)
			virtualTuner->setRecorder(recorder);

		// Unlock access to glbal structure
		m_cs.Unlock();

		// Tune the tuner if necessary
		if(!bTunerUsed && !autodiscoverTransponder)
			tuner->tune(frequency, symbolRate, polarization, modulation, fecRate);

		// Tell tuner it should start recording
		tuner->startRecording(bTunerUsed, autodiscoverTransponder);

		// Same for the recorder
		return recorder->startRecording();
	}
	else
		return false;
}

bool Encoder::stopRecording(Recorder* recorder)
{
	// Lock access to glbal structure
	CAutoLock lock(&m_cs);

	// Nullify last recorder if necessary
	/*if(recorder == m_LastRecorder)
		m_LastRecorder = NULL;*/

	// Get the tuner of the recorder
	Tuner* tuner = recorder->getTuner();

	// Remove the recorder
	for(hash_map<USHORT, Recorder*>::iterator it = m_Recorders.begin(); it != m_Recorders.end(); it++)
		if(it->second == recorder)
		{
			m_Recorders.erase(it);
			break;
		}

	// Get the parser from the recorder and tell plugins handler to start ignoring it
	m_pPluginsHandler->removeCaller(recorder->getParser());

	// Let's see if the tuner is still used
	bool bTunerUsed = false;
	for(hash_map<USHORT, Recorder*>::iterator it = m_Recorders.begin(); it != m_Recorders.end(); it++)
		if(it->second->getTuner() == tuner)
		{
			bTunerUsed = true;
			break;
		}

	// If no, stop running graph on it
	if(!bTunerUsed)
		tuner->stopRecording();

	return true;
}

int Encoder::getNumberOfTuners() const
{
	return m_Tuners.size();
}

LPCTSTR Encoder::getTunerFriendlyName(int i) const
{
	return m_Tuners[i]->getTunerFriendlyName();
}

int Encoder::getTunerOrdinal(int i) const
{
	return m_Tuners[i]->getTunerOrdinal();
}
