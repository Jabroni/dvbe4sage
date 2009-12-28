#include "stdafx.h"

#include "encoder.h"
#include "logger.h"
#include "configuration.h"
#include "extern.h"

// This it the global encoder object
Encoder*	g_pEncoder = NULL;

extern "C" ENCODER_API void createEncoder(HINSTANCE hInstance,
										  HWND hWnd,
										  HMENU hParentMenu)
{
	g_pLogger = new Logger;
	g_pConfiguration = new Configuration;
	g_pEncoder = new Encoder(hInstance, hWnd, hParentMenu);
}

extern "C" ENCODER_API void deleteEncoder()
{
	delete g_pEncoder;
	delete g_pLogger;
}

extern "C" ENCODER_API LRESULT encoderWindowProc(UINT message,
												 WPARAM wParam,
												 LPARAM lParam)
{
	if(g_pEncoder != NULL)
		return g_pEncoder->WindowProc(message, wParam, lParam);
	else
		return 0;
}

extern "C" ENCODER_API void waitForFullInitialization()
{
	if(g_pEncoder != NULL)
		g_pEncoder->waitForFullInitialization();
}

extern "C" ENCODER_API LPCTSTR getLogFileName()
{
	if(g_pLogger != NULL)
		return g_pLogger->getLogFileName();
	else
		return NULL;
}

extern "C" ENCODER_API int getNumberOfTuners()
{
	if(g_pEncoder != NULL)
		return g_pEncoder->getNumberOfTuners();
	else
		return 0;
}

extern "C" ENCODER_API LPCTSTR getTunerFriendlyName(int i)
{
	if(g_pEncoder != NULL)
		return g_pEncoder->getTunerFriendlyName(i);
	else
		return NULL;
}

extern "C" ENCODER_API int getTunerOrdinal(int i)
{
	if(g_pEncoder != NULL)
		return g_pEncoder->getTunerOrdinal(i);
	else
		return -1;
}

extern "C" ENCODER_API bool startRecording(bool autodiscoverTransponder,
										   ULONG frequency,
										   ULONG symbolRate,
										   Polarisation polarization,
										   ModulationType modulation,
										   BinaryConvolutionCodeRate fecRate,
										   int tunerOrdinal,
										   int channel,
										   bool useSid,
										   __int64 duration,
										   LPCWSTR outFileName,
										   __int64 size,
										   bool startFullTransponderDump)
{
	return g_pEncoder != NULL ? g_pEncoder->startRecording(autodiscoverTransponder, frequency, symbolRate, polarization, modulation, fecRate,
		tunerOrdinal, false, channel, useSid, duration, outFileName, NULL, size, false, startFullTransponderDump) : false;
}

extern "C" ENCODER_API bool startRecordingFromFile(LPCWSTR inFileName,
												   int sid,
												   __int64 duration,
												   LPCWSTR outFileName)
{
	return g_pEncoder != NULL ? g_pEncoder->startRecordingFromFile(inFileName, sid, duration, outFileName) : false;
}

extern "C" ENCODER_API bool dumpECMCache(LPCTSTR fileName,
										 LPTSTR reason)
{
	return g_pEncoder != NULL ? g_pEncoder->dumpECMCache(fileName, reason) : false;
}

extern "C" ENCODER_API bool loadECMCache(LPCTSTR fileName,
										 LPTSTR reason)
{
	return g_pEncoder != NULL ? g_pEncoder->loadECMCache(fileName, reason) : false;
}
