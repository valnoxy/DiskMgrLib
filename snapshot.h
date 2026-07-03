#ifndef DISKMGR_SNAPSHOT_H
#define DISKMGR_SNAPSHOT_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

    __declspec(dllexport) HRESULT DiskMgr_CreateVssSnapshot(
        const wchar_t* volumePath,
        wchar_t* outSnapshotId,
        int snapshotIdBufferSize,
        wchar_t* outDeviceObjectPath,
        int deviceObjectPathBufferSize
    );

    __declspec(dllexport) HRESULT DiskMgr_DeleteVssSnapshot(
        const wchar_t* snapshotId
    );

#ifdef __cplusplus
}
#endif

#endif // DISKMGR_SNAPSHOT_H
