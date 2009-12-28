//------------------------------------------------------------------------------
// File: Graph.h
//
// Desc: Sample code for BDA graph building.
//
//
// Copyright (c) 2003, Conexant Sytems, Inc. All rights reserved.
//------------------------------------------------------------------------------

#ifndef GRAPH_H_INCLUDED_
#define GRAPH_H_INCLUDED_

#include "THIOCtrl.h"

typedef enum _CAM_TYPE_ENUM
{
    CAM_DEFAULT = 1, //Viaccess
    TH_CAM_ASTON = 2,
    TH_CAM_CONAX = 3,
    TH_CAM_CRYPTOWORKS = 4    
}  CAM_TYPE_ENUM;

// Twinhan CAPMT parser Filter Interface
// {B5FAEB6A-D051-43a1-B499-24F4DE1B87EC}
static const GUID IID_ITHPsiParser = 
{ 0xb5faeb6a, 0xd051, 0x43a1, { 0xb4, 0x99, 0x24, 0xf4, 0xde, 0x1b, 0x87, 0xec } };

// Twinhan CAPMT parser Filter Object
// {19712ABA-EA52-4959-BF49-F02E639F27E3}
static const GUID CLSID_THPsiParser = 
{ 0x19712aba, 0xea52, 0x4959, { 0xbf, 0x49, 0xf0, 0x2e, 0x63, 0x9f, 0x27, 0xe3 } }; 

DECLARE_INTERFACE_(IMpeg2PsiParser, IUnknown)
{
	STDMETHOD(SetPMTType) (THIS_
		unsigned int PMTType           //0: Original, 1:CAPMT
	) PURE;

	STDMETHOD(SetCAPMT) (THIS_
		unsigned int VPID, unsigned int APID, unsigned int CamType
	) PURE;

	STDMETHOD(GetPMTData) (THIS_
		PBYTE pBuff, unsigned int *byBuffSize
	) PURE;

	STDMETHOD(SetCAPMT_2) (THIS_
		unsigned int *PIDAry, int PIDNum, unsigned int CamType
	) PURE;
	
	STDMETHOD(Set_ES_CAPMT_CMD_ID) (THIS_
		BYTE Es_capmt_cmd_id
	) PURE;

	STDMETHOD(Set_CAPMT_List_Management) (THIS_
		BYTE CAMPT_List_Management
	) PURE;

	STDMETHOD(Set_SID) (THIS_
		unsigned int SID
	) PURE;
};

class DVBFilterToParserProxy;

class CBDAFilterGraph
{
private:
    CComPtr <ITuningSpace>          m_pITuningSpace;
	CComPtr <ITuningSpaceContainer> m_pTuningSpaceContainer;

    CComPtr <IScanningTuner> m_pITuner;

    CComPtr <IGraphBuilder>  m_pFilterGraph;      // for current graph
    CComPtr <IMediaControl>  m_pIMediaControl;    // for controlling graph state
    CComPtr <ICreateDevEnum> m_pICreateDevEnum;   // for enumerating system devices

    CComPtr <IBaseFilter>    m_pNetworkProvider;  // for network provider filter
    CComPtr <IBaseFilter>    m_pTunerDemodDevice; // for tuner device filter
    CComPtr <IBaseFilter>    m_pCaptureDevice;    // for capture device filter
    CComPtr <IBaseFilter>    m_pDemux;            // for demux filter
    CComPtr <IBaseFilter>    m_pVideoDecoder;     // for mpeg video decoder filter
    CComPtr <IBaseFilter>    m_pAudioDecoder;     // for mpeg audio decoder filter
    CComPtr <IBaseFilter>    m_pTIF;              // for transport information filter
	CComPtr <IBaseFilter>    m_pTHPsiParser;      // for THPSI Parser
    CComPtr <IBaseFilter>    m_pIPSink;           // for ip sink filter
    CComPtr <IBaseFilter>    m_pOVMixer;          // for overlay mixer filter
    CComPtr <IBaseFilter>    m_pVRenderer;        // for video renderer filter
    CComPtr <IBaseFilter>    m_pDDSRenderer;      // for sound renderer filter

