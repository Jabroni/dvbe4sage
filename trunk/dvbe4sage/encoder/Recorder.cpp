#include "StdAfx.h"

#include "Recorder.h"
#include "dvbfilter.h"
#include "pluginshandler.h"
#include "logger.h"
#include "configuration.h"
#include "virtualtuner.h"
#include "encoder.h"
#include "GenericSource.h"
#include "FileSource.h"
#include "GrowlHandler.h"

DWORD WINAPI StopRecordingCallback(LPVOID vpRecorder)
{
	// Get the recorder first
	Recorder* recorder = (Recorder*)vpRecorder;

	// Loop infinitely
	while(!recorder->m_StopRecordingThreadCanEnd)
	{
		// Sleep for 100 milliseconds
		Sleep(100);

		// Let's see if the time has passed
		// Get the current time
		time_t now = 0;
		time(&now);

		// Calculate the difference
		if((__int64)difftime(now, recorder->m_Time) > recorder->m_Duration)
		{
			// Log stop recording message
			log(0, true, recorder->getSource()->getSourceOrdinal(), TEXT("Time passed, "));

			// Tell it to stop recording
			recorder->stopRecording();

			// Delete the recorder object
			delete recorder;

			// Boil out
			break;
		}
		else if(recorder->brokenPipe())
		{
			// Log stop recording message
			log(0, true, recorder->getSource()->getSourceOrdinal(), TEXT("File operation failed, "));

			// Tell it to stop recording
			recorder->stopRecording();

			// Delete the recorder object
			delete recorder;

			// Boil out
			break;
		}
		else
		{
			// Here we update the encoder with the latest and greatest PSI parser info
			// Get the parser from the source
			DVBParser* const sourceParser = recorder->m_pSource->getParser();
			
			// Let's see if it's valid and can be used
			if(sourceParser != NULL && sourceParser->getNetworkProvider().canBeCopied() && !sourceParser->providerInfoHasBeenCopied() && sourceParser->getTimeStamp() != 0 && 
				(__int64)difftime(now, sourceParser->getTimeStamp()) > g_pConfiguration->getPSIMaturityTime())
			{
				
				// If yes, get the encoder's network provider
				NetworkProvider& encoderNetworkProvider = recorder->m_pEncoder->getNetworkProvider();
	
				// If enabled, we dump the services and transponders to file after the timeout of scanning the PSI for new services
				if(g_pConfiguration->getCacheServices()) {
					TCHAR reason[MAX_ERROR_MESSAGE_SIZE];
					recorder->m_pEncoder->dumpNetworkProvider(reason);
				}

				// Lock network provider
				encoderNetworkProvider.lock();

				// Copy the contents of the network provider
				encoderNetworkProvider.copy(sourceParser->getNetworkProvider());

				// And unlock the encoder provider
				encoderNetworkProvider.unlock();

				// Set "HasBeenCopied" flag to prevent multiple copy
				sourceParser->setProviderInfoHasBeenCopied();	
				
			}			
		}
	}

	return 0;
}

DWORD WINAPI StartRecordingCallback(LPVOID vpRecorder)
{
	// Get the recorder first
	Recorder* recorder = (Recorder*)vpRecorder;

	// Loop while can exit
	while(!recorder->m_StartRecordingThreadCanEnd)
	{
		// Sleep for 10 milliseconds
		Sleep(10);

		// Try to change the recorder state
		if(!recorder->changeState())
		{
			// If something went wrong, delete the recorder and boil out
			delete recorder;
			break;
		}
	}

	return 0;
}

