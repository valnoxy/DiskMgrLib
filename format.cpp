#include "pch.h"
#include "logging.h"

#include <cwctype>
#include <windows.h>
#include <winternl.h>
#include <initguid.h>
#include <iostream>
#include <vds.h>
#include <stdio.h>

#pragma comment(lib, "ole32.lib")

#define SafeRelease(x) { if (NULL != x) { x->Release(); x = NULL; } }
#define SafeCoFree(x) { if (NULL != x) { CoTaskMemFree(x); x = NULL; } }
#define ObjectNameInformation (OBJECT_INFORMATION_CLASS)1

extern "C" __declspec(dllexport) HRESULT DiskMgr_FormatPartition(
    wchar_t driveLetter,
    const wchar_t* fileSystem,
    const wchar_t* label,
    bool quickFormat)
{
    if (!fileSystem || wcslen(fileSystem) == 0)
    {
        DISKMGR_LOG_ERROR(L"Invalid file system");
	    return E_INVALIDARG;
    }

    driveLetter = std::towupper(driveLetter);
    if (driveLetter < L'A' || driveLetter > L'Z')
    {
        DISKMGR_LOG_ERROR(L"Invalid device letter: " << driveLetter);
        return E_INVALIDARG;
    }
    DISKMGR_LOG_DEBUG(L"Begin DiskMgr_FormatPartition for: " << driveLetter);

    HRESULT hResult;
    HRESULT asyncRes;
    ULONG ulFetchCount;

    // VDS Interfaces
    IVdsServiceLoader* pLoader = NULL;
    IVdsService* pService = NULL;
    IVdsSwProvider* pProvider = NULL;
    IVdsPack* pPack = NULL;
    IVdsVolume* pVolume = NULL;
    IVdsVolumeMF* pVolumeMF = NULL;
    IVdsVolumeMF2* pVolumeMF2 = NULL;
    IVdsAsync* pAsync = NULL;
    IUnknown* pUnknown = NULL;
    IEnumVdsObject* pEnumVdsSwProviders = NULL;
    IEnumVdsObject* pEnumVdsPacks = NULL;
    IEnumVdsObject* pEnumVolumes = NULL;

    VDS_ASYNC_OUTPUT asyncOut;
    wchar_t** accessPaths = NULL;
    wchar_t targetDrivePath[4];
    bool formatComplete = false;

    swprintf_s(targetDrivePath, L"%c:\\", driveLetter);

    // init
    hResult = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (hResult == RPC_E_CHANGED_MODE)
    {
        hResult = S_OK;
    }
    else if (FAILED(hResult))
    {
        DISKMGR_LOG_ERROR(L"Failed CoInitializeEx: " << hResult);
        return hResult;
    }

    // IVdsServiceLoader
    hResult = CoCreateInstance(CLSID_VdsLoader, NULL, CLSCTX_LOCAL_SERVER,
        IID_IVdsServiceLoader, (void**)&pLoader);
    if (FAILED(hResult))
    {
        DISKMGR_LOG_ERROR(L"Failed CoCreateInstance: " << hResult);
        goto cleanup;
    }

    hResult = pLoader->LoadService(NULL, &pService);
    SafeRelease(pLoader);
    if (FAILED(hResult))
    {
        DISKMGR_LOG_ERROR(L"Failed LoadService: " << hResult);
        goto cleanup;
    }

    hResult = pService->WaitForServiceReady();
    if (FAILED(hResult))
    {
        DISKMGR_LOG_ERROR(L"Failed WaitForServiceReady on pService: " << hResult);
	    goto cleanup;
    }

    // Update vds
    hResult = pService->Reenumerate();
    if (FAILED(hResult))
    {
        DISKMGR_LOG_ERROR(L"Failed ReEnumerate: " << hResult);
	    goto cleanup;
    }

    hResult = pService->Refresh();
    if (FAILED(hResult))
    {
        DISKMGR_LOG_ERROR(L"Failed Refresh on pService: " << hResult);
    	goto cleanup;
    }

    // request
    hResult = pService->QueryProviders(VDS_QUERY_SOFTWARE_PROVIDERS, &pEnumVdsSwProviders);
    if (FAILED(hResult))
    {
        DISKMGR_LOG_ERROR(L"Failed QueryProviders: " << hResult);
	    goto cleanup;
    }

    while ((hResult = pEnumVdsSwProviders->Next(1, &pUnknown, &ulFetchCount)) == S_OK)
    {
        hResult = pUnknown->QueryInterface(&pProvider);
        SafeRelease(pUnknown);
        if (FAILED(hResult))
        {
            DISKMGR_LOG_ERROR(L"Failed QueryInterface -> &pProvider: " << hResult);
            continue;
        }

        hResult = pProvider->QueryPacks(&pEnumVdsPacks);
        SafeRelease(pProvider);
        if (FAILED(hResult))
        {
            DISKMGR_LOG_ERROR(L"Failed QueryPacks -> &pEnumVdsPacks: " << hResult);
            continue;
        }

        while ((hResult = pEnumVdsPacks->Next(1, &pUnknown, &ulFetchCount)) == S_OK)
        {
            hResult = pUnknown->QueryInterface(&pPack);
            SafeRelease(pUnknown);
            if (FAILED(hResult))
            {
                DISKMGR_LOG_ERROR(L"Failed QueryInterface -> &pPack: " << hResult);
                continue;
            }

            hResult = pPack->QueryVolumes(&pEnumVolumes);
            SafeRelease(pPack);
            if (FAILED(hResult))
            {
                DISKMGR_LOG_ERROR(L"Failed QueryVolumes -> &pEnumVolumes: " << hResult);
                continue;
            }

            while ((hResult = pEnumVolumes->Next(1, &pUnknown, &ulFetchCount)) == S_OK)
            {
                hResult = pUnknown->QueryInterface(&pVolume);
                SafeRelease(pUnknown);
                if (FAILED(hResult))
                {
                    DISKMGR_LOG_ERROR(L"Failed QueryInterface -> &pVolume: " << hResult);
                    continue;
                }

                hResult = pVolume->QueryInterface(&pVolumeMF);
                SafeRelease(pVolume);
                if (FAILED(hResult))
                {
                    DISKMGR_LOG_ERROR(L"Failed QueryInterface -> &pVolumeMF: " << hResult);
                    continue;
                }

                hResult = pVolumeMF->QueryAccessPaths(&accessPaths, (PLONG)&ulFetchCount);
                if (FAILED(hResult))
                {
                    DISKMGR_LOG_ERROR(L"Failed QueryAccessPaths: " << hResult);
                    SafeRelease(pVolumeMF);
                    continue;
                }

                bool foundVolume = false;
                for (ULONG i = 0; i < ulFetchCount; i++)
                {
                    if (accessPaths[i] && wcslen(accessPaths[i]) >= 2)
                    {
                        wchar_t volLetter = towupper(accessPaths[i][0]);
                        if (volLetter == driveLetter)
                        {
                            foundVolume = true;
                            break;
                        }
                    }
                }

                SafeCoFree(accessPaths);
                accessPaths = NULL;

                if (!foundVolume)
                {
                    //DISKMGR_LOG_ERROR(L"Volume not found!");
                    SafeRelease(pVolumeMF);
                    continue;
                }

                hResult = pVolumeMF->QueryInterface(&pVolumeMF2);
                SafeRelease(pVolumeMF);
                if (FAILED(hResult))
                {
                    DISKMGR_LOG_ERROR(L"Failed QueryInterface -> &pVolumeMF2: " << hResult);
	                goto cleanup;
                }

                // prepare parameters
                DISKMGR_LOG_DEBUG(L"Preparing format parameters");
                wchar_t fsNameBuffer[32];
                wchar_t fsLabelBuffer[32];
                wcscpy_s(fsNameBuffer, fileSystem);
                wcscpy_s(fsLabelBuffer, label ? label : L"");

                DISKMGR_LOG_DEBUG(L"Formatting partition ...");
                hResult = pVolumeMF2->FormatEx(
                    fsNameBuffer,                // File system
                    0,                           // Revision
                    0,                           // Cluster size
                    fsLabelBuffer,               // Volume label
                    TRUE,                        // Force
                    quickFormat ? TRUE : FALSE,  // Quick Format
                    FALSE,                       // Enable Compression
                    &pAsync                      // Async 
                );

                if (FAILED(hResult))
                {
                    DISKMGR_LOG_ERROR(L"Format failed: "<< hResult);
                    SafeRelease(pVolumeMF2);
                    goto cleanup;
                }

                // wait for completion
                asyncRes = pAsync->Wait(&hResult, &asyncOut);
                SafeRelease(pAsync);
                SafeRelease(pVolumeMF2);

                if (FAILED(asyncRes))
                {
                    DISKMGR_LOG_ERROR(L"Format failed (asyncRes): " << hResult);
                    hResult = asyncRes;
                    goto cleanup;
                }

                if (FAILED(hResult))
                {
                    DISKMGR_LOG_ERROR(L"Format failed (after async): " << hResult);
					goto cleanup;
                }

                formatComplete = true;
                DISKMGR_LOG_INFO(L"Format completed! -> Result: " << hResult);
                goto cleanup;
            }

            SafeRelease(pEnumVolumes);
            pEnumVolumes = NULL;
        }

        SafeRelease(pEnumVdsPacks);
        pEnumVdsPacks = NULL;
    }

    hResult = VDS_E_OBJECT_NOT_FOUND;

cleanup:
    SafeCoFree(accessPaths);
    SafeRelease(pEnumVolumes);
    SafeRelease(pEnumVdsPacks);
    SafeRelease(pEnumVdsSwProviders);
    SafeRelease(pAsync);
    SafeRelease(pVolumeMF2);
    SafeRelease(pVolumeMF);
    SafeRelease(pVolume);
    SafeRelease(pPack);
    SafeRelease(pProvider);
    SafeRelease(pService);
    SafeRelease(pLoader);
    SafeRelease(pUnknown);

    CoUninitialize();

    if (formatComplete)
        return S_OK;

    return hResult;
}