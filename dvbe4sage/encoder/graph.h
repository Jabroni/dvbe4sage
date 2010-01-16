#pragma once

#include "GenericFilterGraph.h"
#include "THIOCtrl.h"

class DVBSFilterGraph : public GenericFilterGraph
{
private:
    CComPtr<ITuningSpace>	m_pITuningSpace;			// The tuning space
    CComPtr<ITuner>			m_pITuner;					// The tuner
    CComPtr<IBaseFilter>	m_pNetworkProvider;			// for network provider filter
    CComPtr<IBaseFilter>	m_pTunerDemodDevice;		// for tuner device filter
    CComPtr<IBaseFilter>	m_pCaptureDevice;			// for capture device filter

    CComPtr<IKsPropertySet>	m_KsTunerPropSet;			// IKsPropertySet for tuner
    CComPtr<IKsPropertySet>	m_KsDemodPropSet;			// IKsPropertySet for demod

    CComPtr<IPin>			m_pTunerPin;				// the tuner pin on the tuner/demod filter
    CComPtr<IPin>			m_pDemodPin;				// the demod pin on the tuner/demod filter

	//DummyNetworkProvider	m_NetworkProvider;			// Our dummy network provider

	TCHAR					m_TunerName[100];			// Tuner friendly name
	USHORT					m_pmtPid;					// Current PMT pid
	LNB_DATA				m_LNB_Data;					// LNB data

    HRESULT LoadNetworkProvider();
    virtual void ReleaseInterfaces();

    HRESULT CreateDVBSTuneRequest(IDVBTuneRequest** pTuneRequest);

    HRESULT CreateTuningSpace();

	// No default or copy constructor!
	DVBSFilterGraph();
	DVBSFilterGraph(const DVBSFilterGraph&);

public:
    DVBSFilterGraph(UINT ordinal);

	virtual HRESULT BuildGraph();
    virtual HRESULT TearDownGraph();
  	
	bool GetTunerStatus(BOOLEAN *pLocked, LONG *pQuality, LONG *pStrength);	
	BOOL ChangeSetting(void);

	LPCTSTR getTunerName() const { return m_TunerName; }

	static int getNumberOfTuners();

    ULONG						m_ulCarrierFrequency;
	ULONG						m_ulSymbolRate;
	Polarisation				m_SignalPolarisation;
	ModulationType				m_Modulation;
	BinaryConvolutionCodeRate	m_FECRate;
	bool						m_IsHauppauge;				// True if the device is Hauppauge
	bool						m_IsFireDTV;				// True if the device is FireDTV
	bool						m_IsTTBDG2;					// True if the device is a TechnoTrend Budget 2
	bool						m_IsTTUSB2;					// True if the device is a TechnoTrend USB 2.0

	// Twinhan specific stuff
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
	BOOL THBDA_IOCTL_SET_LNB_DATA_Fun(ULONG ulLNBLOFLowBand, ULONG ulLNBLOFHighBand, ULONG ulLNBLOFHiLoSW);
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
