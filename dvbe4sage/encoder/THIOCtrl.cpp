#include "stdafx.h"

#include "graph.h"

BOOL DVBSFilterGraph::BDAIOControl( DWORD  dwIoControlCode,
									LPVOID lpInBuffer,
									DWORD  nInBufferSize,
									LPVOID lpOutBuffer,
									DWORD  nOutBufferSize,
									LPDWORD lpBytesReturned)
{
    if (!m_KsTunerPropSet)
        return FALSE;

    KSPROPERTY instance_data;
    THBDACMD THBDACmd;

    THBDACmd.CmdGUID = GUID_THBDA_CMD;
    THBDACmd.dwIoControlCode = dwIoControlCode;
    THBDACmd.lpInBuffer = lpInBuffer;
    THBDACmd.nInBufferSize = nInBufferSize;
    THBDACmd.lpOutBuffer = lpOutBuffer;
    THBDACmd.nOutBufferSize = nOutBufferSize;
    THBDACmd.lpBytesReturned = lpBytesReturned;

    HRESULT hr = m_KsTunerPropSet->Set(GUID_THBDA_TUNER, 
                              NULL, 
	  						  &instance_data, sizeof(instance_data),
                              &THBDACmd, sizeof(THBDACmd));

    if (FAILED(hr))
        return FALSE;
    else
        return TRUE;
}

BOOL DVBSFilterGraph::THBDA_IOCTL_SET_PID_FILTER_INFO_Fun(VOID)
{
	BOOL bResult	= FALSE;
	DWORD   nBytes  = 0;
	PID_FILTER_INFO PidInf;

	memset(&PidInf,0,sizeof(PID_FILTER_INFO));

//=====================================================================================================
//			Added by Michael, needed only for Starbox version 1
//=====================================================================================================

	PidInf.MaxPidsNum = 3;
	PidInf.PIDFilterMode = PID_FILTER_MODE_PASS;
	PidInf.CurPidValidMap = 0xfc;
	PidInf.PidValue[0] = 0x00;
	PidInf.PidValue[1] = 0x11;
	PidInf.PidValue[2] = 0x12;
//=====================================================================================================
//			End of added by Michael
//=====================================================================================================

	bResult = BDAIOControl(	(DWORD)THBDA_IOCTL_SET_PID_FILTER_INFO,
							(LPVOID)&PidInf, 
							sizeof(PID_FILTER_INFO),     
							NULL, 
							0,                    
							(LPDWORD)&nBytes);

	if (bResult==FALSE || nBytes != 0)
	{
		return FALSE;
	}

	return bResult;
}

BOOL DVBSFilterGraph::THBDA_IOCTL_GET_PID_FILTER_INFO_Fun(VOID)
{
	BOOL bResult	= FALSE;
	DWORD   nBytes  = 0;
	PID_FILTER_INFO PidInf; 
	int i;

	memset(&PidInf,0,sizeof(PID_FILTER_INFO));

	bResult = BDAIOControl(	(DWORD)THBDA_IOCTL_GET_PID_FILTER_INFO,
							(LPVOID)NULL, 
							0,     
							&PidInf, 
							sizeof(PID_FILTER_INFO),                    
							(LPDWORD)&nBytes);
	if (bResult==FALSE || nBytes != sizeof(PID_FILTER_INFO))
	{
		return FALSE;
	}

	printf("PID info: PIDFilterMode = %d\n", PidInf.PIDFilterMode);
	printf("PID info: MaxPidsNum = %d\n", PidInf.MaxPidsNum);
	if (PidInf.PIDFilterMode == PID_FILTER_MODE_PASS)
	{
		printf("PID info: CurPidValidMap = 0x%02x\n", PidInf.CurPidValidMap);
		for (i=0; i<PidInf.MaxPidsNum; i++)
			printf("Pid %d value = 0x%04x\n", i, PidInf.PidValue[i]);
	}
	return bResult;
}

//***************************************************************//
//************** Basic IOCTL sets  (must support) ***************//
//***************************************************************//

BOOL DVBSFilterGraph::THBDA_IOCTL_CHECK_INTERFACE_Fun(void)
{
    BOOL bResult = FALSE;
    DWORD nBytes = 0;

	DWORD tmp;
    bResult = BDAIOControl((DWORD) THBDA_IOCTL_CHECK_INTERFACE,
									&tmp,
									4,
									NULL,
									0,
									(LPDWORD)&nBytes);
    return bResult;
}

