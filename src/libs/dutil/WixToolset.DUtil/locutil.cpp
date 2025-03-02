// Copyright (c) .NET Foundation and contributors. All rights reserved. Licensed under the Microsoft Reciprocal License. See LICENSE.TXT file in the project root for full license information.

#include "precomp.h"


// Exit macros
#define LocExitOnLastError(x, s, ...) ExitOnLastErrorSource(DUTIL_SOURCE_LOCUTIL, x, s, __VA_ARGS__)
#define LocExitOnLastErrorDebugTrace(x, s, ...) ExitOnLastErrorDebugTraceSource(DUTIL_SOURCE_LOCUTIL, x, s, __VA_ARGS__)
#define LocExitWithLastError(x, s, ...) ExitWithLastErrorSource(DUTIL_SOURCE_LOCUTIL, x, s, __VA_ARGS__)
#define LocExitOnFailure(x, s, ...) ExitOnFailureSource(DUTIL_SOURCE_LOCUTIL, x, s, __VA_ARGS__)
#define LocExitOnRootFailure(x, s, ...) ExitOnRootFailureSource(DUTIL_SOURCE_LOCUTIL, x, s, __VA_ARGS__)
#define LocExitOnFailureDebugTrace(x, s, ...) ExitOnFailureDebugTraceSource(DUTIL_SOURCE_LOCUTIL, x, s, __VA_ARGS__)
#define LocExitOnNull(p, x, e, s, ...) ExitOnNullSource(DUTIL_SOURCE_LOCUTIL, p, x, e, s, __VA_ARGS__)
#define LocExitOnNullWithLastError(p, x, s, ...) ExitOnNullWithLastErrorSource(DUTIL_SOURCE_LOCUTIL, p, x, s, __VA_ARGS__)
#define LocExitOnNullDebugTrace(p, x, e, s, ...)  ExitOnNullDebugTraceSource(DUTIL_SOURCE_LOCUTIL, p, x, e, s, __VA_ARGS__)
#define LocExitOnInvalidHandleWithLastError(p, x, s, ...) ExitOnInvalidHandleWithLastErrorSource(DUTIL_SOURCE_LOCUTIL, p, x, s, __VA_ARGS__)
#define LocExitOnWin32Error(e, x, s, ...) ExitOnWin32ErrorSource(DUTIL_SOURCE_LOCUTIL, e, x, s, __VA_ARGS__)
#define LocExitOnGdipFailure(g, x, s, ...) ExitOnGdipFailureSource(DUTIL_SOURCE_LOCUTIL, g, x, s, __VA_ARGS__)

// prototypes
static HRESULT ParseWxl(
    __in IXMLDOMDocument* pixd,
    __out WIX_LOCALIZATION** ppWixLoc
    );
static HRESULT ParseWxlStrings(
    __in IXMLDOMElement* pElement,
    __in WIX_LOCALIZATION* pWixLoc
    );
static HRESULT ParseWxlControls(
    __in IXMLDOMElement* pElement,
    __in WIX_LOCALIZATION* pWixLoc
    );
static HRESULT ParseWxlString(
    __in IXMLDOMNode* pixn,
    __in DWORD dwIdx,
    __in WIX_LOCALIZATION* pWixLoc
    );
static HRESULT ParseWxlControl(
    __in IXMLDOMNode* pixn,
    __in DWORD dwIdx,
    __in WIX_LOCALIZATION* pWixLoc
    );

// from Winnls.h
#ifndef MUI_LANGUAGE_ID
#define MUI_LANGUAGE_ID                     0x4      // Use traditional language ID convention
#endif
#ifndef MUI_MERGE_USER_FALLBACK
#define MUI_MERGE_USER_FALLBACK             0x20     // GetThreadPreferredUILanguages merges in user preferred languages
#endif
#ifndef MUI_MERGE_SYSTEM_FALLBACK
#define MUI_MERGE_SYSTEM_FALLBACK           0x10     // GetThreadPreferredUILanguages merges in parent and base languages
#endif
typedef WINBASEAPI BOOL (WINAPI *GET_THREAD_PREFERRED_UI_LANGUAGES) (
    __in DWORD dwFlags,
    __out PULONG pulNumLanguages,
    __out_ecount_opt(*pcchLanguagesBuffer) PZZWSTR pwszLanguagesBuffer,
    __inout PULONG pcchLanguagesBuffer
);

