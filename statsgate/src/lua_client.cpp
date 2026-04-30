#include "statsgate.h"
#include "stat_client.h"
#include <ScriptUtils.h>

#include <memory>

struct lua_State;

namespace statsgate
{
	static std::unique_ptr<stat_client> client;

	int start_lua_client(lua_State* L)
	{
		return 0;
	}

	extern "C" __declspec(dllexport) void DLLAPI luaopen_statsgate(lua_State* L)
	{
		if (!check_singleton())
			return;
		client = std::make_unique<stat_client>(stat_client::type::hosted_lua, nullptr);
	}
}
