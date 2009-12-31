#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <share.h>
#include <tchar.h>
#include <conio.h>
#include "../encoder/extern.h"

#define SERVICE_NAME		TEXT("DVB Enhancer For SageTV")

#define WINDOW_CLASS_NAME	TEXT("MyHiddenClass")
#define WINDOW_NAME			TEXT("MyWindow")

#define INSTALL_FLAG		TEXT("/install")
#define UNINSTALL_FLAG		TEXT("/uninstall")
#define USERNAME_PREFIX		TEXT("/user:")
#define PASSWORD_PREFIX		TEXT("/password:")
#define SERVICE_NAME_PREFIX	TEXT("/servicename:")

#define STRING_LENGTH(str)	((sizeof(str) - 1)/ sizeof(TCHAR))

// Static variables
SERVICE_STATUS ServiceStatus;
SERVICE_STATUS_HANDLE hStatus;
HWND hWnd;
int globalArgc = 1;
TCHAR** globalArgv = NULL;

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
	//OutputDebugString(TEXT("Got message\n"));
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

void ServiceMain(int argc,
				 TCHAR** argv) 
{
	if(globalArgc == 1)
	{
		// Legacy mode - set current directory to where the modules are located
		TCHAR path[MAXSHORT];
		GetModuleFileName(NULL, path, STRING_LENGTH(path));
		LPTSTR end = _tcsrchr(path, TCHAR('\\'));
		end[0] = TCHAR('\0');
		SetCurrentDirectory(path);
	}
	else
		SetCurrentDirectory(globalArgv[1]);

	ServiceStatus.dwServiceType        = SERVICE_WIN32_OWN_PROCESS; 
	ServiceStatus.dwCurrentState       = SERVICE_START_PENDING; 
	ServiceStatus.dwControlsAccepted   = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	ServiceStatus.dwWin32ExitCode      = NO_ERROR; 
	ServiceStatus.dwServiceSpecificExitCode = 0; 
	ServiceStatus.dwCheckPoint         = 0; 
	ServiceStatus.dwWaitHint           = 60000; 
 
	hStatus = RegisterServiceCtrlHandler(TEXT(""), (LPHANDLER_FUNCTION)ControlHandler);
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
	myWndClass.lpszClassName = WINDOW_CLASS_NAME;
	myWndClass.hInstance = GetModuleHandle(NULL);
	ATOM myClass = RegisterClass(&myWndClass);	
	hWnd = CreateWindow(WINDOW_CLASS_NAME, WINDOW_NAME, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, GetModuleHandle(NULL), NULL);

	// Initialize the encoder
	createEncoder(GetModuleHandle(NULL), hWnd, NULL);
	waitForFullInitialization();

	// Now, we can report the running status to SCM. 
	ServiceStatus.dwCurrentState = SERVICE_RUNNING; 
	SetServiceStatus (hStatus, &ServiceStatus);

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

			// We can quit if the message is to quit (received when the service is told to stop)
			if(msg.message == WM_QUIT)
				break;
		}
	}

	// Now, delete the encoder
	deleteEncoder();

	// Now we can notify the service manager that the service is stopped
	ServiceStatus.dwWin32ExitCode = 0; 
	ServiceStatus.dwCurrentState  = SERVICE_STOPPED;
	SetServiceStatus(hStatus, &ServiceStatus);
}

void PrintUsageInfo(LPCTSTR executableFilePath)
{
	_ftprintf(stderr, TEXT("Usage:\n\"%s\" /install [/servicename:service_name] [/user:domain\\username] [/password:password] - install the service\n\"%s\" /uninstall [/servicename:service_name] - uninstall the service\n"),
		executableFilePath, executableFilePath);
}

void getpass(LPTSTR getpassbuff)
{
	size_t i = 0;
	for(;;)
    {
		TCHAR c = _gettch();
		if(c == TCHAR('\r'))
		{
			getpassbuff[i] = TCHAR('\0');
			_puttc(TCHAR('\n'), stdout);
			break;
		}
		else
			getpassbuff[i++] = c;
    }
}

