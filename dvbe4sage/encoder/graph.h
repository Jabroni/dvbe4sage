#pragma once

#include "THIOCtrl.h"
#include "DVBFilter.h"

class CBDAFilterGraph
{
private:
    CComPtr<ITuningSpace>	m_pITuningSpace;			// The tuning space
    CComPtr<ITuner>			m_pITuner;					// The tuner
    CComPtr<IGraphBuilder>	m_pFilterGraph;				// for current graph
    CComPtr<IMediaControl>	m_pIMediaControl;			// for controlling graph state
    CComPtr<ICreateDevEnum>	m_pICreateDevEnum;			// for enumerating system devices
    CComPtr<IBaseFilter>	m_pNetworkProvider;			// for network provider filter
    CComPtr<IBaseFilter>	m_pTunerDemodDevice;		// for tuner device filter
    CComPtr<IBaseFilter>	m_pCaptureDevice;			// for capture device filter
	CComPtr<IBaseFilter>	m_pDemux;					// for demux filter
    CComPtr<IBaseFilter>	m_pTIF;						// for transport information filter

    CComPtr<IKsPropertySet>	m_KsTunerPropSet;			// IKsPropertySet for tuner
    CComPtr<IKsPropertySet>	m_KsDemodPropSet;			// IKsPropertySet for demod

    CComPtr<IPin>			m_pTunerPin;				// the tuner pin on the tuner/demod filter
    CComPtr<IPin>			m_pDemodPin;				// the demod pin on the tuner/demod filter

	DVBFilter*				m_pDVBFilter;				// Our filter
	DummyNetworkProvider	m_NetworkProvider;			// Our dummy network provider

	TCHAR					m_TunerName[100];			// Tuner friendly name
	int						m_iTunerNumber;				// Tuner ordinal number
	USHORT					m_pmtPid;					// Current PMT pid
	LNB_DATA				m_LNB_Data;					// LNB data

    HRESULT InitializeGraphBuilder();
    HRESULT LoadNetworkProvider();
	HRESULT LoadDemux();
    HRESULT RenderDemux();
    void    BuildGraphError();
    void    ReleaseInterfaces();

    HRESULT LoadFilter(REFCLSID clsid, 
					   IBaseFilter** ppFilter,
					   IBaseFilter* pConnectFilter, 
					   BOOL fIsUpstream,
					   BOOL useCounter);

    HRESULT ConnectFilters(IBaseFilter* pFilterUpstream, 
						   IBaseFilter* pFilterDownstream);

    HRESULT CreateDVBSTuneRequest(IDVBTuneRequest** pTuneRequest);

    HRESULT CreateTuningSpace();

	// No default or copy constructor!
	CBDAFilterGraph();
	CBDAFilterGraph(CBDAFilterGraph&);

public:
    bool m_fGraphBuilt;
    bool m_fGraphRunning;
    bool m_fGraphFailure;

    CBDAFilterGraph(int ordinal,
					ULONG initialFrequency,
					ULONG initialSymbolRate,
					Polarisation initialPolarization,
					ModulationType initialModulation,
					BinaryConvolutionCodeRate initialFecRate);
					//PluginsHandler* const pPluginsHandler);
    ~CBDAFilterGraph();

	HRESULT BuildGraph();
    HRESULT RunGraph();
    HRESULT StopGraph();
    HRESULT TearDownGraph();
  	
	bool GetTunerStatus(BOOLEAN *pLocked, LONG *pQuality, LONG *pStrength);	
	BOOL ChangeSetting(void);

	DVBParser& getParser() { return m_pDVBFilter->getParser(); }
	bool startRecording();

	LPCTSTR getTunerName() const { return m_TunerName; }
	int getTunerOrdinal() const { return m_iTunerNumber; }

	static int getNumberOfTuners();

    ULONG						m_ulCarrierFrequency;
	ULONG						m_ulSymbolRate;
	Polarisation				m_SignalPolarisation;
	ModulationType				m_Modulation;
	BinaryConvolutionCodeRate	m_FECRate;
	USHORT						m_Tid;

	// Twinhan specific stuff
	IPin* FindPinOnFilter(IBaseFilter *pBaseFilter, char *pPinName,BOOL bCheckPinName);	
	BOOL GetTunerDemodPropertySetInterfaces();
	BOOL BDAIOControl(DWORD  dwIoControlCode,
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
