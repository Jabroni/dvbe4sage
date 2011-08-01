#pragma once

class ESCAParser;
class GenericSource;
class PluginsHandler;
class Encoder;

using namespace std;

class Recorder
{
	friend DWORD WINAPI StartRecordingCallback(LPVOID vpRecorder);
	friend DWORD WINAPI StopRecordingCallback(LPVOID vpRecorder);
private:
	// Disallow default and copy constructors
	Recorder();
	Recorder(const Recorder&);

	// Data members
	PluginsHandler* const		m_pPluginsHandler;
	ESCAParser*					m_pParser;
	FILE*						m_fout;
	bool						m_IsRecording;
	bool						m_HasError;
	const int					m_ChannelNumber;
	const __int64				m_Duration;
	GenericSource* const		m_pSource;
	const USHORT				m_Sid;
	const USHORT				m_Onid;
	TCHAR						m_ChannelName[256];
	Encoder* const				m_pEncoder;
	time_t						m_Time;
	HANDLE						m_StartRecordingThread;
	HANDLE						m_StopRecordingThread;
	DWORD						m_StartRecordingThreadId;
	DWORD						m_StopRecordingThreadId;
	bool						m_StartRecordingThreadCanEnd;
	bool						m_StopRecordingThreadCanEnd;
	bool						m_IsBrokenPipe;
	bool						m_UseSid;
	const wstring				m_FileName;
	const __int64				m_Size;
	bool						m_ForEIT;

public:
	Recorder(PluginsHandler* const plugins,
			 GenericSource* const source,
			 LPCWSTR outFileName,
			 bool useSid,
			 int channel,
			 USHORT sid,
			 USHORT onid,
			 LPCTSTR channelName,
			 __int64 duration,
			 Encoder* pEncoder,
			 __int64 size,
			 bool bySage,
			 bool forEIT);
	virtual ~Recorder(void);
	bool isRecording() const							{ return m_IsRecording; }
	bool hasError() const								{ return m_HasError; }
	void startRecording();
	void stopRecording();
	bool changeState();
	GenericSource* getSource()							{ return m_pSource; }
	ESCAParser* getParser()								{ return m_pParser; }
	int getChannel() const								{ return m_ChannelNumber; }
	bool brokenPipe() const								{ return m_IsBrokenPipe; }
	void setBrokenPipe()								{ m_IsBrokenPipe = true; }
	__int64 getFileLength() const;
	const wstring& getFileName() const					{ return m_FileName; }
	USHORT getOnid() const								{ return m_Onid; }
};
