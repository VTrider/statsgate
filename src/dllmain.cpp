// dllmain.cpp : Defines the entry point for the DLL application.

#include "stat_client.h"
#include "statsgate.h"
#include "thread_guard.h"

#include <ExtraUtils.h>
#include <windows.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stacktrace>
#include <thread>

HANDLE singleton_mutex = NULL;  

bool check_singleton()
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

using namespace statsgate;

std::atomic_flag running_freestanding;

extern "C" __declspec(dllexport) DWORD WINAPI run_freestanding(LPVOID hModule)
{
    if (!check_singleton())
    {
		FreeLibraryAndExitThread(static_cast<HMODULE>(hModule), 0);
        return 0;
    }
    
    running_freestanding.test_and_set(std::memory_order::relaxed);

    std::unique_ptr<stat_client> client;
    {
        thread_guard g;

        client = std::make_unique<stat_client>(stat_client::type::freestanding, &running_freestanding);
    }

    while (running_freestanding.test(std::memory_order::acquire) == true)
    {
        client->poll_mission_change();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    {
        thread_guard g;
        client.reset();
    }

    FreeLibraryAndExitThread(static_cast<HMODULE>(hModule), 0);

    return 0;
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
            std::ofstream crash_log(mod_folder / "crash.txt", std::ios::app);
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