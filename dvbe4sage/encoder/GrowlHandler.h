#pragma once

#include "growl++.hpp"

using namespace std;
using namespace stdext;

// Note: This needs to match static const char *notifications[]
enum NotificationType {NOTIFICATION_ERROR, NOTIFICATION_NEWCHANNEL, NOTIFICATION_NEWSID, NOTIFICATION_ONTUNE };

class GrowlHandler
{
public:
	GrowlHandler(void);
	~GrowlHandler(void);

	// Send a formatted message to the configured Growl listener
	void SendNotificationMessage(NotificationType nType, string title, LPCTSTR format, ...);

private:
	Growl *m_pGrowl;
};

extern GrowlHandler* g_pGrowlHandler;