extern "C" HRESULT DAPI LocProbeForFile(
    __in_z LPCWSTR wzBasePath,
    __in_z LPCWSTR wzLocFileName,
    __in_z_opt LPCWSTR wzLanguage,
    __inout LPWSTR* psczPath
    )
{
    HRESULT hr = S_OK;
    LPWSTR sczProbePath = NULL;
    LANGID langid = 0;
    LPWSTR sczLangIdFile = NULL;
    LPWSTR sczLangsBuff = NULL;
    GET_THREAD_PREFERRED_UI_LANGUAGES pvfnGetThreadPreferredUILanguages =
        reinterpret_cast<GET_THREAD_PREFERRED_UI_LANGUAGES>(
            GetProcAddress(GetModuleHandle("Kernel32.dll"), "GetThreadPreferredUILanguages"));

    // If a language was specified, look for a loc file in that as a directory.
    if (wzLanguage && *wzLanguage)
    {
        hr = PathConcat(wzBasePath, wzLanguage, &sczProbePath);
        LocExitOnFailure(hr, "Failed to concat base path to language.");

        hr = PathConcat(sczProbePath, wzLocFileName, &sczProbePath);
        LocExitOnFailure(hr, "Failed to concat loc file name to probe path.");

        if (FileExistsEx(sczProbePath, NULL))
        {
            ExitFunction();
        }
    }

    if (pvfnGetThreadPreferredUILanguages)
    {
        ULONG nLangs;
        ULONG cchLangs = 0;
        DWORD dwFlags = MUI_LANGUAGE_ID | MUI_MERGE_USER_FALLBACK | MUI_MERGE_SYSTEM_FALLBACK;
        if (!(*pvfnGetThreadPreferredUILanguages)(dwFlags, &nLangs, NULL, &cchLangs))
        {
            LocExitWithLastError(hr, "GetThreadPreferredUILanguages failed to return buffer size.");
        }

        hr = StrAlloc(&sczLangsBuff, cchLangs);
        LocExitOnFailure(hr, "Failed to allocate buffer for languages");

        nLangs = 0;
        if (!(*pvfnGetThreadPreferredUILanguages)(dwFlags, &nLangs, sczLangsBuff, &cchLangs))
        {
            LocExitWithLastError(hr, "GetThreadPreferredUILanguages failed to return language list.");
        }

        LPWSTR szLangs = sczLangsBuff;
        for (ULONG i = 0; i < nLangs; ++i, szLangs += 5)
        {
            // StrHexDecode assumes low byte is first. We'll need to swap the bytes once we parse out the value.
            hr = StrHexDecode(szLangs, reinterpret_cast<BYTE*>(&langid), sizeof(langid));
            LocExitOnFailure(hr, "Failed to parse langId.");

            langid = MAKEWORD(HIBYTE(langid), LOBYTE(langid));
            hr = StrAllocFormatted(&sczLangIdFile, L"%u\\%ls", langid, wzLocFileName); 
            LocExitOnFailure(hr, "Failed to format user preferred langid.");

            hr = PathConcat(wzBasePath, sczLangIdFile, &sczProbePath); 
            LocExitOnFailure(hr, "Failed to concat user preferred langid file name to base path.");

            if (FileExistsEx(sczProbePath, NULL)) 
            { 
                ExitFunction(); 
            }
        }
    }

    langid = ::GetUserDefaultUILanguage();

    hr = StrAllocFormatted(&sczLangIdFile, L"%u\\%ls", langid, wzLocFileName);
    LocExitOnFailure(hr, "Failed to format user langid.");

    hr = PathConcat(wzBasePath, sczLangIdFile, &sczProbePath);
    LocExitOnFailure(hr, "Failed to concat user langid file name to base path.");

    if (FileExistsEx(sczProbePath, NULL))
    {
        ExitFunction();
    }

    if (MAKELANGID(langid & 0x3FF, SUBLANG_DEFAULT) != langid) 
    { 
        langid = MAKELANGID(langid & 0x3FF, SUBLANG_DEFAULT); 
        
        hr = StrAllocFormatted(&sczLangIdFile, L"%u\\%ls", langid, wzLocFileName); 
        LocExitOnFailure(hr, "Failed to format user langid (default sublang).");

        hr = PathConcat(wzBasePath, sczLangIdFile, &sczProbePath); 
        LocExitOnFailure(hr, "Failed to concat user langid file name to base path (default sublang).");

        if (FileExistsEx(sczProbePath, NULL)) 
        { 
            ExitFunction(); 
        } 
    }

    langid = ::GetSystemDefaultUILanguage();

    hr = StrAllocFormatted(&sczLangIdFile, L"%u\\%ls", langid, wzLocFileName);
    LocExitOnFailure(hr, "Failed to format system langid.");

    hr = PathConcat(wzBasePath, sczLangIdFile, &sczProbePath);
    LocExitOnFailure(hr, "Failed to concat system langid file name to base path.");

    if (FileExistsEx(sczProbePath, NULL))
    {
        ExitFunction();
    }

    if (MAKELANGID(langid & 0x3FF, SUBLANG_DEFAULT) != langid) 
    { 
        langid = MAKELANGID(langid & 0x3FF, SUBLANG_DEFAULT); 
        
        hr = StrAllocFormatted(&sczLangIdFile, L"%u\\%ls", langid, wzLocFileName); 
        LocExitOnFailure(hr, "Failed to format user langid (default sublang).");

        hr = PathConcat(wzBasePath, sczLangIdFile, &sczProbePath); 
        LocExitOnFailure(hr, "Failed to concat user langid file name to base path (default sublang).");

        if (FileExistsEx(sczProbePath, NULL)) 
        { 
            ExitFunction(); 
        } 
    }

    // Finally, look for the loc file in the base path.
    hr = PathConcat(wzBasePath, wzLocFileName, &sczProbePath);
    LocExitOnFailure(hr, "Failed to concat loc file name to base path.");

    if (!FileExistsEx(sczProbePath, NULL))
    {
        hr = E_FILENOTFOUND;
    }

LExit:
    if (SUCCEEDED(hr))
    {
        hr = StrAllocString(psczPath, sczProbePath, 0);
    }

    ReleaseStr(sczLangIdFile);
    ReleaseStr(sczProbePath);
    ReleaseStr(sczLangsBuff);

    return hr;
}

