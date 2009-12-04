// NewRecording.cpp : implementation file
//

#include "stdafx.h"
#include "dvbe4sage.h"
#include "NewRecording.h"
#include "dvbe4sagedlg.h"
#include "extern.h"

// NewRecording dialog

IMPLEMENT_DYNAMIC(NewRecording, CDialog)

NewRecording::NewRecording(CWnd* pParent /*=NULL*/)
	: CDialog(NewRecording::IDD, pParent)
	, m_pParentDialog((CDVBE4SageDlg*)pParent)
	, m_TunerFrequency(_T("10842000"))
	, m_TunerSymbolRate(_T("27500"))
	, m_TunerPolarization(_T("Vertical"))
	, m_TunerModulation(_T("QPSK"))
	, m_TunerFEC(_T("3/4"))
	, m_RecordingTunerOrdinal(_T(""))
	, m_RecordingChannelNumber(_T("1"))
	, m_RecordingDuration(_T("60"))
	, m_OutputFileName(_T("Channel1.ts"))
	, m_TunerName(_T(""))
	, m_UseSID(FALSE)
	, m_TransponderAutodiscovery(TRUE)
	, m_bFirstTime(true)
	, m_DumpFullTransponder(false)
{
}

NewRecording::~NewRecording()
{
}

void NewRecording::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_TRANSPONDER, m_FrequencyEdit);
	DDX_Text(pDX, IDC_TRANSPONDER, m_TunerFrequency);
	DDV_MaxChars(pDX, m_TunerFrequency, 8);

	DDX_Control(pDX, IDC_SR, m_SymbolRateEdit);
	DDX_Text(pDX, IDC_SR, m_TunerSymbolRate);
	DDV_MaxChars(pDX, m_TunerSymbolRate, 5);

	DDX_Control(pDX, IDC_POLARIZATION, m_PolarizationCombo);
	DDX_CBString(pDX, IDC_POLARIZATION, m_TunerPolarization);

	DDX_Control(pDX, IDC_MODULATION, m_ModulationCombo);
	DDX_CBString(pDX, IDC_MODULATION, m_TunerModulation);

	DDX_Control(pDX, IDC_FEC, m_FECCombo);
	DDX_CBString(pDX, IDC_FEC, m_TunerFEC);

	DDX_Text(pDX, IDC_TUNER_ORDINAL, m_RecordingTunerOrdinal);
	DDV_MaxChars(pDX, m_RecordingTunerOrdinal, 2);

	DDX_Text(pDX, IDC_CHANNEL, m_RecordingChannelNumber);
	DDV_MaxChars(pDX, m_RecordingChannelNumber, 5);

	DDX_Text(pDX, IDC_DURATION, m_RecordingDuration);
	DDX_Text(pDX, IDC_OUTPUT_FILE_NAME, m_OutputFileName);
	
	DDX_Control(pDX, IDC_TUNER_NAME, m_TunerNameBox);
	DDX_CBString(pDX, IDC_TUNER_NAME, m_TunerName);
	
	DDX_Check(pDX, IDC_DISCOVER_TRANSPONDER, m_TransponderAutodiscovery);
	DDX_Check(pDX, IDC_USE_SID, m_UseSID);
	DDX_Check(pDX, IDC_DUMP_FULL_TRANSPONDER, m_DumpFullTransponder);
}


BEGIN_MESSAGE_MAP(NewRecording, CDialog)
	ON_BN_CLICKED(IDC_BUTTON1, &NewRecording::OnBnClickedBrowse)
	ON_EN_KILLFOCUS(IDC_CHANNEL, &NewRecording::OnEnKillfocusChannel)
	ON_BN_CLICKED(IDC_DISCOVER_TRANSPONDER, &NewRecording::OnBnClickedDiscoverTransponder)
	ON_CBN_SELENDOK(IDC_TUNER_NAME, &NewRecording::OnCbnSelendokTunerName)
END_MESSAGE_MAP()


// NewRecording message handlers

BOOL NewRecording::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_FrequencyEdit.EnableWindow(!m_TransponderAutodiscovery);
	m_SymbolRateEdit.EnableWindow(!m_TransponderAutodiscovery);
	m_PolarizationCombo.EnableWindow(!m_TransponderAutodiscovery);
	m_ModulationCombo.EnableWindow(!m_TransponderAutodiscovery);
	m_FECCombo.EnableWindow(!m_TransponderAutodiscovery);

	for(int i = 0; i < getNumberOfTuners(); i++)
	{
		LPCTSTR tunerFriendlyName = getTunerFriendlyName(i);
		m_TunerNameBox.AddString(tunerFriendlyName);
		m_TunerNameBox.SetItemData(i, getTunerOrdinal(i));
	}
	
	if(m_bFirstTime && getNumberOfTuners() > 0)
	{
		m_bFirstTime = false;
		m_TunerNameBox.SetCurSel(0);

		TCHAR buffer[2];
		_itot_s(m_TunerNameBox.GetItemData(0), buffer, sizeof(buffer) / sizeof(buffer[0]), 10);
		m_RecordingTunerOrdinal = buffer;
	}

	UpdateData(FALSE);
	return TRUE;
}

void NewRecording::OnBnClickedBrowse()
{
	UpdateData(TRUE);
	CString fileName;
	CFileDialog dlg(TRUE, _T("ts"), NULL, OFN_OVERWRITEPROMPT, NULL, this);
	if(dlg.DoModal() == IDOK)
	{
		m_OutputFileName = dlg.GetPathName();
		UpdateData(FALSE);
	}
}

void NewRecording::OnEnKillfocusChannel()
{
	UpdateData(TRUE);
	m_OutputFileName = m_OutputFileName.SpanExcluding(TEXT("0123456789."));
	m_OutputFileName += m_RecordingChannelNumber + TEXT(".ts");
	UpdateData(FALSE);
}

void NewRecording::OnBnClickedDiscoverTransponder()
{
	UpdateData(TRUE);
	m_FrequencyEdit.EnableWindow(!m_TransponderAutodiscovery);
	m_SymbolRateEdit.EnableWindow(!m_TransponderAutodiscovery);
	m_PolarizationCombo.EnableWindow(!m_TransponderAutodiscovery);
	m_ModulationCombo.EnableWindow(!m_TransponderAutodiscovery);
	m_FECCombo.EnableWindow(!m_TransponderAutodiscovery);
	UpdateData(FALSE);
}

void NewRecording::OnCbnSelendokTunerName()
{
	UpdateData(TRUE);
	TCHAR buffer[2];
	int curSel = m_TunerNameBox.GetCurSel();
	_itot_s(m_TunerNameBox.GetItemData(curSel), buffer, sizeof(buffer) / sizeof(buffer[0]), 10);
	m_RecordingTunerOrdinal = buffer;
	UpdateData(FALSE);
}
