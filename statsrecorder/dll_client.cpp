#include "dll_client.h"

#include "stat_client.h"

#include <memory>

namespace statsgate
{
	static std::unique_ptr<stat_client> client;

	startup_result DLLAPI start_dll_client()
	{
		if (!check_singleton())
			return startup_result::failed_existing_client;

		client = std::make_unique<stat_client>(stat_client::type::hosted_dll, nullptr);
		client->start_hosted();
	}
}
