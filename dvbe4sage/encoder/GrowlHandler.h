#pragma once

#include "growl++.hpp"

using namespace std;
using namespace stdext;

enum NotificationType {NOTIFICATION_ERROR, NOTIFICATION_NEWCHANNEL };

class GrowlHandler
{
public:
	GrowlHandler(void);
	~GrowlHandler(void);

	void SendNotificationMessage(NotificationType nType, string title, string message);

private:
	Growl *m_pGrowl;
};

extern GrowlHandler* g_pGrowlHandler;
