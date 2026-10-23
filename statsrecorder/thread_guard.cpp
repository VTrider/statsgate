#include "thread_guard.h"

thread_guard::thread_guard()
{
	auto snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	THREADENTRY32 entry{};
	entry.dwSize = sizeof(THREADENTRY32);
	if (Thread32First(snap, &entry))
	{
		do
		{
			if (entry.th32ThreadID == GetCurrentThreadId() or entry.th32OwnerProcessID != GetProcessId(GetCurrentProcess()))
				continue;
			
			if (auto thread = OpenThread(THREAD_SUSPEND_RESUME, 0, entry.th32ThreadID))
			{
				SuspendThread(thread);
				suspended_threads.push_back(thread);
			}
		} while (Thread32Next(snap, &entry));
	}
	CloseHandle(snap);
}

thread_guard::~thread_guard()
{
	for (const auto& thread : suspended_threads)
	{
		ResumeThread(thread);
		CloseHandle(thread);
	}
}
