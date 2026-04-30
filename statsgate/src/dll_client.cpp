#include "statsgate.h"
#include "stat_client.h"
#include <ScriptUtils.h>

#include <memory>

namespace statsgate
{
	static std::unique_ptr<stat_client> client;

	void DLLAPI start_dll_client()
	{
		if (!check_singleton())
			return;
		client = std::make_unique<stat_client>(stat_client::type::hosted_dll, nullptr);
		client->start_hosted();
	}
}