extern "C" HRESULT DAPI LocLoadFromFile(
    __in_z LPCWSTR wzWxlFile,
    __out WIX_LOCALIZATION** ppWixLoc
    )
{
    HRESULT hr = S_OK;
    IXMLDOMDocument* pixd = NULL;

    hr = XmlLoadDocumentFromFile(wzWxlFile, &pixd);
    LocExitOnFailure(hr, "Failed to load WXL file as XML document.");

    hr = ParseWxl(pixd, ppWixLoc);
    LocExitOnFailure(hr, "Failed to parse WXL.");

LExit:
    ReleaseObject(pixd);

    return hr;
}

extern "C" HRESULT DAPI LocLoadFromResource(
    __in HMODULE hModule,
    __in_z LPCSTR szResource,
    __out WIX_LOCALIZATION** ppWixLoc
    )
{
    HRESULT hr = S_OK;
    LPVOID pvResource = NULL;
    DWORD cbResource = 0;
    LPWSTR sczXml = NULL;
    IXMLDOMDocument* pixd = NULL;

    hr = ResReadData(hModule, szResource, &pvResource, &cbResource);
    LocExitOnFailure(hr, "Failed to read theme from resource.");

    hr = StrAllocStringAnsi(&sczXml, reinterpret_cast<LPCSTR>(pvResource), cbResource, CP_UTF8);
    LocExitOnFailure(hr, "Failed to convert XML document data from UTF-8 to unicode string.");

    hr = XmlLoadDocument(sczXml, &pixd);
    LocExitOnFailure(hr, "Failed to load theme resource as XML document.");

    hr = ParseWxl(pixd, ppWixLoc);
    LocExitOnFailure(hr, "Failed to parse WXL.");

LExit:
    ReleaseObject(pixd);
    ReleaseStr(sczXml);

    return hr;
}

extern "C" void DAPI LocFree(
    __in_opt WIX_LOCALIZATION* pWixLoc
    )
{
    if (pWixLoc)
    {
        for (DWORD idx = 0; idx < pWixLoc->cLocStrings; ++idx)
        {
            ReleaseStr(pWixLoc->rgLocStrings[idx].wzId);
            ReleaseStr(pWixLoc->rgLocStrings[idx].wzText);
        }

        for (DWORD idx = 0; idx < pWixLoc->cLocControls; ++idx)
        {
            ReleaseStr(pWixLoc->rgLocControls[idx].wzControl);
            ReleaseStr(pWixLoc->rgLocControls[idx].wzText);
        }

        ReleaseMem(pWixLoc->rgLocStrings);
        ReleaseMem(pWixLoc->rgLocControls);
        ReleaseMem(pWixLoc);
    }
}

