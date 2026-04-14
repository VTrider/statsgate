// dllmain.cpp : Defines the entry point for the DLL application.

#include "stat_client.h"
#include "statsgate.h"
#include "thread_guard.h"

#include <ExtraUtils.h>
#include <windows.h>
#include <ScriptUtils.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <stacktrace>
#include <thread>

using namespace statsgate;

std::atomic_flag running_freestanding;

extern "C" __declspec(dllexport) DWORD WINAPI run_freestanding(LPVOID hModule)
{
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
        break;
    }
    return TRUE;
}