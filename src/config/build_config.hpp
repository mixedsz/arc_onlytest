#pragma once

#ifdef _DEBUG
    #define DEBUG_CONSOLE 1
#else
    // Uncomment to enable console output in release builds:
    // #define DEBUG_CONSOLE 1
#endif

#ifdef DEBUG_CONSOLE
    #include <Windows.h>
    #include <iostream>
    #include <mutex>
    #include <sstream>
    #include "debug/Logger.hpp"

    namespace DbgLogDetail {
        inline std::mutex g_mtx;
        inline BOOL WINAPI ConsoleCtrlHandler(DWORD) { return TRUE; }
    }

#ifndef ENABLE_QUICK_EDIT_MODE
#define ENABLE_QUICK_EDIT_MODE 0x0040
#endif
#ifndef ENABLE_EXTENDED_FLAGS
#define ENABLE_EXTENDED_FLAGS 0x0080
#endif

    #define DBG_LOG(...)  do { \
        std::lock_guard<std::mutex> _dbgLk(DbgLogDetail::g_mtx); \
        std::ostringstream _dbgOss; \
        _dbgOss << __VA_ARGS__; \
        std::cout << _dbgOss.str() << std::endl; \
        Logger_SinkInfo(_dbgOss.str()); \
    } while(0)
    #define DBG_ERR(...)  do { \
        std::lock_guard<std::mutex> _dbgLk(DbgLogDetail::g_mtx); \
        std::ostringstream _dbgOss; \
        _dbgOss << __VA_ARGS__; \
        std::cerr << _dbgOss.str() << std::endl; \
        Logger_SinkError(_dbgOss.str()); \
    } while(0)
    #define DBG_CONSOLE_INIT() do { \
        AllocConsole(); \
        FILE* f = nullptr; \
        freopen_s(&f, "CONOUT$", "w", stdout); \
        freopen_s(&f, "CONOUT$", "w", stderr); \
        SetConsoleCtrlHandler(DbgLogDetail::ConsoleCtrlHandler, TRUE); \
        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE); \
        DWORD mode = 0; \
        if (hIn && hIn != INVALID_HANDLE_VALUE && GetConsoleMode(hIn, &mode)) { \
            mode &= ~ENABLE_QUICK_EDIT_MODE; \
            mode |= ENABLE_EXTENDED_FLAGS; \
            SetConsoleMode(hIn, mode); \
        } \
    } while(0)
#else
    #define DBG_LOG(...)  ((void)0)
    #define DBG_ERR(...)  ((void)0)
    #define DBG_CONSOLE_INIT() ((void)0)
#endif