extern "C" HRESULT DAPI LocLocalizeString(
    __in const WIX_LOCALIZATION* pWixLoc,
    __inout LPWSTR* ppsczInput
    )
{
    Assert(ppsczInput && pWixLoc);
    HRESULT hr = S_OK;

    for (DWORD i = 0; i < pWixLoc->cLocStrings; ++i)
    {
        hr = StrReplaceStringAll(ppsczInput, pWixLoc->rgLocStrings[i].wzId, pWixLoc->rgLocStrings[i].wzText);
        LocExitOnFailure(hr, "Localizing string failed.");
    }

LExit:
    return hr;
}

extern "C" HRESULT DAPI LocGetControl(
    __in const WIX_LOCALIZATION* pWixLoc,
    __in_z LPCWSTR wzId,
    __out LOC_CONTROL** ppLocControl
    )
{
    HRESULT hr = S_OK;
    LOC_CONTROL* pLocControl = NULL;

    for (DWORD i = 0; i < pWixLoc->cLocControls; ++i)
    {
        pLocControl = &pWixLoc->rgLocControls[i];

        if (CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, 0, pLocControl->wzControl, -1, wzId, -1))
        {
            *ppLocControl = pLocControl;
            ExitFunction1(hr = S_OK);
        }
    }

    hr = E_NOTFOUND;

LExit:
    return hr;
}

extern "C" HRESULT DAPI LocGetString(
    __in const WIX_LOCALIZATION* pWixLoc,
    __in_z LPCWSTR wzId,
    __out LOC_STRING** ppLocString
    )
{
    HRESULT hr = E_NOTFOUND;
    LOC_STRING* pLocString = NULL;

    for (DWORD i = 0; i < pWixLoc->cLocStrings; ++i)
    {
        pLocString = pWixLoc->rgLocStrings + i;

        if (CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, 0, pLocString->wzId, -1, wzId, -1))
        {
            *ppLocString = pLocString;
            hr = S_OK;
            break;
        }
    }

    return hr;
}

extern "C" HRESULT DAPI LocAddString(
    __in WIX_LOCALIZATION* pWixLoc,
    __in_z LPCWSTR wzId,
    __in_z LPCWSTR wzLocString,
    __in BOOL bOverridable
    )
{
    HRESULT hr = S_OK;

    ++pWixLoc->cLocStrings;
    pWixLoc->rgLocStrings = static_cast<LOC_STRING*>(MemReAlloc(pWixLoc->rgLocStrings, sizeof(LOC_STRING) * pWixLoc->cLocStrings, TRUE));
    LocExitOnNull(pWixLoc->rgLocStrings, hr, E_OUTOFMEMORY, "Failed to reallocate memory for localization strings.");

    LOC_STRING* pLocString = pWixLoc->rgLocStrings + (pWixLoc->cLocStrings - 1);

    hr = StrAllocFormatted(&pLocString->wzId, L"#(loc.%s)", wzId);
    LocExitOnFailure(hr, "Failed to set localization string Id.");

    hr = StrAllocString(&pLocString->wzText, wzLocString, 0);
    LocExitOnFailure(hr, "Failed to set localization string Text.");

    pLocString->bOverridable = bOverridable;

LExit:
    return hr;
}

// helper functions

static HRESULT ParseWxl(
    __in IXMLDOMDocument* pixd,
    __out WIX_LOCALIZATION** ppWixLoc
    )
{
    HRESULT hr = S_OK;
    IXMLDOMElement *pWxlElement = NULL;
    WIX_LOCALIZATION* pWixLoc = NULL;

    pWixLoc = static_cast<WIX_LOCALIZATION*>(MemAlloc(sizeof(WIX_LOCALIZATION), TRUE));
    LocExitOnNull(pWixLoc, hr, E_OUTOFMEMORY, "Failed to allocate memory for Wxl file.");

    // read the WixLocalization tag
    hr = pixd->get_documentElement(&pWxlElement);
    LocExitOnFailure(hr, "Failed to get localization element.");

    // get the Language attribute if present
    pWixLoc->dwLangId = WIX_LOCALIZATION_LANGUAGE_NOT_SET;
    hr = XmlGetAttributeNumber(pWxlElement, L"Language", &pWixLoc->dwLangId);
    if (S_FALSE == hr)
    {
        hr = S_OK;
    }
    LocExitOnFailure(hr, "Failed to get Language value.");

    // store the strings and controls in a node list
    hr = ParseWxlStrings(pWxlElement, pWixLoc);
    LocExitOnFailure(hr, "Parsing localization strings failed.");

    hr = ParseWxlControls(pWxlElement, pWixLoc);
    LocExitOnFailure(hr, "Parsing localization controls failed.");

    *ppWixLoc = pWixLoc;
    pWixLoc = NULL;

LExit:
    ReleaseObject(pWxlElement);
    ReleaseMem(pWixLoc);

    return hr;
}


