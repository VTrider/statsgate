#pragma once

#include <ExtraUtils.h>

#include <concepts>
#include <functional>
#include <string>
#include <unordered_map>

namespace statsgate
{
	class command
	{
	public:
		command(const std::string& name, const std::function<void()>& func);

		static void handler(unsigned long crc);

	private:
		static inline bool handler_active = false;
		static inline std::unordered_map<unsigned long, std::function<void()>> command_map{};
		std::string name;
		std::function<void()> func;
		unsigned long crc;
	};
}
