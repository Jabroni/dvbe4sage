#pragma once

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
	void valog(UINT logLevel, bool timeStamp, LPCTSTR format, va_list argList);

	LPCTSTR getLogFileName() const	{ return m_LogFileName; }
};

void log(UINT logLevel, bool timeStamp, LPCTSTR format, ...);