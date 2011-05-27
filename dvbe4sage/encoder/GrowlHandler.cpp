#include "StdAfx.h"
#include "GrowlHandler.h"
#include "Logger.h"
#include "configuration.h"


GrowlHandler* g_pGrowlHandler = NULL;

// Note: This needs to match the NotificationType enumeration
static const char *notifications[] = { 	"Error",
										"NewChannel",
										"NewSID",
										"OnTune"};

GrowlHandler::GrowlHandler(void)
{
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

GrowlHandler::~GrowlHandler(void)
{
	if(m_pGrowl != NULL)
		delete(m_pGrowl);
}

void GrowlHandler::SendNotificationMessage(NotificationType nType, string title, LPCTSTR format, ...)
{	
	TCHAR m_Message[200];
	try
	{
		va_list argList;
		va_start(argList, format);

		vsprintf(m_Message, format, argList);

		if(m_pGrowl != NULL)
			m_pGrowl->Notify(notifications[nType], title.c_str(), m_Message);
	}
	catch(...) {}
}

