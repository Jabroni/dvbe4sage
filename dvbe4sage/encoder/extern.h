#pragma once

#ifdef ENCODER_EXPORTS
#define ENCODER_API __declspec(dllexport)
#else
#define ENCODER_API __declspec(dllimport)
#endif

extern "C" ENCODER_API void createEncoder(HINSTANCE hInstance, HWND hWnd, HMENU hParentMenu);
extern "C" ENCODER_API void deleteEncoder();
extern "C" ENCODER_API LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
extern "C" ENCODER_API void waitForFullInitialization();
extern "C" ENCODER_API LPCTSTR getLogFileName();
extern "C" ENCODER_API int getNumberOfTuners();
extern "C" ENCODER_API LPCTSTR getTunerFriendlyName(int i);
extern "C" ENCODER_API int getTunerOrdinal(int i);
extern "C" ENCODER_API bool startRecording(bool autodiscoverTransponder,
										   ULONG frequency,
										   ULONG symbolRate,
										   Polarisation polarization,
										   ModulationType modulation,
										   BinaryConvolutionCodeRate fecRate,
										   int tunerOrdinal,
										   int channel,
										   bool useSid,
										   __int64 duration,
										   LPCWSTR outFileName,
										   __int64 size);