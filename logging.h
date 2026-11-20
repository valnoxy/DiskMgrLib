#ifndef DISKMGR_LOGGING_H
#define DISKMGR_LOGGING_H

#include <windows.h>

#ifdef __cplusplus
#include <sstream>
#include <string>
#endif

// Log levels passed to the log callback.
#define DISKMGR_LOG_LEVEL_DEBUG   0
#define DISKMGR_LOG_LEVEL_INFO    1
#define DISKMGR_LOG_LEVEL_WARNING 2
#define DISKMGR_LOG_LEVEL_ERROR   3

// Receives every log message of DiskMgrLib
typedef void (__cdecl *DiskMgrLogCallback)(int level, const wchar_t* message);

#ifdef __cplusplus
extern "C" {
#endif

    // Suppresses (or re-enables) the console output (std::wcout / std::wcerr) of DiskMgrLib.
    // Does not affect a registered log callback.
    __declspec(dllexport) void DiskMgr_SetSilentMode(bool silent);

    // Registers a callback which receives all log messages. While a callback is registered,
    // nothing is written to the console. Pass NULL to restore console output.
    __declspec(dllexport) void DiskMgr_SetLogCallback(DiskMgrLogCallback callback);

#ifdef __cplusplus
}

// Internal: dispatches a message to the callback or the console.
void DiskMgr_Log(int level, const std::wstring& message);

#define DISKMGR_LOG(level, expr) \
    do { std::wostringstream diskmgr_log_stream_; diskmgr_log_stream_ << expr; DiskMgr_Log((level), diskmgr_log_stream_.str()); } while (0)

#define DISKMGR_LOG_DEBUG(expr)   DISKMGR_LOG(DISKMGR_LOG_LEVEL_DEBUG, expr)
#define DISKMGR_LOG_INFO(expr)    DISKMGR_LOG(DISKMGR_LOG_LEVEL_INFO, expr)
#define DISKMGR_LOG_WARNING(expr) DISKMGR_LOG(DISKMGR_LOG_LEVEL_WARNING, expr)
#define DISKMGR_LOG_ERROR(expr)   DISKMGR_LOG(DISKMGR_LOG_LEVEL_ERROR, expr)
#endif

#endif // DISKMGR_LOGGING_H