BOOL DVBSFilterGraph::THBDA_IOCTL_SET_REG_PARAMS_DATA_Fun(THBDAREGPARAMS *pTHBDAREGPARAMS)
{	
	BOOL bResult	= FALSE;
	DWORD   nBytes  = 0;	

	bResult = BDAIOControl(	(DWORD)THBDA_IOCTL_SET_REG_PARAMS,
							(LPVOID)pTHBDAREGPARAMS, 
							sizeof(THBDAREGPARAMS),     
							NULL, 
							0,                    
							(LPDWORD)&nBytes);
	return bResult;
}

BOOL DVBSFilterGraph::THBDA_IOCTL_GET_REG_PARAMS_Fun(THBDAREGPARAMS *pTHBDAREGPARAMS)
{	
	BOOL bResult	= FALSE;
	DWORD   nBytes  = 0;	

	bResult = BDAIOControl(	(DWORD)THBDA_IOCTL_GET_REG_PARAMS,							     
							NULL, 
							0,
							(LPVOID)pTHBDAREGPARAMS, 
							sizeof(THBDAREGPARAMS),                    
							(LPDWORD)&nBytes);
	return bResult;
}

BOOL DVBSFilterGraph::THBDA_IOCTL_GET_DEVICE_INFO_Fun(DEVICE_INFO *pDEVICE_INFO)
{	
	BOOL bResult	= FALSE;
	DWORD   nBytes  = 0;	

	bResult = BDAIOControl(	(DWORD)THBDA_IOCTL_GET_DEVICE_INFO,							     
							NULL, 
							0, 
							(LPVOID)pDEVICE_INFO, 
							sizeof(DEVICE_INFO),                   
							(LPDWORD)&nBytes);
	return bResult;
}

BOOL DVBSFilterGraph::THBDA_IOCTL_GET_DRIVER_INFO_Fun(DriverInfo *pDriverInfo)
{	
	BOOL bResult	= FALSE;
	DWORD   nBytes  = 0;	

	bResult = BDAIOControl(	(DWORD)THBDA_IOCTL_GET_DRIVER_INFO,							     
							NULL, 
							0, 
							(LPVOID)pDriverInfo, 
							sizeof(DriverInfo),                   
							(LPDWORD)&nBytes);
	return bResult;
}

BOOL DVBSFilterGraph::THBDA_IOCTL_SET_TUNER_POWER_Fun(BYTE TunerPower)
{	
	BOOL bResult	= FALSE;
	DWORD   nBytes  = 0;	

	bResult = BDAIOControl(	(DWORD)THBDA_IOCTL_SET_TUNER_POWER,
							(LPVOID)&TunerPower, 
							sizeof(BYTE),     
							NULL, 
							0,                    
							(LPDWORD)&nBytes);
	return bResult;
}

BOOL DVBSFilterGraph::THBDA_IOCTL_GET_TUNER_POWER_Fun(BYTE *pTunerPower)
{	
	BOOL bResult	= FALSE;
	DWORD   nBytes  = 0;	

	bResult = BDAIOControl(	(DWORD)THBDA_IOCTL_GET_TUNER_POWER,							    
							NULL, 
							0, 
							(LPVOID)pTunerPower, 
							sizeof(BYTE),                    
							(LPDWORD)&nBytes);
	return bResult;
}

//***************************************************************//
//********************* DVB-S (must support)*********************//
//***************************************************************//

BOOL DVBSFilterGraph::THBDA_IOCTL_SET_LNB_DATA_Fun(ULONG ulLNBLOFLowBand,        // LOF Low Band, in KHz
												   ULONG ulLNBLOFHighBand,       // LOF High Band, in KHz
												   ULONG ulLNBLOFHiLoSW)         // LOF High/Low Band Switch Freq, in KHz)
{	
	BOOL bResult	= FALSE;
	DWORD   nBytes  = 0;	

	m_LNB_Data.DiSEqC_Port = 0;				
	m_LNB_Data.LNB_POWER = 1;													// Make sure LNB power is set!
	m_LNB_Data.ulLNBLOFLowBand = ulLNBLOFLowBand;
	m_LNB_Data.ulLNBLOFHighBand = ulLNBLOFHighBand;
	m_LNB_Data.ulLNBLOFHiLoSW = ulLNBLOFHiLoSW;
	m_LNB_Data.Tone_Data_Burst = 0;
	m_LNB_Data.f22K_Output = 0;

	bResult = BDAIOControl(	(DWORD)THBDA_IOCTL_SET_LNB_DATA,
							(LPVOID)&m_LNB_Data, 
							sizeof(LNB_DATA),     
							NULL, 
							0,                    
							(LPDWORD)&nBytes);
	return bResult;
}

