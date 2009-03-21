#include "StdAfx.h"

#include "VirtualTuner.h"
#include "recorder.h"

VirtualTuner::VirtualTuner(USHORT port,
						   HWND hWnd) :
	m_pRecorder(NULL)
{
	// Create the server socket
	m_ServerSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0);
	
	// Make it listen on port 6969
	sockaddr_in address;
	memset(&address.sin_addr, 0, sizeof(address.sin_addr));
	address.sin_port = htons(port);
	address.sin_family = AF_INET;
	memset(address.sin_zero, 0, sizeof(address.sin_zero));
	BOOL rc = bind(m_ServerSocket, (sockaddr*)&address, sizeof(address));

	// Listen on this port
	rc = listen(m_ServerSocket, 10);

	// And do async select on it
	rc = WSAAsyncSelect(m_ServerSocket, hWnd, WM_ACCEPT_CONNECTION, FD_ACCEPT);
}

VirtualTuner::~VirtualTuner(void)
{
	// Close the server socket
	closesocket(m_ServerSocket);
}

void VirtualTuner::setRecorder(Recorder* recorder)
{
	m_pRecorder = recorder;
	if(recorder != NULL)
		recorder->setVirtualTuner(this);
}
