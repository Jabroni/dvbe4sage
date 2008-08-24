
// dvbe4sageDlg.h : header file
//

#pragma once

#include "encoder.h"
#include "afxcmn.h"
#include "afxwin.h"
#include "TrayDialog.h"

// CDVBE4SageDlg dialog
class CDVBE4SageDlg : public CTrayDialog
{
	friend DWORD WINAPI logTabWorkerThreadRoutine(LPVOID param);
// Construction
public:
	CDVBE4SageDlg(CWnd* pParent = NULL);	// standard constructor
	virtual ~CDVBE4SageDlg();

// Dialog Data
	enum { IDD = IDD_DVBE4SAGE_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON				m_hIcon;
	CMenu				m_MainMenu;
	Encoder*			m_pEncoder;
	FILE*				m_LogFile;
	HANDLE				m_WorkerThread;
	bool				m_ContinueToRun;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
public:
	CString m_LogContent;
	afx_msg void OnTcnSelchangeMainTab(NMHDR *pNMHDR, LRESULT *pResult);
	CTabCtrl m_MainTabControl;
	afx_msg void OnOperationsStartrecording();
	CEdit m_LogBox;
	afx_msg void OnEnUpdateLog();
	afx_msg void OnOperationsExit();
	afx_msg void OnClose();

	int getNumberOfTuners() const;
	LPCTSTR getTunerFriendlyName(int i) const;
	int getTunerOrdinal(int i) const;
	bool continueToRun() const { return m_ContinueToRun; }
};