static HRESULT ParseWxlStrings(
    __in IXMLDOMElement* pElement,
    __in WIX_LOCALIZATION* pWixLoc
    )
{
    HRESULT hr = S_OK;
    IXMLDOMNode* pixn = NULL;
    IXMLDOMNodeList* pixnl = NULL;
    DWORD dwIdx = 0;

    hr = XmlSelectNodes(pElement, L"String", &pixnl);
    LocExitOnLastError(hr, "Failed to get String child nodes of Wxl File.");

    hr = pixnl->get_length(reinterpret_cast<long*>(&pWixLoc->cLocStrings));
    LocExitOnLastError(hr, "Failed to get number of String child nodes in Wxl File.");

    if (0 < pWixLoc->cLocStrings)
    {
        pWixLoc->rgLocStrings = static_cast<LOC_STRING*>(MemAlloc(sizeof(LOC_STRING) * pWixLoc->cLocStrings, TRUE));
        LocExitOnNull(pWixLoc->rgLocStrings, hr, E_OUTOFMEMORY, "Failed to allocate memory for localization strings.");

        while (S_OK == (hr = XmlNextElement(pixnl, &pixn, NULL)))
        {
            hr = ParseWxlString(pixn, dwIdx, pWixLoc);
            LocExitOnFailure(hr, "Failed to parse localization string.");

            ++dwIdx;
            ReleaseNullObject(pixn);
        }

        hr = S_OK;
        LocExitOnFailure(hr, "Failed to enumerate all localization strings.");
    }

LExit:
    if (FAILED(hr) && pWixLoc->rgLocStrings)
    {
        for (DWORD idx = 0; idx < pWixLoc->cLocStrings; ++idx)
        {
            ReleaseStr(pWixLoc->rgLocStrings[idx].wzId);
            ReleaseStr(pWixLoc->rgLocStrings[idx].wzText);
        }

        ReleaseMem(pWixLoc->rgLocStrings);
    }

    ReleaseObject(pixn);
    ReleaseObject(pixnl);

    return hr;
}

static HRESULT ParseWxlControls(
    __in IXMLDOMElement* pElement,
    __in WIX_LOCALIZATION* pWixLoc
    )
{
    HRESULT hr = S_OK;
    IXMLDOMNode* pixn = NULL;
    IXMLDOMNodeList* pixnl = NULL;
    DWORD dwIdx = 0;

    hr = XmlSelectNodes(pElement, L"UI|Control", &pixnl);
    LocExitOnLastError(hr, "Failed to get UI child nodes of Wxl File.");

    hr = pixnl->get_length(reinterpret_cast<long*>(&pWixLoc->cLocControls));
    LocExitOnLastError(hr, "Failed to get number of UI child nodes in Wxl File.");

    if (0 < pWixLoc->cLocControls)
    {
        pWixLoc->rgLocControls = static_cast<LOC_CONTROL*>(MemAlloc(sizeof(LOC_CONTROL) * pWixLoc->cLocControls, TRUE));
        LocExitOnNull(pWixLoc->rgLocControls, hr, E_OUTOFMEMORY, "Failed to allocate memory for localized controls.");

        while (S_OK == (hr = XmlNextElement(pixnl, &pixn, NULL)))
        {
            hr = ParseWxlControl(pixn, dwIdx, pWixLoc);
            LocExitOnFailure(hr, "Failed to parse localized control.");

            ++dwIdx;
            ReleaseNullObject(pixn);
        }

        hr = S_OK;
        LocExitOnFailure(hr, "Failed to enumerate all localized controls.");
    }

LExit:
    if (FAILED(hr) && pWixLoc->rgLocControls)
    {
        for (DWORD idx = 0; idx < pWixLoc->cLocControls; ++idx)
        {
            ReleaseStr(pWixLoc->rgLocControls[idx].wzControl);
            ReleaseStr(pWixLoc->rgLocControls[idx].wzText);
        }

        ReleaseMem(pWixLoc->rgLocControls);
    }

    ReleaseObject(pixn);
    ReleaseObject(pixnl);

    return hr;
}

