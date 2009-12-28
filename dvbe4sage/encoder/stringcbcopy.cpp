#include "stdafx.h"

#define STRSAFE_NO_CB_FUNCTIONS
#include <strsafe.h>

#pragma warning(disable : 4995)

EXTERN_C HRESULT __stdcall
StringCbCopyA(
    __out_bcount(cbDest) STRSAFE_LPSTR pszDest,
    __in size_t cbDest,
    __in STRSAFE_LPCSTR pszSrc)
{
    HRESULT hr;
    size_t cchDest = cbDest / sizeof(char);

    hr = StringValidateDestA(pszDest, cchDest, STRSAFE_MAX_CCH);

    if (SUCCEEDED(hr))
    {
        hr = StringCopyWorkerA(pszDest,
                               cchDest,
                               NULL,
                               pszSrc,
                               STRSAFE_MAX_LENGTH);
    }

    return hr;
}

EXTERN_C HRESULT __stdcall
StringCbCopyW(
    __out_bcount(cbDest) STRSAFE_LPWSTR pszDest,
    __in size_t cbDest,
    __in STRSAFE_LPCWSTR pszSrc)
{
    HRESULT hr;
    size_t cchDest = cbDest / sizeof(wchar_t);

    hr = StringValidateDestW(pszDest, cchDest, STRSAFE_MAX_CCH);

    if (SUCCEEDED(hr))
    {
        hr = StringCopyWorkerW(pszDest,
                               cchDest,
                               NULL,
                               pszSrc,
                               STRSAFE_MAX_LENGTH);
    }

    return hr;
}
