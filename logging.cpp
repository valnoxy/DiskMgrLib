#include "pch.h"
#include "logging.h"

#include <atomic>
#include <iostream>
#include <mutex>

namespace {

    std::atomic<DiskMgrLogCallback> g_logCallback{ nullptr };
    std::atomic<bool> g_silent{ false };
    std::mutex g_consoleMutex;

}

extern "C" __declspec(dllexport) void DiskMgr_SetSilentMode(bool silent)
{
    g_silent = silent;
}

extern "C" __declspec(dllexport) void DiskMgr_SetLogCallback(DiskMgrLogCallback callback)
{
    g_logCallback = callback;
}

void DiskMgr_Log(int level, const std::wstring& message)
{
    if (const auto callback = g_logCallback.load())
    {
        callback(level, message.c_str());
        return;
    }

    if (g_silent) return;

    std::lock_guard<std::mutex> lock(g_consoleMutex);
    auto& stream = level >= DISKMGR_LOG_LEVEL_ERROR ? std::wcerr : std::wcout;
    stream << L"[DiskMgr] " << message << L"\n";
}
