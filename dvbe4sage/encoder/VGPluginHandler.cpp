#include "stdafx.h"

#include "resource.h"
#include "VGPluginHandler.h"
#include "dvbparser.h"
#include "logger.h"
#include "configuration.h"

#define EMM_TIMEOUT 100
#define ECM_TIMEOUT 100
#define STT_TIMEOUT 100

// This is the constructor of a class that has been exported.
// see VGPluginsHandler.h for the class definition
VGPluginsHandler::VGPluginsHandler() : 
	m_GetCardStatus(NULL),
	m_GetECM(NULL),
	m_DoEMM(NULL)
{
	// Get a handle to the DLL module.
	LoadVGHandler(m_hinstLib, m_GetCardStatus, m_GetECM, m_DoEMM);
	if (m_hinstLib == NULL)
		log(0, true, "VGPluginsHandler() Could not load VCHandler.DLL (Error: %u)", GetLastError());

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
		log(0, true, "Timed out while posting ECM to VGHandler");
}

void VGPluginsHandler::processEMMPacket(BYTE *packet)
{
	if (!m_DoEMM(packet + 1, EMM_TIMEOUT))
		log(0, true, "Timed out while posting EMM to VGService");
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

	bool isOddKey = nParm != 0;

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

		// And set the key to the parser which called us
		if(wrong(dcw) || !m_pCurrentClient->caller->setKey(isOddKey, dcw.key))
		{
			// Log the key
			log(2, true, TEXT("Received %s DCW = %.02hX%.02hX%.02hX%.02hX%.02hX%.02hX%.02hX%.02hX - wrong key, discarded!\n"), isOddKey ? TEXT("ODD") : TEXT("EVEN"),
				(USHORT)dcw.key[0], (USHORT)dcw.key[1], (USHORT)dcw.key[2], (USHORT)dcw.key[3], (USHORT)dcw.key[4], (USHORT)dcw.key[5], (USHORT)dcw.key[6], (USHORT)dcw.key[7]);
			return;
		}
		else
			// Log the key
			log(2, true, TEXT("Received %s DCW = %.02hX%.02hX%.02hX%.02hX%.02hX%.02hX%.02hX%.02hX - accepted!\n"), isOddKey ? TEXT("ODD") : TEXT("EVEN"),
			(USHORT)dcw.key[0], (USHORT)dcw.key[1], (USHORT)dcw.key[2], (USHORT)dcw.key[3], (USHORT)dcw.key[4], (USHORT)dcw.key[5], (USHORT)dcw.key[6], (USHORT)dcw.key[7]);

		log(2, true, TEXT("Response for SID=%hu received, passing to the parser...\n"), m_CurrentSid);

		ECMRequestComplete();
	}
	else
		log(0, true, TEXT("Key received but the client has already left\n"));
}
