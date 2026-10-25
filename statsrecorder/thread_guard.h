#pragma once

#include <Windows.h>
#include <TlHelp32.h>

#include <vector>

// Suspends all threads of the current process except the current one
// in the current scope
class thread_guard
{
public:
	thread_guard();
	thread_guard(thread_guard&) = delete;
	thread_guard(thread_guard&& g) noexcept : suspended_threads(std::move(g.suspended_threads)) {}
	thread_guard& operator=(thread_guard& g) = delete;
	thread_guard& operator=(thread_guard&& g) noexcept { this->suspended_threads = std::move(g.suspended_threads); }
	~thread_guard();

private:
	std::vector<HANDLE> suspended_threads;
};