Recorder::Recorder(PluginsHandler* const plugins,
				   GenericSource* const source,
				   LPCWSTR outFileName,
				   bool useSid,
				   const int channel,
				   const USHORT sid,
				   const USHORT onid,
				   LPCTSTR channelName,
				   const __int64 duration,
				   Encoder* pEncoder,
				   const __int64 size,
				   bool bySage,
				   bool forEIT) :
	m_pPluginsHandler(plugins),
	m_pSource(source),
	m_fout(NULL),
	m_IsRecording(false),
	m_HasError(false),
	m_ChannelNumber(channel),
	m_Duration(duration),
	m_pParser(NULL),
	m_pEncoder(pEncoder),
	m_IsBrokenPipe(false),
	m_UseSid(useSid),
	m_Sid(sid),
	m_Onid(onid),
	m_FileName(outFileName),
	m_Size(size),
	m_StartRecordingThreadCanEnd(false),
	m_StopRecordingThreadCanEnd(false),
	m_StartRecordingThread(NULL),
	m_StopRecordingThread(NULL),
	m_StopRecordingThreadId(0),
	m_StartRecordingThreadId(0),
	m_ForEIT(forEIT)
{
	// If the source is not OK, boil out first
	if(!source->isSourceOK())
	{
		m_HasError = true;
		return;
	}
	// Copy the channel name
	_tcscpy_s(m_ChannelName, sizeof(m_ChannelName) / sizeof(m_ChannelName[0]), channelName);

	// If this is standalone recording, bypass all this bullshit
	if(!bySage)
	{
		// Open the output file
		m_fout = _wfsopen(outFileName, L"wb", _SH_DENYWR);

		// Check if opening file succeeded
		if(m_fout == NULL)
		{
			log(0, true, getSource()->getSourceOrdinal(), TEXT("Cannot open the file \"%s\", error code=0x%.08X\n"), CW2CT(outFileName), GetLastError());
			m_HasError = true;
		}

		// Make it use no buffer if used as cyclic buffer
		if(g_pConfiguration->getDisableWriteBuffering() || m_Size != (__int64)-1)
			setvbuf(m_fout, NULL, _IONBF, 0);
	}
	else
	{
		// Start searching the file
		WIN32_FIND_DATAW findData;
		HANDLE search = FindFirstFileW(outFileName, &findData);

		// Go inside only if a valid file was found
		if(search != INVALID_HANDLE_VALUE)
		{
			// In the beginning the file is not found
			bool found = false;
			do
			{
				// Check time difference only if the file is 0 size
				if(findData.nFileSizeHigh == 0 && findData.nFileSizeLow == 0)
						found = true;
			}
			// Loop while the file is found or the search is exhausted
			while(!found && FindNextFileW(search, &findData));

			// Close the search handle
			FindClose(search);

			// If the file is found
			if(found)
			{
				// Construct its full real path
				wstring outFilePath(outFileName, (int)(wcsrchr(outFileName, L'\\') - outFileName) + 1);

				// Open the output file
				m_fout = _wfsopen((outFilePath + findData.cFileName).c_str(), L"wb", _SH_DENYWR);

				// Check if opening file succeeded
				if(m_fout == NULL)
				{
					log(0, true, getSource()->getSourceOrdinal(), TEXT("Cannot open the file \"%s\", error code=0x%.08X\n"), CW2CT(outFileName), GetLastError());
					m_HasError = true;
				}

				// Make it use no buffer if used as cyclic buffer
				if(g_pConfiguration->getDisableWriteBuffering() || m_Size != (__int64)-1)
					setvbuf(m_fout, NULL, _IONBF, 0);
			}
			else
			{
				log(0, true, getSource()->getSourceOrdinal(), TEXT("The file \"%s\" was not found!\n"), CW2CT(outFileName), GetLastError());
				m_HasError = true;
			}
		}
		else
		{
			log(0, true, getSource()->getSourceOrdinal(), TEXT("The file \"%s\" was not found!\n"), CW2CT(outFileName), GetLastError());
			m_HasError = true;
		}
	}
}

Recorder::~Recorder(void)
{
	// Delete the parser
	delete m_pParser;

	// Close the output file
	if(m_fout != NULL)
		fclose(m_fout);

	// If the recorder works on a FileSource kind of source, also delete it
	if(dynamic_cast<FileSource*>(m_pSource) != NULL)
		delete m_pSource;

	// Close orphan thread handles
	if(m_StartRecordingThread != NULL)
		CloseHandle(m_StartRecordingThread);

	if(m_StopRecordingThread != NULL)
		CloseHandle(m_StopRecordingThread);
}

void Recorder::startRecording()
{
	// Get the current time
	time(&m_Time);

	// Start the recording thread
	m_StartRecordingThread = CreateThread(NULL, 0, StartRecordingCallback, this, 0, &m_StartRecordingThreadId);
}

void Recorder::stopRecording()
{
	if(g_pConfiguration->getGrowlNotification() && g_pConfiguration->getGrowlNotifyOnTune()) 
			g_pGrowlHandler->SendNotificationMessage(NOTIFICATION_ONTUNE, "Stop Recording Event", 
				TEXT("Stopping recording of %s %d (\"%s\") on source=\"%s\", Ordinal=%d"),
				!m_UseSid ? TEXT("channel") : TEXT("service"), m_ChannelNumber, m_ChannelName, m_pSource->getSourceFriendlyName(), m_pSource->getSourceOrdinal()); 
							
	log(0, false, 0, TEXT("Stopping recording of %s %d (\"%s\") on source=\"%s\", Ordinal=%d\n"),
		!m_UseSid ? TEXT("channel") : TEXT("service"), m_ChannelNumber, m_ChannelName, m_pSource->getSourceFriendlyName(), m_pSource->getSourceOrdinal());

	// Make sure start recording test is finished
	m_StartRecordingThreadCanEnd = true;

	// If needed, wait for its end
	if(m_StartRecordingThread != NULL && GetCurrentThreadId() != m_StartRecordingThreadId)
		WaitForSingleObject(m_StartRecordingThread, INFINITE);

	// Make sure stop recording test is finished too
	m_StopRecordingThreadCanEnd = true;

	// If needed, wait for its end
	if(m_StopRecordingThread != NULL && GetCurrentThreadId() != m_StopRecordingThreadId)
		WaitForSingleObject(m_StopRecordingThread, INFINITE);

	// Get the main parser
	DVBParser* const sourceParser = m_pSource->getParser();

	if(sourceParser != NULL)
	{
		// Lock the parser
		sourceParser->lock();

		// Make the main parser stop sending packets to recorder's parser
		sourceParser->dropParser(m_pParser);

		// Unlock the parser
		sourceParser->unlock();
	}

	// Tell the encoder to stop recording on this recorder
	m_pEncoder->stopRecording(this);
}

