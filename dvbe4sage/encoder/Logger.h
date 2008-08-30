#pragma once

#include "encoder.h"

// This is the logger classs
class Logger
{
	CRITICAL_SECTION	m_cs;
	FILE*				m_LogFile;
	UINT				m_LogLevel;
	TCHAR				m_LogFileName[19];
public:
	Logger(void);
	virtual ~Logger(void);
	void setLogLevel(UINT level);
	void log(UINT logLevel, bool timeStamp, LPCTSTR format, ...);
	LPCTSTR getLogFileName() { return m_LogFileName; }
};

// This is a reference to the global logger
extern ENCODER_API Logger g_Logger;