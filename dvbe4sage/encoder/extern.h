#pragma once

#ifdef ENCODER_EXPORTS
#define ENCODER_API __declspec(dllexport)
#else
#define ENCODER_API __declspec(dllimport)
#endif

#define MAX_ERROR_MESSAGE_SIZE	2048

// Main control points
extern "C" ENCODER_API void createEncoder(HINSTANCE hInstance, HWND hWnd, HMENU hParentMenu);
extern "C" ENCODER_API void deleteEncoder();
extern "C" ENCODER_API LRESULT encoderWindowProc(UINT message, WPARAM wParam, LPARAM lParam);
extern "C" ENCODER_API void waitForFullInitialization();

#ifdef NEED_ACCESS_FUNCTIONS
// External getters
extern "C" ENCODER_API LPCTSTR getLogFileName();
extern "C" ENCODER_API LPCTSTR getInstanceName();
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
										   __int64 size,
										   bool startFullTransponderDump);
extern "C" ENCODER_API bool startRecordingFromFile(LPCWSTR inFileName,
												   int sid,
												   __int64 duration,
												   LPCWSTR outFileName,
												   bool forEIT);
extern "C" ENCODER_API bool dumpECMCache(LPCTSTR fileName, LPTSTR reason);
extern "C" ENCODER_API bool loadECMCache(LPCTSTR fileName, LPTSTR reason);

extern "C" ENCODER_API bool dumpChannels(LPCTSTR fileName, LPTSTR reason);
extern "C" ENCODER_API bool dumpServices(LPCTSTR fileName, LPTSTR reason);
extern "C" ENCODER_API bool dumpTransponders(LPCTSTR fileName, LPTSTR reason);
extern "C" ENCODER_API bool logEPG(LPTSTR reason);

// DVB translation functions
extern "C" ENCODER_API BinaryConvolutionCodeRate getFECFromDescriptor(USHORT descriptor);
extern "C" ENCODER_API LPCTSTR printableFEC(BinaryConvolutionCodeRate fec);
extern "C" ENCODER_API BinaryConvolutionCodeRate getFECFromString(LPCTSTR str);
extern "C" ENCODER_API Polarisation getPolarizationFromDescriptor(USHORT descriptor);
extern "C" ENCODER_API LPCTSTR printablePolarization(Polarisation polarization);
extern "C" ENCODER_API Polarisation getPolarizationFromString(LPCTSTR str);
extern "C" ENCODER_API ModulationType getDVBSModulationFromDescriptor(BYTE modulationSystem, BYTE modulationType);
extern "C" ENCODER_API ModulationType getDVBCModulationFromDescriptor(BYTE modulation);
extern "C" ENCODER_API LPCTSTR printableModulation(ModulationType modulation);
extern "C" ENCODER_API ModulationType getModulationFromString(LPCTSTR str);
#endif	// NEED_ACCESS_FUNCTIONS