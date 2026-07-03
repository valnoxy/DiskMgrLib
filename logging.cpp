#include "pch.h"
#include "logging.h"

#include <iostream>

namespace {

    // Discards everything written to it; used to mute std::wcout / std::wcerr without
    // touching every individual log call site across the library.
    class NullWideBuffer : public std::wstreambuf {
    protected:
        int_type overflow(int_type c) override {
            return c == traits_type::eof() ? traits_type::eof() : c;
        }
    };

    NullWideBuffer g_nullBuffer;
    std::wstreambuf* g_originalCoutBuf = nullptr;
    std::wstreambuf* g_originalCerrBuf = nullptr;

}

extern "C" __declspec(dllexport) void DiskMgr_SetSilentMode(bool silent)
{
    if (silent)
    {
        if (!g_originalCoutBuf)
        {
            g_originalCoutBuf = std::wcout.rdbuf(&g_nullBuffer);
            g_originalCerrBuf = std::wcerr.rdbuf(&g_nullBuffer);
        }
    }
    else
    {
        if (g_originalCoutBuf)
        {
            std::wcout.rdbuf(g_originalCoutBuf);
            std::wcerr.rdbuf(g_originalCerrBuf);
            g_originalCoutBuf = nullptr;
            g_originalCerrBuf = nullptr;
        }
    }
}
