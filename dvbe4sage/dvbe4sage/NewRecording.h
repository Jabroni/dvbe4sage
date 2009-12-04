#pragma once
#include "afxwin.h"

class CDVBE4SageDlg;

// NewRecording dialog

class NewRecording : public CDialog
{
private:
	CDVBE4SageDlg*	m_pParentDialog;

	DECLARE_DYNAMIC(NewRecording)

	bool			m_bFirstTime;
public:
	NewRecording(CWnd* pParent = NULL);   // standard constructor
	virtual ~NewRecording();

// Dialog Data
	enum { IDD = IDD_START_RECORDING };

protected:
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CString m_TunerFrequency;
	CString m_TunerSymbolRate;
	CString m_TunerPolarization;
	CString m_TunerModulation;
	CString m_TunerFEC;
	CString m_RecordingTunerOrdinal;
	CString m_RecordingChannelNumber;
	CString m_RecordingDuration;
	CString m_OutputFileName;
	afx_msg void OnBnClickedBrowse();
	CComboBox m_TunerNameBox;
	CString m_TunerName;
	afx_msg void OnEnKillfocusChannel();
	BOOL m_UseSID;
	BOOL m_TransponderAutodiscovery;
	afx_msg void OnBnClickedDiscoverTransponder();
	CEdit m_FrequencyEdit;
	CEdit m_SymbolRateEdit;
	CComboBox m_PolarizationCombo;
	CComboBox m_ModulationCombo;
	CComboBox m_FECCombo;
	afx_msg void OnCbnSelendokTunerName();
	BOOL m_DumpFullTransponder;
};
