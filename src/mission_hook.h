#pragma once

#include <Windows.h>
#include <ScriptUtils.h>

namespace statsgate
{
	class mission_hook
	{
	public:
		mission_hook(const MisnExport& hooks, const MisnExport2& hooks2);
		~mission_hook();
		const MisnExport& get_mission() const;
		const MisnExport2& get_mission2() const;
		void apply_hooks();
		void update(); // this will be running on a separate thread from the game

	private:
		MisnExport* current_export = nullptr;
		MisnExport2* current_export2 = nullptr;
		bool hooks_enabled = false;
		MisnExport old_mission;
		MisnExport2 old_mission2;
		MisnExport hooks;
		MisnExport2 hooks2;
	};
}
