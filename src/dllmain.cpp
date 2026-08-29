// dllmain.cpp : Defines the entry point for the DLL application.

#include "statsgate.h"

#include <ExtraUtils.h>
#include <google/protobuf/stubs/common.h>
#include <windows.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stacktrace>

HANDLE singleton_mutex = NULL;  

bool statsgate::check_singleton()
{
	HANDLE m = CreateMutexW(NULL, false, L"statsgate-client-mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        MessageBoxW(NULL, L"Only one instance of the stat client is allowed", L"statsgate", MB_ICONERROR);
        if (m)
			CloseHandle(m);
        return false;
    }
    singleton_mutex = m;
    return true;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        DisableThreadLibraryCalls(hModule);
        std::set_terminate([]()
        {
            std::ofstream crash_log(statsgate::mod_folder / "crash.txt", std::ios::app);
            crash_log << std::stacktrace::current() << std::endl;
            MessageBoxW(NULL, L"statsgate", L"statsgate.dll has exited abnormally. See why in mydocs\\statesgate\\crash.txt", MB_ICONERROR | MB_APPLMODAL);
            std::abort();
        });
		GOOGLE_PROTOBUF_VERIFY_VERSION;
        exu2::ProcessAttach();
		break;
    }
    case DLL_PROCESS_DETACH:
        exu2::ProcessDetach();
        if (singleton_mutex)
            CloseHandle(singleton_mutex);
        break;
    }
    return TRUE;
}