BOOL DVBSFilterGraph::THBDA_IOCTL_GET_LNB_DATA_Fun()
{	
	BOOL bResult	= FALSE;
	DWORD   nBytes  = 0;	

	bResult = BDAIOControl(	(DWORD)THBDA_IOCTL_GET_LNB_DATA,							  
							NULL, 
							0,
							(LPVOID)&m_LNB_Data, 
							sizeof(LNB_DATA),                       
							(LPDWORD)&nBytes);
	return bResult;
}

//***************************************************************//
//****************** CI & MMI (must support for CI)**************//
//***************************************************************//

BOOL DVBSFilterGraph::THBDA_IOCTL_CI_GET_STATE_Fun(THCIState *pTHCIState)
{	
	BOOL bResult	= FALSE;
	DWORD   nBytes  = 0;	

	bResult = BDAIOControl(	(DWORD)THBDA_IOCTL_CI_GET_STATE,							  
							NULL, 
							0,
							(LPVOID)pTHCIState, 
							sizeof(THCIState),                       
							(LPDWORD)&nBytes);
	return bResult;
}

BOOL DVBSFilterGraph::THBDA_IOCTL_CI_GET_APP_INFO_Fun(THAppInfo *pTHAppInfo)
{	
	BOOL bResult	= FALSE;
	DWORD   nBytes  = 0;	

	bResult = BDAIOControl(	(DWORD)THBDA_IOCTL_CI_GET_APP_INFO,							  
							NULL, 
							0,
							(LPVOID)pTHAppInfo, 
							sizeof(THAppInfo),                       
							(LPDWORD)&nBytes);
	return bResult;
}

BOOL DVBSFilterGraph::THBDA_IOCTL_CI_INIT_MMI_Fun(void)
{	
	BOOL bResult	= FALSE;
	DWORD   nBytes  = 0;	

	bResult = BDAIOControl(	(DWORD)THBDA_IOCTL_CI_INIT_MMI,							  
							NULL, 
							0,
							NULL, 
							0,                       
							(LPDWORD)&nBytes);
	return bResult;
}

BOOL DVBSFilterGraph::THBDA_IOCTL_CI_GET_MMI_Fun(THMMIInfo *pTHMMIInfo)
{	
	BOOL bResult	= FALSE;
	DWORD   nBytes  = 0;	

	bResult = BDAIOControl(	(DWORD)THBDA_IOCTL_CI_GET_MMI,							  
							NULL, 
							0,
							(LPVOID)pTHMMIInfo, 
							sizeof(THMMIInfo),                       
							(LPDWORD)&nBytes);
	return bResult;
}

BOOL DVBSFilterGraph::THBDA_IOCTL_CI_ANSWER_Fun(THMMIInfo *pTHMMIInfo)
{	
	BOOL bResult	= FALSE;
	DWORD   nBytes  = 0;	

	bResult = BDAIOControl(	(DWORD)THBDA_IOCTL_CI_ANSWER,	
							(LPVOID)pTHMMIInfo, 
							sizeof(THMMIInfo),				  
							NULL, 
							0,							                       
							(LPDWORD)&nBytes);
	return bResult;
}

BOOL DVBSFilterGraph::THBDA_IOCTL_CI_CLOSE_MMI_Fun(void)
{	
	BOOL bResult	= FALSE;
	DWORD   nBytes  = 0;	

	bResult = BDAIOControl(	(DWORD)THBDA_IOCTL_CI_CLOSE_MMI,							  
							NULL, 
							0,
							NULL, 
							0,                       
							(LPDWORD)&nBytes);
	return bResult;
}

BOOL DVBSFilterGraph::THBDA_IOCTL_CI_SEND_PMT_Fun(PBYTE pBuff, BYTE byBuffSize)
{
    BOOL bResult = FALSE;
    DWORD nBytes = 0;

    bResult = BDAIOControl( (DWORD)THBDA_IOCTL_CI_SEND_PMT,
							(LPVOID)pBuff,
							byBuffSize,
							NULL,
							0,
							(LPDWORD)&nBytes);
    return bResult;
}

//***************************************************************//
//****************** PID filters (for 7021, 7041) ***************//
//***************************************************************//


//***************************************************************//
//******************* Miscellaneous (Optional) ******************//
//***************************************************************//


//***************************************************************//
//********************* DVB-T ***********************************//
//***************************************************************//


//***************************************************************//
//********************* DVB-C ***********************************//
//***************************************************************//



//***************************************************************//
//***** Stream Capture (optional, used in 7045 BDA Agent) *******//
//***************************************************************//


//***************************************************************//
//**** DVB-S with virtual DVB-T interface for MCE (Optional) ****//
//***************************************************************//


//***************************************************************//
//******* Tuner firmware download (optional, used in 704C) ******//
//***************************************************************//