bool Recorder::changeState()
{
	// Get the parser
	DVBParser* const sourceParser = m_pSource->getParser();

	if(sourceParser != NULL)
	{
		// Lock the parser
		sourceParser->lock();

		// We make progress on this source only if its graph is running
		if(m_pSource->running())
		{
			// Get the current time
			time_t now = 0;
			time(&now);

			// Calculate the difference
			if(difftime(now, m_Time) > g_pConfiguration->getTuningTimeout())
			{
				// Log stop recording message
				log(0, true, getSource()->getSourceOrdinal(), TEXT("Could not start recording after %u seconds, "), g_pConfiguration->getTuningTimeout());

				// Unlock the parser
				sourceParser->unlock();

				// Stop recording without locking the parser
				stopRecording();
							
				// The recording is not OK, delete the recorder
				return false;
			}

			// Check for source lock timeout (any source other than physical tuner should return immediate success)
			if(!m_pSource->getLockStatus() && difftime(now, m_Time) > g_pConfiguration->getTuningLockTimeout())
			{
				// Log stop recording message
				log(0, true, getSource()->getSourceOrdinal(), TEXT("Could not lock signal after %u seconds, "), g_pConfiguration->getTuningLockTimeout());

				// Unlock the parser
				sourceParser->unlock();

				// Stop recording without locking the parser
				stopRecording();
							
				// The recording is not OK, delete the recorder
				return false;
			}
			
			// Allow it to start the actual recording
			// Get the PMT PID for the recorder SID
			USHORT pmtPid = 0;
			if(sourceParser->getPMTPidForSid(m_Sid, pmtPid))
			{
				// Get the ES PIDs for the recorder SID
				hash_set<USHORT> esPids;
				if(sourceParser->getESPidsForSid(m_Sid, esPids))
				{
					// Get the CA PIDs for the recorder SID
					hash_set<USHORT> caPids;
					if(sourceParser->getCAPidsForSid(m_Sid, caPids))
					{
						// Get ECM CA Types (might be empty)
						hash_set<CAScheme> ecmCATypes;
						sourceParser->getECMCATypesForSid(m_Sid, ecmCATypes);
						// Get EMM CA Types (might be empty)
						EMMInfo emmCATypes;
						sourceParser->getEMMCATypes(emmCATypes);
						// Create the parser
						m_pParser = new ESCAParser(sourceParser, this, m_fout, m_pPluginsHandler, m_ChannelName, m_Sid, pmtPid, ecmCATypes, emmCATypes, m_Size);
						// Assign recorder's parser to PAT PID
						sourceParser->assignParserToPid(0, m_pParser);
						m_pParser->setESPid(0, false);
						// Assign recorder's parser to PMT PID
						sourceParser->assignParserToPid(pmtPid, m_pParser);
						m_pParser->setESPid(pmtPid, false);
						// Assign recorder's parser to every relevant ES PID
						for(hash_set<USHORT>::const_iterator it = esPids.begin(); it != esPids.end(); it++)
						{
							sourceParser->assignParserToPid(*it, m_pParser);
							m_pParser->setESPid(*it, true);
							m_pParser->setValidatePid(*it, ESCAParser::validateType(sourceParser->getTypeForPid(*it)));
						}
						// Assign recorder's parser to every relevant CA PID
						for(hash_set<USHORT>::const_iterator it = caPids.begin(); it != caPids.end(); it++)
						{
							sourceParser->assignParserToPid(*it, m_pParser);
							m_pParser->setESPid(*it, false);
						}
						// Add CAT to the parser as well, saying it's not an ES type
						sourceParser->assignParserToPid(1, m_pParser);
						m_pParser->setESPid(1, false);
						// Finally, tell the parser it has connected clients
						sourceParser->setHasConnectedClients();

						// Indicate start recording thread can end
						m_StartRecordingThreadCanEnd = true;

						// Save the time of the beginning of recording
						time(&m_Time);

						// Start the "stop recording" thread
						m_StopRecordingThread = CreateThread(NULL, 0, StopRecordingCallback, this, 0, &m_StopRecordingThreadId);
					}
				}
			}
		}
		else
			// The source's graph is not running, so we can indicate that the start recording thread can end
			m_StartRecordingThreadCanEnd = true;

		// Unlock the parser
		sourceParser->unlock();
	}

	// Everything is OK
	return true;
}

__int64 Recorder::getFileLength() const
{
	return m_pParser != NULL ? m_pParser->getFileLength() : (__int64)0;
}
