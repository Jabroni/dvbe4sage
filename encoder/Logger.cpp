#include "StdAfx.h"
#include "Logger.h"

// This is the global logger
ENCODER_API Logger g_Logger;

Logger::Logger(void) : 
	m_LogLevel(0),
	m_LogFile(NULL)
{
	// Get current time in UCT
    time_t currentUTCTime;
    time(&currentUTCTime);

    // Convert time to struct tm form 
    tm currentLocalTime;
	localtime_s(&currentLocalTime, &currentUTCTime);

	// Create filename
	_stprintf_s(m_LogFileName, TEXT("%d%02d%02d%02d%02d%02d.txt"), 
							currentLocalTime.tm_year + 1900,
							currentLocalTime.tm_mon + 1,
							currentLocalTime.tm_mday,
							currentLocalTime.tm_hour,
							currentLocalTime.tm_min,
							currentLocalTime.tm_sec);

	// Open the log file
	m_LogFile = _tfsopen(m_LogFileName, TEXT("w"),  _SH_DENYWR);
	// Make it use no buffer
	setvbuf(m_LogFile, NULL, _IONBF, 0);
	// Initialize the clitical section
	InitializeCriticalSection(&m_cs);
}

Logger::~Logger(void)
{
	// Close the log file
	if(m_LogFile != NULL)
		fclose(m_LogFile);
	// Delete the critical section
	DeleteCriticalSection(&m_cs);
}

void Logger::log(UINT logLevel,
				 bool timeStamp,
				 LPCTSTR format, ...)
{
	// Do this in critical section
	EnterCriticalSection(&m_cs);

	va_list argList;
	va_start(argList, format);

	// Write log only if the level of the message is lower than or equal to the requested level
	if(logLevel <= m_LogLevel)
	{
		// Get current time in UCT
		time_t currentUTCTime;
		time(&currentUTCTime);

		// Convert time to struct tm form 
		tm currentLocalTime;
		localtime_s(&currentLocalTime, &currentUTCTime);

		if(timeStamp)
			_ftprintf(m_LogFile, TEXT("%d-%02d-%02d %02d:%02d:%02d "), 
								currentLocalTime.tm_year + 1900,
								currentLocalTime.tm_mon + 1,
								currentLocalTime.tm_mday,
								currentLocalTime.tm_hour,
								currentLocalTime.tm_min,
								currentLocalTime.tm_sec);
		_vftprintf(m_LogFile, format, argList);
	}
	// Leave the critical section
	LeaveCriticalSection(&m_cs);
}

void Logger::setLogLevel(UINT level)
{
	// Do this in critical section
	EnterCriticalSection(&m_cs);

	// Set the log level
	m_LogLevel = level;

	// Leave the critical section
	LeaveCriticalSection(&m_cs);
}
