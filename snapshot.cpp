#include "pch.h"
#include "snapshot.h"

#include <vss.h>
#include <vswriter.h>
#include <vsbackup.h>
#include <combaseapi.h>
#include <iostream>

#pragma comment(lib, "VssApi.lib")
#pragma comment(lib, "ole32.lib")

#define SafeRelease(x) { if (NULL != x) { x->Release(); x = NULL; } }

extern "C" __declspec(dllexport) HRESULT DiskMgr_CreateVssSnapshot(
    const wchar_t* volumePath,
    wchar_t* outSnapshotId,
    int snapshotIdBufferSize,
    wchar_t* outDeviceObjectPath,
    int deviceObjectPathBufferSize)
{
    if (!volumePath || wcslen(volumePath) == 0)
    {
        std::wcout << L"[DiskMgr] Invalid volume path\n";
        return E_INVALIDARG;
    }

    HRESULT hResult;
    IVssBackupComponents* pBackup = NULL;
    IVssAsync* pAsync = NULL;
    VSS_ID snapshotSetId = GUID_NULL;
    VSS_ID snapshotId = GUID_NULL;
    VSS_SNAPSHOT_PROP snapshotProp;
    ZeroMemory(&snapshotProp, sizeof(snapshotProp));
    bool snapshotCreated = false;

    std::wcout << L"[DiskMgr] Begin DiskMgr_CreateVssSnapshot for: " << volumePath << "\n";

    hResult = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (hResult == RPC_E_CHANGED_MODE)
    {
        hResult = S_OK;
    }
    else if (FAILED(hResult))
    {
        std::wcout << L"[DiskMgr] Failed CoInitializeEx: " << hResult << "\n";
        return hResult;
    }

    hResult = CreateVssBackupComponents(&pBackup);
    if (FAILED(hResult))
    {
        std::wcout << L"[DiskMgr] Failed CreateVssBackupComponents: " << hResult << "\n";
        goto cleanup;
    }

    hResult = pBackup->InitializeForBackup();
    if (FAILED(hResult))
    {
        std::wcout << L"[DiskMgr] Failed InitializeForBackup: " << hResult << "\n";
        goto cleanup;
    }

    hResult = pBackup->SetContext(VSS_CTX_APP_ROLLBACK);
    if (FAILED(hResult))
    {
        std::wcout << L"[DiskMgr] Failed SetContext: " << hResult << "\n";
        goto cleanup;
    }

    hResult = pBackup->SetBackupState(false, false, VSS_BT_COPY, false);
    if (FAILED(hResult))
    {
        std::wcout << L"[DiskMgr] Failed SetBackupState: " << hResult << "\n";
        goto cleanup;
    }

    hResult = pBackup->GatherWriterMetadata(&pAsync);
    if (FAILED(hResult))
    {
        std::wcout << L"[DiskMgr] Failed GatherWriterMetadata: " << hResult << "\n";
        goto cleanup;
    }
    hResult = pAsync->Wait();
    SafeRelease(pAsync);
    if (FAILED(hResult))
    {
        std::wcout << L"[DiskMgr] Failed GatherWriterMetadata (Wait): " << hResult << "\n";
        goto cleanup;
    }

    hResult = pBackup->StartSnapshotSet(&snapshotSetId);
    if (FAILED(hResult))
    {
        std::wcout << L"[DiskMgr] Failed StartSnapshotSet: " << hResult << "\n";
        goto cleanup;
    }

    hResult = pBackup->AddToSnapshotSet((VSS_PWSZ)volumePath, GUID_NULL, &snapshotId);
    if (FAILED(hResult))
    {
        std::wcout << L"[DiskMgr] Failed AddToSnapshotSet: " << hResult << "\n";
        goto cleanup;
    }

    hResult = pBackup->PrepareForBackup(&pAsync);
    if (FAILED(hResult))
    {
        std::wcout << L"[DiskMgr] Failed PrepareForBackup: " << hResult << "\n";
        goto cleanup;
    }
    hResult = pAsync->Wait();
    SafeRelease(pAsync);
    if (FAILED(hResult))
    {
        std::wcout << L"[DiskMgr] Failed PrepareForBackup (Wait): " << hResult << "\n";
        goto cleanup;
    }

    hResult = pBackup->DoSnapshotSet(&pAsync);
    if (FAILED(hResult))
    {
        std::wcout << L"[DiskMgr] Failed DoSnapshotSet: " << hResult << "\n";
        goto cleanup;
    }
    hResult = pAsync->Wait();
    SafeRelease(pAsync);
    if (FAILED(hResult))
    {
        std::wcout << L"[DiskMgr] Failed DoSnapshotSet (Wait): " << hResult << "\n";
        goto cleanup;
    }

    hResult = pBackup->GetSnapshotProperties(snapshotId, &snapshotProp);
    if (FAILED(hResult))
    {
        std::wcout << L"[DiskMgr] Failed GetSnapshotProperties: " << hResult << "\n";
        goto cleanup;
    }

    if (outSnapshotId && snapshotIdBufferSize > 0)
    {
        wchar_t idBuffer[64] = {};
        if (StringFromGUID2(snapshotProp.m_SnapshotId, idBuffer, ARRAYSIZE(idBuffer)) > 0)
        {
            wcsncpy_s(outSnapshotId, snapshotIdBufferSize, idBuffer, _TRUNCATE);
        }
    }

    if (outDeviceObjectPath && deviceObjectPathBufferSize > 0 && snapshotProp.m_pwszSnapshotDeviceObject)
    {
        wcsncpy_s(outDeviceObjectPath, deviceObjectPathBufferSize, snapshotProp.m_pwszSnapshotDeviceObject, _TRUNCATE);
    }

    VssFreeSnapshotProperties(&snapshotProp);
    snapshotCreated = true;
    std::wcout << L"[DiskMgr] VSS snapshot created successfully\n";

cleanup:
    SafeRelease(pAsync);
    SafeRelease(pBackup);
    CoUninitialize();

    if (snapshotCreated)
        return S_OK;

    return FAILED(hResult) ? hResult : E_FAIL;
}

