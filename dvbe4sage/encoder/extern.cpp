#include "stdafx.h"

#include "encoder.h"
#include "extern.h"

// This it the global encoder object
Encoder* g_pEncoder = NULL;

extern "C" ENCODER_API void createEncoder(HINSTANCE hInstance,
										  HWND hWnd,
										  HMENU hParentMenu)
{
	g_pEncoder = new Encoder(hInstance, hWnd, hParentMenu);
}

extern "C" ENCODER_API void deleteEncoder()
{
	delete g_pEncoder;
}

extern "C" ENCODER_API LRESULT WindowProc(UINT message,
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
