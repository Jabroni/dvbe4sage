
// dvbe4sageDlg.cpp : implementation file
//

#include "stdafx.h"
#include "dvbe4sage.h"
#include "dvbe4sageDlg.h"
#include "logger.h"
#include "NewRecording.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define WM_READY (WM_USER + 1000)

// This is the worker thread main function waits for events
DWORD WINAPI logTabWorkerThreadRoutine(LPVOID param)
{
	// Get the pointer to the dialog
	CDVBE4SageDlg* const dialog = (CDVBE4SageDlg* const)param;
	BYTE buffer[10240];
	while(dialog->continueToRun())
	{
		Sleep(2000);
		int readBytes = fread(buffer, 1, sizeof(buffer) - 1, dialog->m_LogFile);
		buffer[readBytes] = '\0';
		if(readBytes > 0)
		{
			dialog->m_LogContent.Append((LPCTSTR)buffer);
			dialog->m_LogContent = dialog->m_LogContent.Right(50000);
		}
		::SendMessage(dialog->m_hWnd, WM_READY, (WPARAM)param, 0);
	}
	return 0;
}


// CDVBE4SageDlg dialog


CDVBE4SageDlg::CDVBE4SageDlg(CWnd* pParent /*=NULL*/)
	: CTrayDialog(CDVBE4SageDlg::IDD, pParent),
	m_pEncoder(NULL),
	m_LogFile(NULL),
	m_WorkerThread(NULL),
	m_ContinueToRun(true)
	, m_LogContent(_T(""))
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

CDVBE4SageDlg::~CDVBE4SageDlg()
{
	delete m_pEncoder;
	if(m_WorkerThread != NULL)
	{
		m_ContinueToRun = false;
		WaitForSingleObject(m_WorkerThread, INFINITE);
		CloseHandle(m_WorkerThread);
	}
	if(m_LogFile != NULL)
		fclose(m_LogFile);
}

void CDVBE4SageDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_LOG, m_LogContent);
	DDX_Control(pDX, IDC_MAIN_TAB, m_MainTabControl);
	DDX_Control(pDX, IDC_LOG, m_LogBox);
}

BEGIN_MESSAGE_MAP(CDVBE4SageDlg, CTrayDialog)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
	ON_COMMAND(ID_OPERATIONS_STARTRECORDING, &CDVBE4SageDlg::OnOperationsStartrecording)
	ON_COMMAND(ID_OPERATIONS_EXIT, &CDVBE4SageDlg::OnOperationsExit)
END_MESSAGE_MAP()


// CDVBE4SageDlg message handlers

BOOL CDVBE4SageDlg::OnInitDialog()
{
	CTrayDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	m_MainMenu.LoadMenu(IDR_MAIN_MENU);
	SetMenu(&m_MainMenu);
	m_pEncoder = new Encoder(AfxGetInstanceHandle(), m_hWnd, m_MainMenu);
	
	// Open the log file
	m_LogFile = _tfsopen(g_Logger.getLogFileName(), TEXT("rb"),  _SH_DENYNO);

	// Create worker thread
	m_WorkerThread = CreateThread(NULL, 0, logTabWorkerThreadRoutine, this, 0, NULL);

	// Add a tab and set its title
	m_MainTabControl.InsertItem(0, TEXT("Log"));

	// Make log edit box scrollable
	m_LogBox.ShowScrollBar(SB_BOTH);

	// Tray stuff
	TraySetIcon(IDR_MAINFRAME);
    TraySetToolTip(TEXT("DVB Enhancer for SageTV"));
    TraySetMenu(m_MainMenu);

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CDVBE4SageDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CDVBE4SageDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


LRESULT CDVBE4SageDlg::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	// Pass messages to plugins handler first
	if(m_pEncoder != NULL)
		m_pEncoder->WindowProc(message, wParam, lParam);
	if(message == WM_READY)
	{
		CDVBE4SageDlg* const dialog = (CDVBE4SageDlg* const)wParam;
		dialog->UpdateData(FALSE);
		dialog->m_LogBox.SetSel(m_LogContent.GetLength(), m_LogContent.GetLength());
		dialog->UpdateData(TRUE);
	}
	return CDialog::WindowProc(message, wParam, lParam);
}

void CDVBE4SageDlg::OnOperationsStartrecording()
{
	static NewRecording newRecording(this);
	if(newRecording.DoModal() == IDOK)
	{
		m_pEncoder->startRecording(newRecording.m_TransponderAutodiscovery ? true : false,
								   _ttol(newRecording.m_TunerFrequency),
								   _ttol(newRecording.m_TunerSymbolRate),
								   newRecording.m_TunerPolarization == TEXT("Vertical") ? BDA_POLARISATION_LINEAR_V : BDA_POLARISATION_LINEAR_H,
								   newRecording.m_TunerModulation == TEXT("QPSK") ? BDA_MOD_QPSK : BDA_MOD_8VSB,
								   newRecording.m_TunerFEC == TEXT("3/4") ? BDA_BCC_RATE_3_4 : BDA_BCC_RATE_2_3,
								   _ttoi(newRecording.m_RecordingTunerOrdinal),
								   false,
								   _ttoi(newRecording.m_RecordingChannelNumber),
								   newRecording.m_UseSID ? true : false,
								   _ttoi64(newRecording.m_RecordingDuration),
								   newRecording.m_OutputFileName,
								   NULL,
								   (__int64)-1);
								   
	}
}

int CDVBE4SageDlg::getNumberOfTuners() const
{
	return m_pEncoder->getNumberOfTuners();
}

LPCTSTR CDVBE4SageDlg::getTunerFriendlyName(int i) const
{
	return m_pEncoder->getTunerFriendlyName(i);
}

int CDVBE4SageDlg::getTunerOrdinal(int i) const
{
	return m_pEncoder->getTunerOrdinal(i);
}

void CDVBE4SageDlg::OnOperationsExit()
{
	EndDialog(IDOK);
}

void CDVBE4SageDlg::OnClose()
{
	CTrayDialog::OnSysCommand(SC_MINIMIZE, 0);
}