static HRESULT ParseWxlString(
    __in IXMLDOMNode* pixn,
    __in DWORD dwIdx,
    __in WIX_LOCALIZATION* pWixLoc
    )
{
    HRESULT hr = S_OK;
    LOC_STRING* pLocString = NULL;
    BSTR bstrText = NULL;

    pLocString = pWixLoc->rgLocStrings + dwIdx;

    // Id
    hr = XmlGetAttribute(pixn, L"Id", &bstrText);
    LocExitOnFailure(hr, "Failed to get Xml attribute Id in Wxl file.");

    hr = StrAllocFormatted(&pLocString->wzId, L"#(loc.%s)", bstrText);
    LocExitOnFailure(hr, "Failed to duplicate Xml attribute Id in Wxl file.");

    ReleaseNullBSTR(bstrText);

    // Overrideable
    hr = XmlGetAttribute(pixn, L"Overridable", &bstrText);
    LocExitOnFailure(hr, "Failed to get Xml attribute Overridable.");

    if (S_OK == hr)
    {
        pLocString->bOverridable = CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, 0, bstrText, -1, L"yes", -1);
    }

    ReleaseNullBSTR(bstrText);

    // Text
    hr = XmlGetAttribute(pixn, L"Value", &bstrText);
    LocExitOnFailure(hr, "Failed to get Xml attribute Value in Wxl file.");

    hr = StrAllocString(&pLocString->wzText, bstrText ? bstrText : L"", 0);
    LocExitOnFailure(hr, "Failed to duplicate Xml text in Wxl file.");

LExit:
    ReleaseBSTR(bstrText);

    return hr;
}

static HRESULT ParseWxlControl(
    __in IXMLDOMNode* pixn,
    __in DWORD dwIdx,
    __in WIX_LOCALIZATION* pWixLoc
    )
{
    HRESULT hr = S_OK;
    LOC_CONTROL* pLocControl = NULL;
    BSTR bstrText = NULL;

    pLocControl = pWixLoc->rgLocControls + dwIdx;

    // Id
    hr = XmlGetAttribute(pixn, L"Control", &bstrText);
    LocExitOnFailure(hr, "Failed to get Xml attribute Control in Wxl file.");

    hr = StrAllocString(&pLocControl->wzControl, bstrText, 0);
    LocExitOnFailure(hr, "Failed to duplicate Xml attribute Control in Wxl file.");

    ReleaseNullBSTR(bstrText);

    // X
    pLocControl->nX = LOC_CONTROL_NOT_SET;
    hr = XmlGetAttributeNumber(pixn, L"X", reinterpret_cast<DWORD*>(&pLocControl->nX));
    LocExitOnFailure(hr, "Failed to get control X attribute.");

    // Y
    pLocControl->nY = LOC_CONTROL_NOT_SET;
    hr = XmlGetAttributeNumber(pixn, L"Y", reinterpret_cast<DWORD*>(&pLocControl->nY));
    LocExitOnFailure(hr, "Failed to get control Y attribute.");

    // Width
    pLocControl->nWidth = LOC_CONTROL_NOT_SET;
    hr = XmlGetAttributeNumber(pixn, L"Width", reinterpret_cast<DWORD*>(&pLocControl->nWidth));
    LocExitOnFailure(hr, "Failed to get control width attribute.");

    // Height
    pLocControl->nHeight = LOC_CONTROL_NOT_SET;
    hr = XmlGetAttributeNumber(pixn, L"Height", reinterpret_cast<DWORD*>(&pLocControl->nHeight));
    LocExitOnFailure(hr, "Failed to get control height attribute.");

    // Text
    hr = XmlGetAttribute(pixn, L"Text", &bstrText);
    LocExitOnFailure(hr, "Failed to get Xml attribute Text in Wxl file.");

    hr = StrAllocString(&pLocControl->wzText, bstrText ? bstrText : L"", 0);
    LocExitOnFailure(hr, "Failed to duplicate control text in Wxl file.");

LExit:
    ReleaseBSTR(bstrText);

    return hr;
}
