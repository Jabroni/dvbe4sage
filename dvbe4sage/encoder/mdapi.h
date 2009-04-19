#pragma once

#define MDAPI_GET_PROGRAMM           0x01020010
#define MDAPI_SET_PROGRAMM           0x01020011
#define MDAPI_RESCAN_PROGRAMM        0x01020012
#define MDAPI_SAVE_PROGRAMM          0x01020013
#define MDAPI_GET_PROGRAMM_NUMMER    0x01020014
#define MDAPI_SET_PROGRAMM_NUMMER    0x01020015

#define MDAPI_START_FILTER           0x01020020
#define MDAPI_STOP_FILTER            0x01020021

#define MDAPI_SCAN_CURRENT_TP        0x01020030
#define MDAPI_SCAN_CURRENT_CAT       0x01020031

#define MDAPI_START_OSD              0x01020040
#define MDAPI_OSD_DRAWBLOCK          0x01020041
#define MDAPI_OSD_SETFONT            0x01020042
#define MDAPI_OSD_TEXT               0x01020043
#define MDAPI_SEND_OSD_KEY           0x01020044
#define MDAPI_STOP_OSD               0x01020049

#define MDAPI_GET_VERSION            0x01020100

#define MDAPI_DVB_COMMAND            0x01020060

#define MDAPI_CHANNELCHANGE          0x01020090	

#define PH_MDAPI_MESSAGE	0x2001
#define PH_SEND_PID			0x2002
#define PH_PID_FILTER		0x2003

#define MAX_CA_SYSTEMS 32
#define MAX_PID_IDS  32

typedef struct _TDVB_COMMAND
{
	unsigned short length;
	BYTE buffer[32];
} TDVB_COMMAND;

typedef struct _TSTART_FILTER 
{
	unsigned short DLL_ID;
	unsigned short Filter_ID;
	unsigned short Pid;
	unsigned char Name[32];
	unsigned int Irq_Call_Adresse;
	int Running_ID;
} TSTART_FILTER;

typedef struct _TCA_SYSTEM82
{
	WORD wCA_Type;
	WORD wECM;
	WORD wEMM;
	WORD wRes0; // Visual C++ aligment to 8 bytes
	DWORD dwProviderId;
} TCA_SYSTEM82, *LPTCA_SYSTEM82;

typedef struct _TPID_FILTER
{
	char szFilterName[5];
	BYTE byFilterId;
	WORD wPID;
} TPID_FILTER, *LPTPID_FILTER;

typedef struct _TPROGRAM82
{
	char szName[30];
	char szProvider[30];
	char szCountry[30];
	WORD wRes0; // Visual C++ aligment to 8 bytes
	DWORD dwFreq;
	BYTE byType;
	BYTE byPol;
	BYTE byAfc;
	BYTE byDiseqc;
	WORD wSR;
	WORD wQam;
	WORD wFEC;
	BYTE byNorm;
	BYTE byRes0; // Visual C++ aligment to 8 bytes
	WORD wTpId;
	WORD wVideoPid;
	WORD wAudioPid;
	WORD wTxtPid;
	WORD wPMT;
	WORD wPCR;
	WORD wECM;
	WORD wSID;
	WORD wAC3Pid;
	BYTE byTVType;
	BYTE byServiceType;
	BYTE byCA;
	BYTE byRes1; // Visual C++ aligment to 8 bytes
	WORD wTempAudio;
	WORD wFilterCount;
	TPID_FILTER Filters[MAX_PID_IDS];
	WORD wCACount;
	TCA_SYSTEM82 CA[MAX_CA_SYSTEMS];
	char szCACountry[5];
	BYTE byMerker;
	WORD wLinkTP;
	WORD wLinkSID;
	BYTE byDynamic;
	char szBuffer[16];
} TPROGRAM82, *LPTPROGRAM82;

typedef struct _TPROGRAMNUMBER 
{
	int RealNumber;
	int VirtNumber;
} TPROGRAMNUMBER, *LPTPROGRAMNUMBER;

typedef void (_cdecl *TMDAPIFilterProc)(ULONG hFilter,  ULONG Len,  BYTE* buf);

typedef void (_cdecl *Plugin_Init_Proc)(HINSTANCE, HWND, BOOL, int, char* hotKey, char* vers, int* returnValue);	
typedef void (_cdecl *Plugin_Exit_Proc)();
typedef void (_cdecl *Plugin_Channel_Change_Proc)(TPROGRAM82 TP);
typedef void (_cdecl *Plugin_Hot_Key_Proc)();
typedef void (_cdecl *Plugin_OSD_Key_Proc)(unsigned char osdKey);
typedef void (_cdecl *Plugin_Menu_Cmd_Proc)(int menuID);
typedef void (_cdecl *Plugin_Stream_Proc)(int id, int laenge, unsigned char* daten);
typedef void (_cdecl *Plugin_Send_Dll_ID_Name_Proc)(char* name);
typedef void (_cdecl *Plugin_Filter_Close_Proc)(int filterId);
typedef void (_cdecl *Plugin_Extern_RecPlay_Proc)(int command);
