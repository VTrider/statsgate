#include "command.h"

#include <ScriptUtils.h>

namespace statsgate
{
	command::command(const std::string& name, const std::function<void()>& func)
		: name(name), func(func), crc(CalcCRC(name.c_str()))
	{
		exu2::VarSys_CreateCmd(name.c_str());
		command_map.emplace(crc, func);
	}

	void command::handler(unsigned long crc)
	{
		if (command_map.contains(crc))
		{
			auto& func = command_map.at(crc);
			func();
		}
	}
}