#include "pch.h"
#include "snapshot.h"
#include "logging.h"

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
        DISKMGR_LOG_ERROR(L"Invalid volume path");
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

    DISKMGR_LOG_DEBUG(L"Begin DiskMgr_CreateVssSnapshot for: " << volumePath);

    hResult = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (hResult == RPC_E_CHANGED_MODE)
    {
        hResult = S_OK;
    }
    else if (FAILED(hResult))
    {
        DISKMGR_LOG_ERROR(L"Failed CoInitializeEx: " << hResult);
        return hResult;
    }

    hResult = CreateVssBackupComponents(&pBackup);
    if (FAILED(hResult))
    {
        DISKMGR_LOG_ERROR(L"Failed CreateVssBackupComponents: " << hResult);
        goto cleanup;
    }

    hResult = pBackup->InitializeForBackup();
    if (FAILED(hResult))
    {
        DISKMGR_LOG_ERROR(L"Failed InitializeForBackup: " << hResult);
        goto cleanup;
    }

    hResult = pBackup->SetContext(VSS_CTX_APP_ROLLBACK);
    if (FAILED(hResult))
    {
        DISKMGR_LOG_ERROR(L"Failed SetContext: " << hResult);
        goto cleanup;
    }

    hResult = pBackup->SetBackupState(false, false, VSS_BT_COPY, false);
    if (FAILED(hResult))
    {
        DISKMGR_LOG_ERROR(L"Failed SetBackupState: " << hResult);
        goto cleanup;
    }

    hResult = pBackup->GatherWriterMetadata(&pAsync);
    if (FAILED(hResult))
    {
        DISKMGR_LOG_ERROR(L"Failed GatherWriterMetadata: " << hResult);
        goto cleanup;
    }
    hResult = pAsync->Wait();
    SafeRelease(pAsync);
    if (FAILED(hResult))
    {
        DISKMGR_LOG_ERROR(L"Failed GatherWriterMetadata (Wait): " << hResult);
        goto cleanup;
    }

    hResult = pBackup->StartSnapshotSet(&snapshotSetId);
    if (FAILED(hResult))
    {
        DISKMGR_LOG_ERROR(L"Failed StartSnapshotSet: " << hResult);
        goto cleanup;
    }

    hResult = pBackup->AddToSnapshotSet((VSS_PWSZ)volumePath, GUID_NULL, &snapshotId);
    if (FAILED(hResult))
    {
        DISKMGR_LOG_ERROR(L"Failed AddToSnapshotSet: " << hResult);
        goto cleanup;
    }

    hResult = pBackup->PrepareForBackup(&pAsync);
    if (FAILED(hResult))
    {
        DISKMGR_LOG_ERROR(L"Failed PrepareForBackup: " << hResult);
        goto cleanup;
    }
    hResult = pAsync->Wait();
    SafeRelease(pAsync);
    if (FAILED(hResult))
    {
        DISKMGR_LOG_ERROR(L"Failed PrepareForBackup (Wait): " << hResult);
        goto cleanup;
    }

    hResult = pBackup->DoSnapshotSet(&pAsync);
    if (FAILED(hResult))
    {
        DISKMGR_LOG_ERROR(L"Failed DoSnapshotSet: " << hResult);
        goto cleanup;
    }
    hResult = pAsync->Wait();
    SafeRelease(pAsync);
    if (FAILED(hResult))
    {
        DISKMGR_LOG_ERROR(L"Failed DoSnapshotSet (Wait): " << hResult);
        goto cleanup;
    }

    hResult = pBackup->GetSnapshotProperties(snapshotId, &snapshotProp);
    if (FAILED(hResult))
    {
        DISKMGR_LOG_ERROR(L"Failed GetSnapshotProperties: " << hResult);
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
    DISKMGR_LOG_INFO(L"VSS snapshot created successfully");

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
        DISKMGR_LOG_ERROR(L"Invalid snapshot id");
        return E_INVALIDARG;
    }

    DISKMGR_LOG_DEBUG(L"Begin DiskMgr_DeleteVssSnapshot for: " << snapshotId);

    HRESULT hResult;
    IVssBackupComponents* pBackup = NULL;
    VSS_ID id = GUID_NULL;
    LONG deletedCount = 0;
    VSS_ID nonDeletedId = GUID_NULL;

    hResult = CLSIDFromString(snapshotId, &id);
    if (FAILED(hResult))
    {
        DISKMGR_LOG_ERROR(L"Failed to parse snapshot id: " << hResult);
        return hResult;
    }

    hResult = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (hResult == RPC_E_CHANGED_MODE)
    {
        hResult = S_OK;
    }
    else if (FAILED(hResult))
    {
        DISKMGR_LOG_ERROR(L"Failed CoInitializeEx: " << hResult);
        return hResult;
    }

    hResult = CreateVssBackupComponents(&pBackup);
    if (FAILED(hResult))
    {
        DISKMGR_LOG_ERROR(L"Failed CreateVssBackupComponents: " << hResult);
        goto cleanup;
    }

    hResult = pBackup->InitializeForBackup();
    if (FAILED(hResult))
    {
        DISKMGR_LOG_ERROR(L"Failed InitializeForBackup: " << hResult);
        goto cleanup;
    }

    hResult = pBackup->SetContext(VSS_CTX_APP_ROLLBACK);
    if (FAILED(hResult))
    {
        DISKMGR_LOG_ERROR(L"Failed SetContext: " << hResult);
        goto cleanup;
    }

    hResult = pBackup->DeleteSnapshots(id, VSS_OBJECT_SNAPSHOT, FALSE, &deletedCount, &nonDeletedId);
    if (FAILED(hResult))
    {
        DISKMGR_LOG_ERROR(L"Failed DeleteSnapshots: " << hResult);
        goto cleanup;
    }

    if (deletedCount < 1)
    {
        DISKMGR_LOG_ERROR(L"Snapshot not found");
        hResult = VSS_E_OBJECT_NOT_FOUND;
        goto cleanup;
    }

    DISKMGR_LOG_INFO(L"VSS snapshot deleted successfully");

cleanup:
    SafeRelease(pBackup);
    CoUninitialize();

    return hResult;
}
