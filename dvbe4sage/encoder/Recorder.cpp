#include "StdAfx.h"

#include "Recorder.h"
#include "tuner.h"
#include "dvbfilter.h"
#include "pluginshandler.h"
#include "logger.h"
#include "configuration.h"
#include "virtualtuner.h"
#include "encoder.h"

DWORD WINAPI StopRecordingCallback(LPVOID vpRecorder)
{
	// Get the recorder first
	Recorder* recorder = (Recorder*)vpRecorder;

	// Loop infinitely
	while(!recorder->m_StopRecordingThreadCanEnd)
	{
		// Sleep for 1 second
		Sleep(1000);

		// Let's see if the time has passed
		// Get the current time
		time_t now;
		time(&now);

		// Calculate the differentce
		if((__int64)difftime(now, recorder->m_Time) > recorder->m_Duration)
		{
			// Log stop recording message
			log(0, true, TEXT("Time passed, "));

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
			log(0, true, TEXT("File operation failed, "));

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
			// Get the parser from the tuner
			DVBParser* const pParser = recorder->m_pTuner->getParser();

			// Let's see if it's valid and can be used
			if(pParser != NULL && !pParser->hasBeenCopied() && pParser->getTimeStamp() != 0 && 
				(__int64)difftime(now, pParser->getTimeStamp()) > g_Configuration.getPSIMaturityTime())
			{
				// If yes, get the encoder's parser
				DVBParser* const pEncoderParser = recorder->m_pEncoder->getParser();

				// Lock both parsers
				pEncoderParser->lock();

				// Copy the contents of the parser from the current tuner's parser
				pEncoderParser->copy(*pParser);

				// And unlock it
				pEncoderParser->unlock();

				// Set "HasBeenCopied" flag to prevent multiple copy
				pParser->setHasBeenCopied();
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
		// Sleep for 100 millisecs
		Sleep(100);

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
				   Tuner* const tuner,
				   USHORT logicalTuner,
				   LPCWSTR outFileName,
				   bool useSid,
				   const int channel,
				   const USHORT sid,
				   const __int64 duration,
				   Encoder* pEncoder,
				   const __int64 size,
				   bool bySage) :
	m_pPluginsHandler(plugins),
	m_pTuner(tuner),
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
	m_LogicalTuner(logicalTuner),
	m_FileName(outFileName),
	m_Size(size),
	m_VirtualTuner(NULL),
	m_StartRecordingThreadCanEnd(false),
	m_StopRecordingThreadCanEnd(false),
	m_StartRecordingThread(NULL),
	m_StopRecordingThread(NULL),
	m_StopRecordingThreadId(0),
	m_StartRecordingThreadId(0)
{
	// If this is standalone recording, bypass all this bullshit
	if(!bySage)
	{
		// Open the output file
		m_fout = _wfsopen(outFileName, L"wb", _SH_DENYWR);

		// Make it use no buffer if used as cyclic buffer
		if(g_Configuration.getDisableWriteBuffering() || m_Size != (__int64)-1)
			setvbuf(m_fout, NULL, _IONBF, 0);

		// Check if opening file succeeded
		if(m_fout == NULL)
		{
			log(0, true, TEXT("Cannot open the file \"%S\", error code=0x%.08X\n"), outFileName, GetLastError());
			m_HasError = true;
		}
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

			// Do a little time arithmetics excercise
			__declspec(align(8)) union timeunion
			{
				__int64 longlong;
				FILETIME fileTime;
			} st, ft;

			// Get the current system time
			SYSTEMTIME systemTime;
			GetSystemTime(&systemTime);
			// And convert it into file time
			SystemTimeToFileTime(&systemTime, &st.fileTime);

			do
			{
				// Check time diff only if the file is 0 size
				if(findData.nFileSizeHigh == 0 && findData.nFileSizeLow == 0)
				{
					// Get a creation time of our file
					ft.fileTime = findData.ftCreationTime;

					// Now, calculate the diff
					__int64 diff = st.longlong - ft.longlong;

					// If the diff is less than 10 seconds, this is our file
					// --- removed as not reliable enough
					//if(diff < 100000000)
						found = true;
				}
			}
			// Loop while the file is found or the search is exhausted
			while(!found && FindNextFileW(search, &findData));

			// Close the search handle
			CloseHandle(search);

			// If the file is found
			if(found)
			{
				// Construct its full real path
				wstring outFilePath(outFileName, (int)(wcsrchr(outFileName, L'\\') - outFileName) + 1);

				// Open the output file
				m_fout = _wfsopen((outFilePath + findData.cFileName).c_str(), L"wb", _SH_DENYWR);

				// Make it use no buffer if used as cyclic buffer
				if(g_Configuration.getDisableWriteBuffering() || m_Size != (__int64)-1)
					setvbuf(m_fout, NULL, _IONBF, 0);

				// Check if opening file succeeded
				if(m_fout == NULL)
				{
					log(0, true, TEXT("Cannot open the file \"%S\", error code=0x%.08X\n"), outFileName, GetLastError());
					m_HasError = true;
				}
			}
			else
			{
				log(0, true, TEXT("The file \"%S\" was not found!\n"), outFileName, GetLastError());
				m_HasError = true;
			}
		}
		else
		{
			log(0, true, TEXT("The file \"%S\" was not found!\n"), outFileName, GetLastError());
			m_HasError = true;
		}
	}
}

Recorder::~Recorder(void)
{
	// Nullify pointer to myself in my virtual tuner
	if(m_VirtualTuner != NULL)
		m_VirtualTuner->setRecorder(NULL);

	// Delete the parser
	delete m_pParser;

	// Close the output file
	if(m_fout != NULL)
		fclose(m_fout);

	// Close orphan thread handles
	if(m_StartRecordingThread != NULL)
		CloseHandle(m_StartRecordingThread);

	if(m_StopRecordingThread != NULL)
		CloseHandle(m_StopRecordingThread);
}

bool Recorder::startRecording()
{
	if(m_HasError)
	{
		log(0, true, TEXT("Cannot start recording!\n"));
		return false;
	}
	else
	{
		// Get the current time
		time(&m_Time);

		// Start the recording thread
		m_StartRecordingThread = CreateThread(NULL, 0, StartRecordingCallback, this, 0, &m_StartRecordingThreadId);

		return true;
	}
}

void Recorder::stopRecording()
{
	log(0, false, TEXT("stopping recording of channel %d on tuner=\"%s\", Ordinal=%d\n"),
		m_ChannelNumber, m_pTuner->getTunerFriendlyName(), m_pTuner->getTunerOrdinal());

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
	DVBParser* const pParser = m_pTuner->getParser();

	// Lock the parser
	pParser->lock();

	// Make the main parser stop sending packets to recorder's parser
	pParser->dropParser(m_pParser);

	// Unlock the parser
	pParser->unlock();

	// Tell the encoder to stop recording on this recorder
	m_pEncoder->stopRecording(this);
}

bool Recorder::changeState()
{
	// Get the parser
	DVBParser* const pParser = m_pTuner->getParser();
	
	// Lock the parser
	pParser->lock();

	// We make progress on this tuner only if its graph is running
	if(m_pTuner->running())
	{
		// Get the current time
		time_t now;
		time(&now);

		// Calculate the differentce
		if(difftime(now, m_Time) > g_Configuration.getTuningTimeout())
		{
			// Log stop recording message
			log(0, true, TEXT("Could not start recording after %u seconds, "), g_Configuration.getTuningTimeout());

			// Unlock the parser
			pParser->unlock();

			// Stop recording without locking the parser
			stopRecording();
						
			// The recording is not OK, delete the recorder
			return false;
		}

		// Check for tuner lock timeout
		if(!m_pTuner->getLockStatus() && difftime(now, m_Time) > g_Configuration.getTuningLockTimeout())
		{
			// Log stop recording message
			log(0, true, TEXT("Could not lock signal after %u seconds, "), g_Configuration.getTuningLockTimeout());

			// Unlock the parser
			pParser->unlock();

			// Stop recording without locking the parser
			stopRecording();
						
			// The recording is not OK, delete the recorder
			return false;
		}
		
		// Allow it to start the actual recording
		// Get the PMT PID for the recorder SID
		USHORT pmtPid = 0;
		if(pParser->getPMTPidForSid(m_Sid, pmtPid))
		{
			// Get the ES PIDs for the recorder SID
			hash_set<USHORT> esPids;
			if(pParser->getESPidsForSid(m_Sid, esPids))
			{
				// Get the CA PIDs for the recorder SID
				hash_set<USHORT> caPids;
				if(pParser->getCAPidsForSid(m_Sid, caPids))
				{
					// Get ECM CA Types (might be empty)
					hash_set<CAScheme> ecmCATypes;
					pParser->getECMCATypesForSid(m_Sid, ecmCATypes);
					// Get EMM CA Types (might be empty)
					EMMInfo emmCATypes;
					pParser->getEMMCATypes(emmCATypes);
					// Create the parser
					m_pParser = new ESCAParser(this, m_fout, m_pPluginsHandler, m_Sid, pmtPid, ecmCATypes, emmCATypes, m_Size);
					// Assign recorder's parser to PAT PID
					pParser->assignParserToPid(0, m_pParser);
					m_pParser->setESPid(0, true);
					// Assign recorder's parser to PMT PID
					pParser->assignParserToPid(pmtPid, m_pParser);
					m_pParser->setESPid(pmtPid, true);
					// Assign recorder's parser to every relevant ES PID
					for(hash_set<USHORT>::const_iterator it = esPids.begin(); it != esPids.end(); it++)
					{
						pParser->assignParserToPid(*it, m_pParser);
						m_pParser->setESPid(*it, true);
					}
					// Assign recorder's parser to every relevant CA PID
					for(hash_set<USHORT>::const_iterator it = caPids.begin(); it != caPids.end(); it++)
					{
						pParser->assignParserToPid(*it, m_pParser);
						m_pParser->setESPid(*it, false);
					}
					// Finally, tell the parser it has connected clients
					pParser->setHasConnectedClients();

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
		// The tuner's graph is not running, so we can indicate that the start recording thread can end
		m_StartRecordingThreadCanEnd = true;

	// Unlock the parser
	pParser->unlock();

	// Everything is OK
	return true;
}
