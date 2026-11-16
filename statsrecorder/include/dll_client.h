#pragma once

#include <ScriptUtils.h>

#include <cstdint>

namespace statsgate
{
    enum class startup_result : uint32_t
    {
        success,                // startup was successful
        failed_user_disabled,   // user has recording disabled in their config
        failed_existing_client, // a stat client already exists (did you call this twice?)
        failed_internal_error   // something else went wrong
    };

	extern "C" __declspec(dllexport) startup_result DLLAPI start_dll_client();
}
