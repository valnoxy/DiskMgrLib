#ifndef DISKMGR_LOGGING_H
#define DISKMGR_LOGGING_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

    // Suppresses (or re-enables) all console output written by DiskMgrLib via std::wcout / std::wcerr.
    __declspec(dllexport) void DiskMgr_SetSilentMode(bool silent);

#ifdef __cplusplus
}
#endif

#endif // DISKMGR_LOGGING_H
