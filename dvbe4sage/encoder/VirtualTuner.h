#pragma once

class Recorder;

#define WM_ACCEPT_CONNECTION	(WM_USER + 1)
#define WM_SEND_RECEIVE_CLOSE	(WM_USER + 2)

class VirtualTuner
{
	// Private data
	SOCKET			m_ServerSocket;
	Recorder*		m_pRecorder;

	// No default and copy constructor
	VirtualTuner();
	VirtualTuner(const VirtualTuner&);
public:
	VirtualTuner(USHORT port, HWND hWnd);
	virtual ~VirtualTuner();

	void setRecorder(Recorder* recorder);
	Recorder* getRecorder() const				{ return m_pRecorder; }
	SOCKET getServerSocket() const				{ return m_ServerSocket; }
};
