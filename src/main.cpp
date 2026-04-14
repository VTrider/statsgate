#include <Windows.h>
#include <Tlhelp32.h>

#include <filesystem>
#include <stdexcept>
#include <string>

HMODULE GetRemoteModuleHandle(DWORD processId, const char* moduleName)
{
    HMODULE hMod = NULL;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
    if (hSnap != INVALID_HANDLE_VALUE)
    {
        MODULEENTRY32 me32{};
        me32.dwSize = sizeof(MODULEENTRY32);
        if (Module32First(hSnap, &me32))
        {
            do
            {
                if (_stricmp(me32.szModule, moduleName) == 0)
                {
                    hMod = me32.hModule;
                    break;
                }
            } while (Module32Next(hSnap, &me32));
        }
        CloseHandle(hSnap);
    }
    return hMod;
}

int main()
{
    try
    {
        std::string dll = (std::filesystem::current_path() / "statsgate.dll").string();

        HMODULE k32 = GetModuleHandleA("kernel32.dll");
        if (!k32)
            throw std::runtime_error("Couldn't find kernel32.dll");

        auto loadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(k32, "LoadLibraryA"));
        if (!loadLibrary)
            throw std::runtime_error("Couldn't resolve LoadLibraryA");

        DWORD processID = 0;
        HWND hWnd = FindWindowW(L"BZCC Main Window", NULL);
        if (!hWnd)
            throw std::runtime_error("Couldn't find BZCC window");

        GetWindowThreadProcessId(hWnd, &processID);
        if (!processID)
            throw std::runtime_error("Couldn't find BZCC process id");

        HANDLE targetHandle = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, processID);
        if (!targetHandle)
            throw std::runtime_error("OpenProcess failed");

        const size_t dllBytes = dll.size() + 1; // include null terminator
        void* remoteBuffer = VirtualAllocEx(targetHandle, nullptr, dllBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (!remoteBuffer)
            throw std::runtime_error("VirtualAllocEx failed");

        if (!WriteProcessMemory(targetHandle, remoteBuffer, dll.c_str(), dllBytes, nullptr))
            throw std::runtime_error("WriteProcessMemory failed");

        HANDLE injectionHandle = CreateRemoteThread(targetHandle, nullptr, 0, loadLibrary, remoteBuffer, 0, nullptr);
        if (!injectionHandle)
            throw std::runtime_error("Failed to spawn LoadLibrary thread");

        WaitForSingleObject(injectionHandle, INFINITE);
        CloseHandle(injectionHandle);

        HMODULE remoteModule = GetRemoteModuleHandle(processID, "statsgate.dll");
        if (!remoteModule)
            throw std::runtime_error("Failed to find statsgate.dll in target process");

        HMODULE localModule = LoadLibraryExA(dll.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES);
        if (!localModule)
            throw std::runtime_error("LoadLibraryExA(DONT_RESOLVE_DLL_REFERENCES) failed");

        FARPROC localRun = GetProcAddress(localModule, "_run_freestanding@4"); // stdcall functions are decorated even with extern "C"
        if (!localRun)
        {
            FreeLibrary(localModule);
            throw std::runtime_error("Export run_freestanding not found");
        }

        const uintptr_t rva = reinterpret_cast<uintptr_t>(localRun) - reinterpret_cast<uintptr_t>(localModule);
        FreeLibrary(localModule);

        auto remoteRun = reinterpret_cast<LPTHREAD_START_ROUTINE>(reinterpret_cast<uintptr_t>(remoteModule) + rva);

        HANDLE runHandle = CreateRemoteThread(targetHandle, nullptr, 0, remoteRun, remoteModule, 0, nullptr);
        if (!runHandle)
            throw std::runtime_error("Failed to spawn run_freestanding thread");

        CloseHandle(runHandle);
        VirtualFreeEx(targetHandle, remoteBuffer, 0, MEM_RELEASE);
        CloseHandle(targetHandle);
    }
    catch (const std::exception& e)
    {
        MessageBoxA(NULL, e.what(), "statslauncher", MB_ICONERROR);
    }

    return 0;
}