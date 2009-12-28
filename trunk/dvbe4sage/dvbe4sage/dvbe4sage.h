
// dvbe4sage.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols


// CDVBE4SageApp:
// See dvbe4sage.cpp for the implementation of this class
//

class CDVBE4SageApp : public CWinApp
{
public:
	CDVBE4SageApp();

// Overrides
	public:
	virtual BOOL InitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern CDVBE4SageApp theApp;