// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

// Modify the following defines if you have to target a platform prior to the ones specified below.
// Refer to MSDN for the latest info on corresponding values for different platforms.
#ifndef WINVER				// Allow use of features specific to Windows XP or later.
#define WINVER 0x0502		// Change this to the appropriate value to target other versions of Windows.
#endif

#ifndef _WIN32_WINNT		// Allow use of features specific to Windows XP or later.                   
#define _WIN32_WINNT 0x0502	// Change this to the appropriate value to target other versions of Windows.
#endif						

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS	// some CString constructors will be explicit

#include <atlbase.h>
#include <atlstr.h>
#include <atlcom.h>
#include <atlwin.h>
#include <atltypes.h>
#include <atlctl.h>
#include <atlhost.h>

#include <strmif.h>
#include <ks.h>
#include <ksmedia.h>
#include <bdatypes.h>
#include <bdamedia.h>
#include <bdaiface.h>
#include <bdatif.h>
#include <tuner.h>
#include <mmsystem.h>
#include <wxdebug.h>
#include <wxutil.h>
#include <pullpin.h>
#include <uuids.h>
#include <control.h>
#include <commctrl.h>
#include <tchar.h>
#include <vfwmsgs.h>
#include <streams.h>
#include <wchar.h>
#include <time.h>
#include <stdio.h>
#include <winsock2.h>
#include <io.h>
#include <time.h>
#include <share.h>
#include <stdlib.h>
#include <tchar.h>

#include <string>
#include <hash_map>
#include <hash_set>
#include <vector>
#include <iterator>
#include <list>
#include <deque>
#include <iostream>
#include <fstream>
#include <sstream>

#include <ntverp.h>

#include <ttBdaDrvApi.h>

#include <FFdecsa.h>