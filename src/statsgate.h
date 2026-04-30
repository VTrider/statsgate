#pragma once

#include <ExtraUtils.h>

#include <filesystem>
#include <stdexcept>

namespace statsgate
{
	inline const char* version = "1.0.0-beta.1";
	inline const std::filesystem::path mod_folder = exu2::GetMyDocs() / "statsgate";

	bool check_singleton();

	extern "C" __declspec(dllexport) void DLLAPI start_dll_client();

	class stat_exception : public std::runtime_error 
	{
	public:
		stat_exception(const std::string& msg) : std::runtime_error(msg) {}
	};
}