extern "C" __declspec(dllexport) HRESULT DiskMgr_DeleteVssSnapshot(const wchar_t* snapshotId)
{
    if (!snapshotId || wcslen(snapshotId) == 0)
    {
        std::wcout << L"[DiskMgr] Invalid snapshot id\n";
        return E_INVALIDARG;
    }

    std::wcout << L"[DiskMgr] Begin DiskMgr_DeleteVssSnapshot for: " << snapshotId << "\n";

    HRESULT hResult;
    IVssBackupComponents* pBackup = NULL;
    VSS_ID id = GUID_NULL;
    LONG deletedCount = 0;
    VSS_ID nonDeletedId = GUID_NULL;

    hResult = CLSIDFromString(snapshotId, &id);
    if (FAILED(hResult))
    {
        std::wcout << L"[DiskMgr] Failed to parse snapshot id: " << hResult << "\n";
        return hResult;
    }

    hResult = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (hResult == RPC_E_CHANGED_MODE)
    {
        hResult = S_OK;
    }
    else if (FAILED(hResult))
    {
        std::wcout << L"[DiskMgr] Failed CoInitializeEx: " << hResult << "\n";
        return hResult;
    }

    hResult = CreateVssBackupComponents(&pBackup);
    if (FAILED(hResult))
    {
        std::wcout << L"[DiskMgr] Failed CreateVssBackupComponents: " << hResult << "\n";
        goto cleanup;
    }

    hResult = pBackup->InitializeForBackup();
    if (FAILED(hResult))
    {
        std::wcout << L"[DiskMgr] Failed InitializeForBackup: " << hResult << "\n";
        goto cleanup;
    }

    hResult = pBackup->SetContext(VSS_CTX_APP_ROLLBACK);
    if (FAILED(hResult))
    {
        std::wcout << L"[DiskMgr] Failed SetContext: " << hResult << "\n";
        goto cleanup;
    }

    hResult = pBackup->DeleteSnapshots(id, VSS_OBJECT_SNAPSHOT, FALSE, &deletedCount, &nonDeletedId);
    if (FAILED(hResult))
    {
        std::wcout << L"[DiskMgr] Failed DeleteSnapshots: " << hResult << "\n";
        goto cleanup;
    }

    if (deletedCount < 1)
    {
        std::wcout << L"[DiskMgr] Snapshot not found\n";
        hResult = VSS_E_OBJECT_NOT_FOUND;
        goto cleanup;
    }

    std::wcout << L"[DiskMgr] VSS snapshot deleted successfully\n";

cleanup:
    SafeRelease(pBackup);
    CoUninitialize();

    return hResult;
}
