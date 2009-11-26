#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <share.h>
#include <tchar.h>
#include "../encoder/extern.h"

#define SERVICE_NAME	TEXT("DVB Enhancer For SageTV")
// Static variables
SERVICE_STATUS ServiceStatus;
SERVICE_STATUS_HANDLE hStatus;
HWND hWnd;

#define WM_END_OF_INITIALIZATION	WM_USER + 25

DWORD WINAPI StartupThread(LPVOID lpParameter)
{
	HWND hWnd = (HWND)lpParameter;
	createEncoder(GetModuleHandle(NULL), hWnd, NULL);
	waitForFullInitialization();
	PostMessage(hWnd, WM_END_OF_INITIALIZATION, 0, 0);
	return 0;
}

// Control handler function
void ControlHandler(DWORD request) 
{ 
    switch(request) 
    { 
        case SERVICE_CONTROL_STOP: 
		case SERVICE_CONTROL_SHUTDOWN: 
            PostMessage(hWnd, WM_QUIT, 0, 0);
            break;        
        default:
            break;
    }  
    return; 
} 

LRESULT CALLBACK myWndProc(HWND hwnd,
						   UINT uMsg,
						   WPARAM wParam,
						   LPARAM lParam)
{
	OutputDebugString(TEXT("Got message\n"));
	switch(uMsg)
	{
		case WM_CREATE:
			return 0;
		case WM_NCCREATE:
			return TRUE;
		default:
			return encoderWindowProc(uMsg, wParam, lParam);
	}
}

void ServiceMain(int argc, char** argv) 
{
	// Set current directory to where the modules are located
	TCHAR path[MAXSHORT];
	GetModuleFileName(NULL, path, sizeof(path) / sizeof(path[0]));
	LPTSTR end = _tcsrchr(path, TCHAR('\\'));
	end[0] = TCHAR('\0');
	SetCurrentDirectory(path);
	//SetCurrentDirectory("C:\\Program Files (x86)\\Michael Pogrebisky\\DVB Enhancer for SageTV");

	ServiceStatus.dwServiceType        = SERVICE_WIN32_OWN_PROCESS; 
	ServiceStatus.dwCurrentState       = SERVICE_START_PENDING; 
	ServiceStatus.dwControlsAccepted   = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	ServiceStatus.dwWin32ExitCode      = NO_ERROR; 
	ServiceStatus.dwServiceSpecificExitCode = 0; 
	ServiceStatus.dwCheckPoint         = 0; 
	ServiceStatus.dwWaitHint           = 60000; 
 
	hStatus = RegisterServiceCtrlHandler(SERVICE_NAME, (LPHANDLER_FUNCTION)ControlHandler);
    if(hStatus == (SERVICE_STATUS_HANDLE)0) 
    { 
        // Registering Control Handler failed
        return; 
    }

	// Now we can report the service is pending initialization
	SetServiceStatus(hStatus, &ServiceStatus);

	// Create the window
	WNDCLASS myWndClass;
	ZeroMemory(&myWndClass, sizeof(myWndClass));
	myWndClass.lpfnWndProc = myWndProc;
	myWndClass.lpszClassName = TEXT("MyHiddenClass");
	myWndClass.hInstance = GetModuleHandle(NULL);
	ATOM myClass = RegisterClass(&myWndClass);
	
	hWnd = CreateWindow(TEXT("MyHiddenClass"), TEXT("MyWindow"), 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, GetModuleHandle(NULL), NULL);

	// Create the service worker thread
	HANDLE hStartupThread = CreateThread(NULL, 0, StartupThread, (LPVOID)hWnd, 0, NULL);

	//createEncoder(GetModuleHandle(NULL), hWnd, NULL);
	//waitForFullInitialization();

	// Now, we can report the running status to SCM. 
	//ServiceStatus.dwCurrentState = SERVICE_RUNNING; 
	//SetServiceStatus (hStatus, &ServiceStatus);

	// Now, execute the message loop
	MSG msg;
	BOOL bRet;
	while((bRet = GetMessage( &msg, NULL, 0, 0 )) != 0)
	{
		if (bRet == -1)
			break;
		else
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			// We can quit if the message is to quit
			if(msg.message == WM_QUIT)
				break;

			static bool checkStartupThread = true;
			static DWORD exitCode = 0;
			if(checkStartupThread && msg.message == WM_END_OF_INITIALIZATION)
			{
				checkStartupThread = false;

				// Now, we can report the running status to SCM. 
				ServiceStatus.dwCurrentState = SERVICE_RUNNING; 
				SetServiceStatus (hStatus, &ServiceStatus);
			}
		}
	}

	// Now, delete the encoder
	deleteEncoder();

	// Now we can notify the service manager that the service is stopped
	ServiceStatus.dwWin32ExitCode = 0; 
	ServiceStatus.dwCurrentState  = SERVICE_STOPPED;
	SetServiceStatus(hStatus, &ServiceStatus);
}

void main(int argc, TCHAR** argv)
{
	SERVICE_TABLE_ENTRY ServiceTable[] = { { SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain } , { NULL, NULL } };
	StartServiceCtrlDispatcher(ServiceTable);  
}
