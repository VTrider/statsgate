#pragma once

#include "mission_hook.h"

#include <Windows.h>
#include <ExtraUtils.h>
#include <ScriptUtils.h>

#include <filesystem>
#include <stdexcept>

namespace statsgate
{
	inline const char* version = "1.0.0-beta.1";
	inline const std::filesystem::path mod_folder = exu2::GetMyDocs() / "statsgate";

	class stat_exception : public std::runtime_error 
	{
	public:
		stat_exception(const std::string& msg) : std::runtime_error(msg) {}
	};
}
