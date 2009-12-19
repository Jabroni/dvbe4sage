#include "stdafx.h"

#include "resource.h"
#include "VGPluginHandler.h"
#include "dvbparser.h"
#include "logger.h"
#include "configuration.h"

#define EMM_TIMEOUT 0
#define ECM_TIMEOUT 0
//#define STT_TIMEOUT 100

// This is the constructor of a class that has been exported.
// see VGPluginsHandler.h for the class definition
VGPluginsHandler::VGPluginsHandler() : 
	m_GetCardStatus(NULL),
	m_GetECM(NULL),
	m_DoEMM(NULL)
{
	// Log the entry
	log(0, true, 0, TEXT("Working with VGHandler\n"));

	// Get a handle to the DLL module.
	LoadVGHandler(m_hinstLib, m_GetCardStatus, m_GetECM, m_DoEMM);
	if (m_hinstLib == NULL)
		log(0, true, 0, TEXT("VGPluginsHandler() Could not load vchandler.dll (error: %u)\n"), GetLastError());

	return;
}


// Cleanup plugins
VGPluginsHandler::~VGPluginsHandler()
{
	EndWorkerThread();

	// Do this in the critical section, just in case some threads still do something with this object
	CAutoLock lock(&m_cs);

	if (m_hinstLib)
		UnloadVGHandler(m_hinstLib); 

}

void VGPluginsHandler::processECMPacket(BYTE *packet)
{
	if (!m_GetECM(this, packet[1]&0x1, packet + 1, &DCWHandlerCallback, ECM_TIMEOUT))
		log(0, true, 0, "Timed out while posting ECM to VGHandler");
}

void VGPluginsHandler::processEMMPacket(BYTE *packet)
{
	if (!m_DoEMM(packet + 1, EMM_TIMEOUT))
		log(3, true, 0, "Timed out while posting EMM to VGService");
}

void WINAPI VGPluginsHandler::DCWHandlerCallback(void *pParm, int nParm, unsigned char pDCW[16])
{
	((VGPluginsHandler*)pParm)->DCWHandler(nParm, pDCW);
}

void WINAPI VGPluginsHandler::DCWHandler(int nParm,
										 unsigned char pDCW[16])
{

	// Do this in critical section
	CAutoLock lock(&m_cs);

	bool isOddKey = (nParm != 0);

	// Let's check the state of the current client
	if(m_pCurrentClient != NULL)
	{
		unsigned char *p = (unsigned char *)&pDCW[0] + (isOddKey ? 8 : 0);

		// Here we put the key
		Dcw dcw;

		// If OK, copy the key from the DVB command buffer
		memcpy(dcw.key, p, 8);

		//Fix Checksum
		dcw.key[3] = dcw.key[0] + dcw.key[1] + dcw.key[2];
		dcw.key[7] = dcw.key[4] + dcw.key[5] + dcw.key[6];

		// Indicate that ECM request has been completed and add it to the cache
		ECMRequestComplete(m_pCurrentClient, m_pCurrentClient->ecmPacket, dcw, isOddKey, true);
	}
	else
		log(0, true, 0, TEXT("Key received but the client has already left\n"));
}