int main(int argc,
		 TCHAR** argv)
{
	int retCode = 0;

	// Copy arguments to the global variable;
	globalArgc = argc;
	globalArgv = argv;

	// Try service start first
	SERVICE_TABLE_ENTRY ServiceTable[] = { { TEXT(""), (LPSERVICE_MAIN_FUNCTION)ServiceMain } , { NULL, NULL } };
	if(!StartServiceCtrlDispatcher(ServiceTable))
	{
		// Now we try install/uninstall procedure
		// We assume no username or password have been specified so far
		LPCTSTR username = NULL;
		LPCTSTR password = NULL;
		// And the service name equals to the default
		LPCTSTR servicename = SERVICE_NAME;

		// Different modes and states
		enum MODE { MODE_NONE, MODE_INSTALL, MODE_UNINSTALL, MODE_ERROR } mode = MODE_NONE;

		// Loop through the arguments
		for(int i = 1; i < argc; i++)
			if(mode == MODE_NONE && !_tcsicmp(argv[i], INSTALL_FLAG))
				mode = MODE_INSTALL;
			else if(mode == MODE_NONE && !_tcsicmp(argv[i], UNINSTALL_FLAG))
				mode = MODE_UNINSTALL;
			else if((mode == MODE_NONE || mode == MODE_INSTALL) && !_tcsncicmp(argv[i], USERNAME_PREFIX, STRING_LENGTH(USERNAME_PREFIX)))
				// Let's get the username from here
				username = argv[i] + STRING_LENGTH(USERNAME_PREFIX);
			else if((mode == MODE_NONE || mode == MODE_INSTALL) && !_tcsncicmp(argv[i], PASSWORD_PREFIX, STRING_LENGTH(PASSWORD_PREFIX)))
				// Let's get the password from here
				password = argv[i] + STRING_LENGTH(PASSWORD_PREFIX);
			else if((mode == MODE_NONE || mode == MODE_INSTALL || mode == MODE_UNINSTALL) && !_tcsncicmp(argv[i], SERVICE_NAME_PREFIX, STRING_LENGTH(SERVICE_NAME_PREFIX)))
				// Let's get the password from here
				servicename = argv[i] + STRING_LENGTH(SERVICE_NAME_PREFIX);
			else
			{
				mode = MODE_ERROR;
				break;
			}
		
		switch(mode)
		{
			case MODE_NONE:
			case MODE_ERROR:
				// Print usage information
				PrintUsageInfo(argv[0]);
				return -1;
			case MODE_INSTALL:
			{
				// Install the service here

				TCHAR pwdBuffer[100];

				// Let's ask password if needed but not supplied
				if(username != NULL && password == NULL)
				{
					// Let's get the password from the console
					_tprintf(TEXT("Please, specify the password for the user \"%s\":"), username);
					getpass(pwdBuffer);
					password = pwdBuffer;
				}

				// Open the service manager
				SC_HANDLE managerHandle = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);

				// Check whether we successfully opened the service manager
				if(managerHandle == NULL)
				{
					_ftprintf(stderr, TEXT("Could not open the service manager, error code=%d, are you running this program as an administrator?\n"), GetLastError());
					return -1;
				}

				// Get the current executable full path
				TCHAR binrayFullPath[MAXSHORT];
				GetModuleFileName(NULL, binrayFullPath, STRING_LENGTH(binrayFullPath));
				LPCTSTR pathDelimiterString = _tcschr(binrayFullPath, TCHAR(' ')) != NULL ? TEXT("\"") : TEXT("");

				// Get the current directory
				TCHAR currentDir[MAXSHORT];
				GetCurrentDirectory(STRING_LENGTH(currentDir), currentDir);
				LPCTSTR argDelimiterString = _tcschr(currentDir, TCHAR(' ')) != NULL ? TEXT("\"") : TEXT("");

				// Make service command line
				TCHAR commandLine[MAXSHORT * 2];
				_stprintf_s(commandLine, STRING_LENGTH(commandLine), TEXT("%s%s%s %s%s%s"), 
					pathDelimiterString, binrayFullPath, pathDelimiterString,
					argDelimiterString, currentDir, argDelimiterString);

				// Now, try to create the service
				SC_HANDLE serviceHandle = CreateService(managerHandle, servicename, servicename, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
														SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, commandLine, NULL, NULL, NULL, username, password);

				// Check whether we successfully created our service
				if(serviceHandle == NULL)
				{
					if(username == NULL)
						_ftprintf(stderr, TEXT("Could not create a service \"%s\" as \"LocalSystem\", error code=%d\n"), servicename, GetLastError());
					else
						_ftprintf(stderr, TEXT("Could not create a service \"%s\" using username=\"%s\" and password=\"%s\", error code=%d, please, check the specified credentials!\n"),
												servicename, username, password, GetLastError());
					CloseServiceHandle(managerHandle);
					return -1;
				}

				SERVICE_DESCRIPTION serviceDescription;
				serviceDescription.lpDescription = TEXT("This launches DVB Enhancer for SageTV application in the service mode, so it can be used in a completely unattended manner.");
				ChangeServiceConfig2(serviceHandle, SERVICE_CONFIG_DESCRIPTION, (LPVOID)&serviceDescription);

				// Print success message
				_tprintf(TEXT("Service successfully installed!\n"));

				// Now, change the dependency of SageTV service
				SC_HANDLE sageHandle = OpenService(managerHandle, TEXT("SageTV"), SERVICE_ALL_ACCESS);
				if(sageHandle != NULL)
					if(!ChangeServiceConfig(sageHandle, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, NULL, NULL, NULL, servicename, NULL, NULL, NULL))
						_ftprintf(stderr, TEXT("Could not change dependency of the SageTV service, error code=%d\n"), GetLastError());
					else
						_tprintf(TEXT("Successfully changed the SageTV service dependency\n"));

				// All is OK, just close the handles
				CloseServiceHandle(serviceHandle);
				if(sageHandle != NULL)
					CloseServiceHandle(sageHandle);
				CloseServiceHandle(managerHandle);
				break;
			}
			case MODE_UNINSTALL:
			{
				// Uninstall the service here
		
				// Open the service manager
				SC_HANDLE managerHandle = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

				// Check whether we successfully opened the service manager
				if(managerHandle == NULL)
				{
					_ftprintf(stderr, TEXT("Could not open the service manager, error code=%d, are you running this program as an administrator?\n"), GetLastError());
					return -1;
				}

				// Open the service
				SC_HANDLE serviceHandle = OpenService(managerHandle, servicename, SERVICE_ALL_ACCESS);

				// Check whether we successfully opened our service
				if(serviceHandle == NULL)
				{
					_ftprintf(stderr, TEXT("Could not open the service \"%s\", error code=%d, it might have already been deleted\n"), servicename, GetLastError());
					CloseServiceHandle(managerHandle);
					return -1;
				}

				// Now delete the service
				if(!DeleteService(serviceHandle))
				{
					_ftprintf(stderr, TEXT("Could not delete the service \"%s\", error code=%d, might be a permission problem\n"), GetLastError());
					retCode = -1;
				}
				else
					// Print success message
					_tprintf(TEXT("Service successfully uninstalled!\n"));

				// All is OK, just close the handles
				CloseServiceHandle(serviceHandle);
				CloseServiceHandle(managerHandle);
				break;
			}
			default:
				break;
		}
	}

	return retCode;
}
