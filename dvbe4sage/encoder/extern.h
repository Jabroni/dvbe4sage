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