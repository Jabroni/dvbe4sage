#pragma once

class Recorder;

#define WM_ACCEPT_CONNECTION	(WM_USER + 1)
#define WM_SEND_RECEIVE_CLOSE	(WM_USER + 2)

#define GET_FILE_SIZE_COUNTER_THRESHOLD		100

class VirtualTuner
{
	// Private data
	SOCKET			m_ServerSocket;
	Recorder*		m_pRecorder;

	// No default and copy constructor
	VirtualTuner();
	VirtualTuner(const VirtualTuner&);

	USHORT		m_GetFileSizeCounter;
public:
	VirtualTuner(USHORT port, HWND hWnd);
	virtual ~VirtualTuner();

	void setRecorder(Recorder* recorder);
	Recorder* getRecorder() const				{ return m_pRecorder; }
	SOCKET getServerSocket() const				{ return m_ServerSocket; }
	USHORT incrementGetFileSizeCounter()
	{
		if(m_GetFileSizeCounter == GET_FILE_SIZE_COUNTER_THRESHOLD)
			m_GetFileSizeCounter = 0;
		return m_GetFileSizeCounter++;
	}
};