	CComPtr <IMpeg2PsiParser> m_pITHPsiParser;

	CComPtr <IMPEG2PIDMap>   m_pIVideoPIDMap;     // for demux filter video pin PID 
    CComPtr <IMPEG2PIDMap>   m_pIAudioPIDMap;     // for demux filter audio pin PID
	CComPtr <IMPEG2PIDMap>   m_pITHParserPIDMap;  // for demux filter THParser pin PID

	CComPtr <IPin>           m_pTHParserPin;      // THPsiParser pin
    CComPtr <IPin>           m_pVideoPin;         // demux filter video pin
    CComPtr <IPin>           m_pAudioPin;         // demux filter audio pin

    CComPtr <IKsPropertySet> m_KsTunerPropSet;    // IKsPropertySet for tuner
    CComPtr <IKsPropertySet> m_KsDemodPropSet;    // IKsPropertySet for demod

    CComPtr <IPin>           m_pTunerPin;         // the tuner pin on the tuner/demod filter
    CComPtr <IPin>           m_pDemodPin;         // the demod pin on the tuner/demod filter

	DVBFilterToParserProxy*	 m_pDVBFilterToParserProxy;		  
												  // A pointer to the filter to parser proxy
	BOOL					 m_bLoadCaptureDevice;// True if need to attempt loading the capture device
	int						 m_iTunerNumber;	  // Tuner ordinal number

    //registration number for the RunningObjectTable
    DWORD                    m_dwGraphRegister;

    HRESULT InitializeGraphBuilder();
    //HRESULT LoadTuningSpace(CComBSTR pNetworkType);
    HRESULT LoadNetworkProvider(CComBSTR pNetworkType);
    HRESULT LoadDemux();
    HRESULT RenderDemux();
    void    BuildGraphError();
    HRESULT BuildAVSegment();
    HRESULT LoadVideoDecoder();
    HRESULT LoadAudioDecoder();
    void    ReleaseInterfaces();

    HRESULT LoadFilter(
        REFCLSID clsid, 
        IBaseFilter** ppFilter,
        IBaseFilter* pConnectFilter, 
        BOOL fIsUpstream,
		BOOL useCounter
        );

    HRESULT ConnectFilters(
        IBaseFilter* pFilterUpstream, 
        IBaseFilter* pFilterDownstream
        );

    HRESULT CreateDVBSTuneRequest(IDVBTuneRequest** pTuneRequest);

    HRESULT CreateTuningSpace(CComBSTR pNetworkType);

    IPin* FindPinOnFilter(IBaseFilter *pBaseFilter, char *pPinName,BOOL bCheckPinName);	

public:
    bool m_fGraphBuilt;
    bool m_fGraphRunning;
    bool m_fGraphFailure;

    CBDAFilterGraph();   
    ~CBDAFilterGraph();

	HRESULT BuildGraph(CComBSTR pNetworkType);
    HRESULT RunGraph();
    HRESULT StopGraph();
    HRESULT TearDownGraph();
  
    HRESULT AddGraphToRot(IUnknown *pUnkGraph, DWORD *pdwRegister);
    void RemoveGraphFromRot(DWORD pdwRegister);


	HRESULT SetVideoAndAudioPIDs(LONG lNewVideoPID, LONG lNewAudioPID);
	HRESULT ComposeCAPMT(  int nCAPMT_ListMgt,  int nCAPMT_CMD_ID, UINT PMT_PID, UINT SID, 
		                   UINT *PIDAry, UINT nPIDNum, PBYTE pBuff_CAPMT, UINT *pbyBuffSize);
	
	BOOL GetTunerStatus(BOOLEAN *pLocked, LONG *pQuality, LONG *pStrength);
	
	BOOL ChangeSetting(void);

