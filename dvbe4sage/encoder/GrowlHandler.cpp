#include "StdAfx.h"
#include "GrowlHandler.h"
#include "Logger.h"
#include "configuration.h"


GrowlHandler* g_pGrowlHandler = NULL;

// Note: This needs to match the NotificationType enumeration
static const char *notifications[] = { 	"Error",
										"NewChannel"};

GrowlHandler::GrowlHandler(void)
{
	try
	{
		string ipAddr = g_pConfiguration->getGrowlIP();
		if(ipAddr.length() > 0)
		{
			m_pGrowl = new Growl(GROWL_TCP, ipAddr.c_str(), "DVBE4SAGE", notifications, sizeof(notifications) / sizeof(notifications[0]));	
			log(0, true, 0, TEXT("Growl configured to send notifications to \"%s\"\n"), ipAddr.c_str());
		}
	}
	catch(...) {}
}

GrowlHandler::~GrowlHandler(void)
{
	if(m_pGrowl != NULL)
		delete(m_pGrowl);
}

void GrowlHandler::SendNotificationMessage(NotificationType nType, string title, string message)
{	
	try
	{
		m_pGrowl->Notify(notifications[nType], title.c_str(), message.c_str());
	}
	catch(...) {}
}

