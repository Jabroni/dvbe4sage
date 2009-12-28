#pragma once

#include "pluginshandler.h"
#include "VGHandler.h"

class VGPluginsHandler : public PluginsHandler
{
protected:

	static void WINAPI DCWHandlerCallback(void *pParm, int nParm, unsigned char pDCW[16]);

	void WINAPI DCWHandler(int nParm, unsigned char pDCW[16]);

	VGHandler_GetCardStatus_t m_GetCardStatus;
	VGHandler_GetECM_t m_GetECM;
	VGHandler_DoEMM_t m_DoEMM;

	HINSTANCE m_hinstLib;

	unsigned char m_cw[16];

protected:

	virtual void processECMPacket(BYTE *packet);
	virtual void processEMMPacket(BYTE *packet);
	virtual void processPMTPacket(BYTE *packet)	{}
	virtual void processCATPacket(BYTE *packet) {}
	virtual void resetProcessState() {}
	virtual bool canProcessECM()				{ return m_GetECM != NULL; }

public:
	// Constructor
	VGPluginsHandler();
	// Destructor
	virtual ~VGPluginsHandler();
};
