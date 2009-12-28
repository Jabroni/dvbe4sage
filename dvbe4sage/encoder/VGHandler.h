#pragma once

///<summary>The prototype of the callback which returns the DW from the ECP request.</summary>
///<param name="pParm">The pointer parameter provided to the VGHandler_GetECM</param>
///<param name="nParm">The integer parameter provided to the VGHandler_GetECM</param>
///<param name="pDCW">A pointer to the DCW returned from VGHandler_GetECM</param>
typedef void (WINAPI *DCWCallback_t)(void *pParm, int nParm, unsigned char pDCW[16]);

///<summary>The prototype of the callback which returns the card status message.</summary>
///<param name="pParm">The pointer parameter provided to the VGHandler_GetCardStatus</param>
///<param name="nParm">The integer parameter provided to the VGHandler_GetCardStatus</param>
///<param name="pDCW">A pointer to the string returned from VGHandler_GetCardStatus</param>
typedef void (WINAPI *CardStatusCallback_t)(void *pParm, int nParm, const char *szCardStatus);

///<summary>Initializes the VG Service.</summary>
///<returns>True if message was sent to VGService successfully.</returns>
typedef BOOL (WINAPI *VGHandler_Init_t)();
///<summary>Initializes the VG Service.</summary>
///<returns>True if message was sent to VGService successfully.</returns>
typedef void (WINAPI *VGHandler_Free_t)();
///<summary>Sends the card status request to the VG Service.</summary>
///<param name="pParm">The pointer parameter which will be returned together with pCallback</param>
///<param name="nParm">The integer parameter which will be returned together with pCallback</param>
///<param name="pCallback">The callback to be used to return the status</param>
///<param name="nTimeoutMS">The amount of time to wait on the VGService input queue</param>
///<returns>True if message was sent to VGService successfully.</returns>
typedef BOOL (WINAPI *VGHandler_GetCardStatus_t)(void *pParm, int nParm, CardStatusCallback_t pCallback, int nTimeoutMS);
///<summary>Sends the ECM request to the VG Service.</summary>
///<param name="pParm">The pointer parameter which will be returned together with pCallback</param>
///<param name="nParm">The integer parameter which will be returned together with pCallback</param>
///<param name="pECM">A buffer with the ECM data/param>
///<param name="pCallback">The callback to be used to return the DCW</param>
///<param name="nTimeoutMS">The amount of time to wait on the VGService input queue</param>
///<returns>True if message was sent to VGService successfully.</returns>
typedef BOOL (WINAPI *VGHandler_GetECM_t)(void *pParm, int nParm, const unsigned char pECM[256], DCWCallback_t pCallback, int nTimeoutMS);
///<summary>Sends the EMM request to the VG Service.</summary>
///<param name="pEMM">A buffer with the EMM data/param>
///<param name="nTimeoutMS">The amount of time to wait on the VGService input queue</param>
///<returns>True if message was sent to VGService successfully.</returns>
typedef BOOL (WINAPI *VGHandler_DoEMM_t)(const unsigned char pEMM[258], int nTimeoutMS);

///summary>
///This function loads the VGHandler libary it returns pointers to the exported functions
///<summary>
#define LoadVGHandler(hinstLib, GetCardStatus, GetECM, DoEMM)                                                   \
{                                                                                                               \
  hinstLib = LoadLibrary(TEXT("VGHandler"));                                                                     \
  if (hinstLib != NULL)                                                                                         \
  {                                                                                                             \
    BOOL bSuccess=FALSE;                                                                                        \
    VGHandler_Init_t Init = (VGHandler_Init_t)GetProcAddress(hinstLib, "VGHandler_Init");                          \
    VGHandler_Free_t Free = (VGHandler_Free_t)GetProcAddress(hinstLib, "VGHandler_Free");                          \
    GetCardStatus = (VGHandler_GetCardStatus_t)GetProcAddress(hinstLib, "VGHandler_GetCardStatus");               \
    GetECM = (VGHandler_GetECM_t)GetProcAddress(hinstLib, "VGHandler_GetECM");                                    \
    DoEMM = (VGHandler_DoEMM_t)GetProcAddress(hinstLib, "VGHandler_DoEMM");                                       \
    if (Init && Free && GetCardStatus && GetECM && DoEMM)                                                       \
      bSuccess =Init();                                                                                         \
    if (!bSuccess)                                                                                              \
    {                                                                                                           \
      FreeLibrary(hinstLib);                                                                                    \
      hinstLib = NULL;                                                                                          \
    }                                                                                                           \
  }                                                                                                             \
}                                                                                                               

///<summary>Unloads the previously loaded VGHandler.</summary>                                                   
#define UnloadVGHandler(hinstLib)                                                                               \
{                                                                                                               \
  VGHandler_Free_t Free=NULL;                                                                                   \
  Free = (VGHandler_Free_t)GetProcAddress(hinstLib, "VGHandler_Free");                                          \
  Free();                                                                                                       \
  FreeLibrary(hinstLib);                                                                                        \
}
