#pragma once
#include <iostream>
#include <stdio.h>
#pragma comment(lib, "urlmon.lib")
#include <windows.h>
#include <random>

#include <chrono>
#include <cstdlib>
#include <TlHelp32.h>
#include <filesystem>
#include <vector>
#include <fstream>
#include <sstream>
#include <wininet.h>
#include <cstdarg>
#include <string>
#include <Urlmon.h>
#include <fcntl.h>
#include <io.h>
#include <cstdint>
#include <psapi.h>
#include <fileapi.h>
#include <shlwapi.h>
#include <intrin.h>
#include <algorithm>
#include <setupapi.h>
#include <iphlpapi.h>
#include <wchar.h>
#include "lazy.hpp"
#include "Spoof.h"

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "wininet.lib")

#define secure __forceinline

namespace fs = std::filesystem;


DWORD findProcess(const wchar_t* name) {
    const auto snap = LI_FN(CreateToolhelp32Snapshot)(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32W proc_entry{};
    proc_entry.dwSize = sizeof(proc_entry);

    if (LI_FN(Process32FirstW)(snap, &proc_entry)) {
        do {
            if (LI_FN(wcscmp)(name, proc_entry.szExeFile) == 0) {
                LI_FN(CloseHandle)(snap);
                return proc_entry.th32ProcessID;
            }
        } while (LI_FN(Process32NextW)(snap, &proc_entry));
    }

    LI_FN(CloseHandle)(snap);
    return 0;
}





