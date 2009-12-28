// epgfromdvb.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <share.h>
#include "graph.h"
#include "DVBFilter.h"

VOID NTAPI CloseTimerCallback(PVOID vpApp, BOOL alwaysTrue);

static HANDLE	ghCloseTimer				= NULL;
static DWORD	gLastWriteTickCounter		= 0;
static DWORD	gMainThreadId				= 0;
static int		giTimeout					= 20;
static FILE*	gpOutFile					= NULL;

// This is the timer callback
VOID NTAPI CloseTimerCallback(PVOID vpDVBFilter,
							  BOOL alwaysTrue)
{
	// First, cast the pointer to the right class
	DVBFilterToParserProxy* pDVBFilter = (DVBFilterToParserProxy*)vpDVBFilter;
	if(pDVBFilter)
	{
		// Get the tick counter of the last change
		DWORD tickCounter = pDVBFilter->GetLastWriteTickCount();
		// If not different from the previous one
		if(gLastWriteTickCounter != 0 && gLastWriteTickCounter == tickCounter)
		{
			// Write the closing tag to the output
			fputs("</tv>\r\n", gpOutFile);
			// And make the main thread quit!
			PostThreadMessage(gMainThreadId, WM_QUIT, 0, 0);
		}
		else
			// Update the last tick counter
			gLastWriteTickCounter = tickCounter;
	}
}


// Parsing the config file
void ParseConfigFile(const char* iniFileName,
					 CBDAFilterGraph& oBDAFilterGraph,
					 DVBFilterToParserProxy& oDVBFilterToParserProxy)
{
	// Check for "IterativeTimeout" config parameter
	giTimeout = GetPrivateProfileInt("Tuner", "Timeout", 20, iniFileName);

	// Check for "TunerOrdianl" config parameter
	oBDAFilterGraph.SetTunerOrdinalNumber(GetPrivateProfileInt("Tuner", "TunerOrdinal", 1, iniFileName));

	// Check for "Transponder" config parameter
	oBDAFilterGraph.SetTransponder(GetPrivateProfileInt("Tuner", "Transponder", 10806000, iniFileName));
}

// This function prepares the filter graph
BOOL prepareFilterGraph(CBDAFilterGraph& oBDAFilterGraph,
						DVBFilterToParserProxy& oDVBFilterToParserProxy)
{
	// Create the DVB filter
	oBDAFilterGraph.SetDVBFilterToParserProxy(&oDVBFilterToParserProxy);

	if(FAILED(oBDAFilterGraph.BuildGraph(L"EPG Grabber")))
	{
		printf("Err: Could not Build the DVB-S BDA FilterGraph.\n");
		return FALSE;
	}

	// Tune to a transponder
	//oBDAFilterGraph.m_ulCarrierFrequency = 11510000;
	oBDAFilterGraph.m_ulSymbolRate = 27500;
	oBDAFilterGraph.m_SignalPolarisation = 0;
	oBDAFilterGraph.ChangeSetting();

	// Get lock status
	BOOLEAN bLocked = false;
	LONG lSignalQuality = 0;		
	LONG lSignalStrength = 0;

	oBDAFilterGraph.GetTunerStatus(&bLocked, &lSignalQuality, &lSignalStrength);
	if(bLocked)
		printf("Signal locked, quality=%d, strength=%d\n", lSignalQuality, lSignalStrength);
	else
		printf("Signal not locked!\n");

	// Set filters (this is specific for StarBox)
	oBDAFilterGraph.SetPidInfo();

	// Run the graph
	if(FAILED(oBDAFilterGraph.RunGraph()))
	{
		printf("Err: Could not play the FilterGraph.\n");
		return FALSE;
	}

	if(!CreateTimerQueueTimer(&ghCloseTimer,
		NULL,
		(WAITORTIMERCALLBACK)CloseTimerCallback,
		(PVOID)&oDVBFilterToParserProxy,
		0,
		1000 * giTimeout,
		WT_EXECUTEINTIMERTHREAD) || !ghCloseTimer)
	{
		printf("Failed to create close timer.\n");
		return FALSE;
	}

	return TRUE;
}

void printUsage(const char* name)
{
	// Print usage message
	fprintf(stderr, "Usage: %s [-s] <output_file_name>\n", name);
}

// Main entry point
int main(int argc,
		 char* argv[])
{
	// Check the parameters first
	if(argc < 2 || argc > 3)
	{
		printUsage(argv[0]);
		return -1;
	}

	int fnamePosition = 1;
	if(argc == 3)
		if(strcmp(argv[1], "-s") == 0)
		{
			fnamePosition = 2;
			HWND hwnd = GetConsoleWindow();
			if(hwnd)
			{
				DWORD pid = 0;
				GetWindowThreadProcessId(hwnd, &pid);
				if(pid == GetCurrentProcessId())
					ShowWindow(hwnd, SW_HIDE);
			}
		}
		else
		{
			printUsage(argv[0]);
			return -1;
		}

	// Open the output file
	gpOutFile = _fsopen(argv[fnamePosition], "wb", _SH_DENYWR);

#ifdef _DEBUG
	setvbuf(gpOutFile, NULL, _IONBF, 0);
#endif

	// Check for error
	if(!gpOutFile)
	{
		fprintf(stderr, "Can't open file %s\n", argv[1]);
		return 1;
	}

	// Put the XML header into the output file
	fprintf(gpOutFile, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n");
	fprintf(gpOutFile, "<!DOCTYPE tv SYSTEM \"xmltv.dtd\">\r\n");
	fprintf(gpOutFile, "<tv source-info-name=\"DVB Broadcast by YES Israel\" generator-info-name=\"DVB EPG Generator for YES Israel\">\r\n");

	// Get the main thread handle
	gMainThreadId = GetCurrentThreadId();

	// Initialize COM stuff
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	// Create the graph object
	CBDAFilterGraph*	pBDAFilterGraph = new CBDAFilterGraph;
	// Create the parser object
	DVBFilterToParserProxy*	pDVBFilter = new DVBFilterToParserProxy(gpOutFile);

	ParseConfigFile(".\\epgfromdvb.ini", *pBDAFilterGraph, *pDVBFilter);

	// Prepare the filter graph and parse channel info first
	if(prepareFilterGraph(*pBDAFilterGraph, *pDVBFilter))
	{
		MSG msg;
		BOOL bRet;

		try
		{
			// Run the message loop
			while( (bRet = GetMessage( &msg, NULL, 0, 0 )) != 0)
			{ 
				if (bRet == -1)
				{
					break;
				}
				else
				{
					TranslateMessage(&msg); 
					DispatchMessage(&msg);
					if(msg.message == WM_QUIT)
						break;
				}
			}
		}
		catch(char* string)
		{
			fprintf(stderr, "Exception: %s. Exitting...\n", string);
			exit(-2);
		}
	}

	// Delete the close timer
	if(ghCloseTimer)
		DeleteTimerQueueTimer(NULL, ghCloseTimer, NULL);

	// Delete the graph object
	delete pBDAFilterGraph;

	// Uninitialize COM
	CoUninitialize();

	// Close the output file
	fclose(gpOutFile);

	return 0;
}