	BOOL SetPidInfo(void);
	BOOL GetPidInfo(void);
	BOOL SetDiSEqCCmd(void);
	BOOL GetDiSEqCCmd(void);
	BOOL MCESatePreset(void);
	BOOL MCEVDVBTDisable(void);
	BOOL SetRCConfig(void);
	BOOL GetRCConfig(void);
	CComPtr<IBaseFilter> GetTIF(void) { return m_pTIF; }
	CComPtr<IScanningTuner> GetTuner(void) { return m_pITuner; }

	void SetDVBFilterToParserProxy(DVBFilterToParserProxy* pDVBFilterToParserProxy) { m_pDVBFilterToParserProxy = pDVBFilterToParserProxy; }
	DVBFilterToParserProxy* GetDVBFilterToParserProxy() const { return m_pDVBFilterToParserProxy; }
	void SetTunerOrdinalNumber(int iTunerNumber) { m_iTunerNumber = iTunerNumber; }
	void SetTransponder(ULONG ulCarrierFrequency) { m_ulCarrierFrequency = ulCarrierFrequency; }
	ULONG GetTransponder() const { return m_ulCarrierFrequency; }

	LNB_DATA				 m_LNB_Data;
    ULONG                    m_ulCarrierFrequency;
	ULONG					 m_ulSecondaryCarrierFrequency;
	ULONG                    m_ulSymbolRate;
	UCHAR		             m_SignalPolarisation;

public:  //About THBDA Ioctl functions
	BOOL GetTunerDemodPropertySetInterfaces();
	BOOL BDAIOControl( DWORD  dwIoControlCode,
						LPVOID lpInBuffer,
						DWORD  nInBufferSize,
						LPVOID lpOutBuffer,
						DWORD  nOutBufferSize,
						LPDWORD lpBytesReturned);
	BOOL THBDA_IOCTL_CHECK_INTERFACE_Fun(void);
	BOOL THBDA_IOCTL_SET_REG_PARAMS_DATA_Fun(THBDAREGPARAMS *pTHBDAREGPARAMS);
	BOOL THBDA_IOCTL_GET_REG_PARAMS_Fun(THBDAREGPARAMS *pTHBDAREGPARAMS);
	BOOL THBDA_IOCTL_GET_DEVICE_INFO_Fun(DEVICE_INFO *pDEVICE_INFO);
	BOOL THBDA_IOCTL_GET_DRIVER_INFO_Fun(DriverInfo *pDriverInfo);
	BOOL THBDA_IOCTL_SET_TUNER_POWER_Fun(BYTE TunerPower);
	BOOL THBDA_IOCTL_GET_TUNER_POWER_Fun(BYTE *pTunerPower);

	BOOL THBDA_IOCTL_HID_RC_ENABLE_Fun(BYTE RCEnable);

	BOOL THBDA_IOCTL_SET_LNB_DATA_Fun(VOID);
	BOOL THBDA_IOCTL_GET_LNB_DATA_Fun(VOID);
	BOOL THBDA_IOCTL_SET_PID_FILTER_INFO_Fun(VOID);
	BOOL THBDA_IOCTL_GET_PID_FILTER_INFO_Fun(VOID);
	
	BOOL THBDA_IOCTL_CI_GET_STATE_Fun(THCIState *pTHCIState);
	BOOL THBDA_IOCTL_CI_GET_APP_INFO_Fun(THAppInfo *pTHAppInfo);
	BOOL THBDA_IOCTL_CI_INIT_MMI_Fun(void);
	BOOL THBDA_IOCTL_CI_GET_MMI_Fun(THMMIInfo *pTHMMIInfo);
	BOOL THBDA_IOCTL_CI_ANSWER_Fun(THMMIInfo *pTHMMIInfo);
	BOOL THBDA_IOCTL_CI_CLOSE_MMI_Fun(void);
	BOOL THBDA_IOCTL_CI_SEND_PMT_Fun(PBYTE pBuff, BYTE byBuffSize);
};
 
#endif // GRAPH_H_INCLUDED_
