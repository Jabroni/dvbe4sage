#include "StdAfx.h"
#include "Recorder.h"
#include "tuner.h"
#include "dvbfilter.h"
#include "pluginshandler.h"
#include "logger.h"
#include "configuration.h"
#include "virtualtuner.h"

VOID NTAPI StopRecordingCallback(PVOID vpRecorder,
								 BOOLEAN alwaysTrue)
{
	// Get the recorder first
	Recorder* recorder = (Recorder*)vpRecorder;

	// Let's see if the time has passed
	// Get the current time
	time_t now;
	time(&now);

	// Calculate the differentce
	if((__int64)difftime(now, recorder->m_Time) > recorder->m_Duration)
	{
		// Log stop recording message
		g_Logger.log(0, true, TEXT("Time passed, "));

		// Tell it to stop recording
		recorder->stopRecording();

		// Delete the recorder object
		delete recorder;
	}
	else if(recorder->brokenPipe())
	{
		// Log stop recording message
		g_Logger.log(0, true, TEXT("File operation failed, "));

		// Tell it to stop recording
		recorder->stopRecording();

		// Delete the recorder object
		delete recorder;
	}
}

VOID NTAPI StartRecordingCallback(PVOID vpRecorder,
								  BOOLEAN alwaysTrue)
{
	// Get the recorder first
	Recorder* recorder = (Recorder*)vpRecorder;

	// Delete the recorder if necessary
	if(!recorder->changeState())
		delete recorder;
}

Recorder::Recorder(PluginsHandler* const plugins,
				   Tuner* const tuner,
				   USHORT logicalTuner,
				   LPCTSTR outFileName,
				   bool useSid,
				   const int channel,
				   const __int64 duration,
				   Encoder* pEncoder,
				   const __int64 size) :
	m_pPluginsHandler(plugins),
	m_pTuner(tuner),
	m_fout(NULL),
	m_IsRecording(false),
	m_HasError(false),
	m_CurrentState(INITIAL),
	m_RecordingTimer(NULL),
	m_ChannelNumber(channel),
	m_Duration(duration),
	m_pParser(NULL),
	m_pEncoder(pEncoder),
	m_TimerQueue(NULL),
	m_IsBrokenPipe(false),
	m_UseSid(useSid),
	m_LogicalTuner(logicalTuner),
	m_FileName(outFileName),
	m_Size(size),
	m_VirtualTuner(NULL)
{
	// Create the timer queue
	m_TimerQueue = CreateTimerQueue();

	// Open the output file
	m_fout = _tfsopen(outFileName, TEXT("wb"), _SH_DENYWR);

	// Make it use no buffer if used as cyclic buffer
	if(g_Configuration.getDisableWriteBuffering() || m_Size != (__int64)-1)
		setvbuf(m_fout, NULL, _IONBF, 0);

	// Check if opening file succeeded
	if(m_fout == NULL)
	{
		g_Logger.log(0, true, TEXT("Cannot open the file \"%s\", error code=0x%.08X\n"), outFileName, GetLastError());
		m_HasError = true;
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

	// Delete the timer queue
	DeleteTimerQueue(m_TimerQueue);
}

bool Recorder::startRecording()
{
	if(m_HasError)
	{
		g_Logger.log(0, true, TEXT("Cannot start recording!\n"));
		return false;
	}
	else
	{
		// Mark the state as initial
		m_CurrentState = INITIAL;

		// Get the current time
		time(&m_Time);

		// Create recording timer
		CreateTimerQueueTimer(&m_RecordingTimer,
							  m_TimerQueue,
							  StartRecordingCallback,
							  this,
							  100,
							  100,
							  WT_EXECUTEDEFAULT);
		return true;
	}
}

void Recorder::stopRecording()
{
	g_Logger.log(0, false, TEXT("stopping recording of channel %d on tuner=\"%s\", Ordinal=%d\n"),
		m_ChannelNumber, m_pTuner->getTunerFriendlyName(), m_pTuner->getTunerOrdinal());

	// Delete the timer
	DeleteTimerQueueTimer(m_TimerQueue, m_RecordingTimer, NULL);

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
	if(m_pTuner->running() && m_CurrentState != TUNED)
	{
		// Get the current time
		time_t now;
		time(&now);

		// Calculate the differentce
		if(difftime(now, m_Time) > g_Configuration.getTuningTimeout())
		{
			// Log stop recording message
			g_Logger.log(0, true, TEXT("Timeout, "));

			// Unlock the parser
			pParser->unlock();

			// Stop recording without locking the parser
			stopRecording();
						
			// The recording is not OK, delete the recorder
			return false;
		}
		
		// Depending on the current state
		switch(m_CurrentState)
		{
			case INITIAL:
			{
				// On initial, we say the graph to tune to the channel
				if(m_pTuner->tuneChannel(m_ChannelNumber, m_UseSid, m_Sid))
					m_CurrentState = Recorder::TUNING;
				break;
			}
			case TUNING:
			{
				// Then we allow it to start the actual recording
				// Get the PMT PID for the recorder SID
				USHORT pmtPid = 0;
				if(pParser->getEMMPid() != 0 && pParser->getPMTPidForSid(m_Sid, pmtPid))
				{
					// Get the ES PIDs for the recorder SID
					hash_set<USHORT> esPids;
					if(pParser->getESPidsForSid(m_Sid, esPids))
					{
						// Get the CA PIDs for the recorder SID
						hash_set<USHORT> caPids;
						if(pParser->getCAPidsForSid(m_Sid, caPids))
						{
							// Get CA Types (might be empty)
							hash_set<USHORT> caTypes;
							pParser->getCATypesForSid(m_Sid, caTypes);
							// Create the parser
							m_pParser = new ESCAParser(this, m_fout, m_pPluginsHandler, m_Sid, pmtPid, caTypes, pParser->getEMMPid(), m_Size);
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

							// Idicate the current recorder state as tuned
							m_CurrentState = TUNED;
							// Delete the old timer
							DeleteTimerQueueTimer(m_TimerQueue, m_RecordingTimer, NULL);
							// Save the time of the beginning of recording
							time(&m_Time);
							// Create the new timer to stop the recording based on requested duration
							CreateTimerQueueTimer(&m_RecordingTimer,
												  m_TimerQueue, 
												  StopRecordingCallback,
												  this,
												  1000,
												  1000,
												  WT_EXECUTEDEFAULT);
						}
					}
				}
				break;
			}
			default:
				break;
		}
	}

	// Unlock the parser
	pParser->unlock();

	// Everything is OK
	return true;
}
