#pragma once

#include "dvbparser.h"

class Tuner;
class PluginsHandler;
class Encoder;
class VirtualTuner;

class Recorder
{
	friend VOID NTAPI StartRecordingCallback(PVOID vpRecorder, BOOLEAN alwaysTrue);
	friend VOID NTAPI StopRecordingCallback(PVOID vpRecorder, BOOLEAN alwaysTrue);
private:
	// Disallow default and copy constructors
	Recorder();
	Recorder(Recorder&);

	// Data members
	PluginsHandler* const		m_pPluginsHandler;
	ESCAParser*					m_pParser;
	FILE*						m_fout;
	bool						m_IsRecording;
	bool						m_HasError;
	const int					m_ChannelNumber;
	const __int64				m_Duration;
	Tuner* const				m_pTuner;
	USHORT						m_Sid;
	Encoder* const				m_pEncoder;
	time_t						m_Time;
	HANDLE						m_TimerQueue;
	bool						m_IsBrokenPipe;
	bool						m_UseSid;
	const USHORT				m_LogicalTuner;
	const string				m_FileName;
	const __int64				m_Size;
	VirtualTuner*				m_VirtualTuner;

	// Internal states
	enum states { INITIAL, TUNING, TUNED };
	states						m_CurrentState;

	// Tuning timer
	HANDLE						m_RecordingTimer;
public:
	Recorder(PluginsHandler* const plugins, Tuner* const tuner, USHORT logicalTuner, LPCTSTR outFileName, bool useSid, int channel, __int64 duration, Encoder* pEncoder, __int64 size);
	virtual ~Recorder(void);
	bool isRecording() const							{ return m_IsRecording; }
	bool hasError() const								{ return m_HasError; }
	bool startRecording();
	void stopRecording();
	bool changeState();
	Tuner* getTuner()									{ return m_pTuner; }
	ESCAParser* getParser()								{ return m_pParser; }
	int getChannel() const								{ return m_ChannelNumber; }
	bool brokenPipe() const								{ return m_IsBrokenPipe; }
	void setBrokenPipe()								{ m_IsBrokenPipe = true; }
	USHORT getLogicaTuner() const						{ return m_LogicalTuner; }
	__int64 getFileLength() const						{ return m_pParser != NULL ? m_pParser->getFileLength() : (__int64)0; }
	const string& getFileName() const					{ return m_FileName; }
	void setVirtualTuner(VirtualTuner* virtualTuner)	{ m_VirtualTuner = virtualTuner; }
};
