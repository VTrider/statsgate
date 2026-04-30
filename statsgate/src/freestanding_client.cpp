#include "statsgate.h"
#include "stat_client.h"
#include "thread_guard.h"

#include <Windows.h>

#include <atomic>
#include <chrono>
#include <thread>

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
