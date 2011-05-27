#include "StdAfx.h"
#include "GrowlHandler.h"
#include "Logger.h"
#include "configuration.h"

// Interface to send notifications to a congfigured Growl listener.
// You can find growl at http://growl.info/
// And growl for Windows at http://www.growlforwindows.com/gfw/

GrowlHandler* g_pGrowlHandler = NULL;

// Note: This needs to match the NotificationType enumeration
static const char *notifications[] = { 	"Error",
										"NewChannel",
										"NewSID",
										"OnTune",
										"RecordFailure"};

// Default constructor.  Connects to the Growl Listener.
GrowlHandler::GrowlHandler(void)
{
	// Initialize the clitical section
	InitializeCriticalSection(&m_cs);

	try
	{
		m_pGrowl = NULL;

		string ipAddr = g_pConfiguration->getGrowlIP();
		if(ipAddr.length() == 0)
				ipAddr = "127.0.0.1";
		string password = g_pConfiguration->getGrowlPassword();
		m_pGrowl = new Growl(GROWL_UDP, ipAddr.c_str(), password.c_str(), "DVBE4Sage", notifications, sizeof(notifications) / sizeof(notifications[0]));	
		log(0, true, 0, TEXT("Growl configured to send notifications to \"%s\"\n"), ipAddr.c_str());
					
	}
	catch(...) {}
}

// Default destructor
GrowlHandler::~GrowlHandler(void)
{
	if(m_pGrowl != NULL)
		delete(m_pGrowl);

	// Delete the critical section
	DeleteCriticalSection(&m_cs);
}

// Send a formatted message to the configured Growl listener
void GrowlHandler::SendNotificationMessage(NotificationType nType, string title, LPCTSTR format, ...)
{	
	TCHAR message[1024];

	// If no listener, don't bother
	if(m_pGrowl == NULL)
		return;

	// Do this in critical section
	EnterCriticalSection(&m_cs);

	try
	{
		va_list argList;
		va_start(argList, format);
		vsprintf_s(message, 1024, format, argList);
		
		m_pGrowl->Notify(notifications[nType], title.c_str(), message);
	}
	catch(...) {}

	// Leave the critical section
	LeaveCriticalSection(&m_cs);
}

