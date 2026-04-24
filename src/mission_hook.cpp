#include "mission_hook.h"

#include "thread_guard.h"

#include <ExtraUtils.h>

MisnImport misnImport{}; // definition for scriptutils

namespace statsgate
{
	mission_hook::mission_hook(const MisnExport& hooks, const MisnExport2& hooks2)
		: old_mission(MisnExport()), old_mission2(MisnExport2()), hooks(hooks), hooks2(hooks2)
	{
	}

	mission_hook::~mission_hook()
	{
		// Shutting down mid game the exports will be active so we need to reset them,
		// otherwise they will be nullptr in shell since the game already cleaned them up
		// so we do nothing
		if (current_export)
		{
			*current_export = old_mission;
		}
		if (current_export2)
		{
			*current_export2 = old_mission2;
		}
	}

	const MisnExport& mission_hook::get_mission() const
	{
		return old_mission;
	}

	const MisnExport2& mission_hook::get_mission2() const
	{
		return old_mission2;
	}

	void mission_hook::apply_hooks()
	{
		thread_guard guard;

		current_export = const_cast<MisnExport*>(exu2::GetMissionExport());
		old_mission = *current_export;
		// TODO: add the rest of these
		if (hooks.Update)
			current_export->Update = hooks.Update;
		if (hooks.PostRun)
			current_export->PostRun = hooks.PostRun;
		if (hooks.ObjectKilled)
			current_export->ObjectKilled = hooks.ObjectKilled;

		// Fill misnImport for scriptutils
		misnImport = *current_export->misnImport;

		current_export2 = const_cast<MisnExport2*>(exu2::GetMissionExport2());
		old_mission2 = *current_export2;
		if (hooks2.m_pChatMessageSentCallback)
			current_export2->m_pChatMessageSentCallback = hooks2.m_pChatMessageSentCallback;
		if (hooks2.m_pPostTargetChangedCallback)
			current_export2->m_pPostTargetChangedCallback = hooks2.m_pPostTargetChangedCallback;
		if (hooks2.m_pPreGetInCallback)
			current_export2->m_pPreGetInCallback = hooks2.m_pPreGetInCallback;
		if (hooks2.m_pPreOrdnanceHitCallback)
			current_export2->m_pPreOrdnanceHitCallback = hooks2.m_pPreOrdnanceHitCallback;
		if (hooks2.m_pPrePickupPowerupCallback)
			current_export2->m_pPrePickupPowerupCallback = hooks2.m_pPrePickupPowerupCallback;
		if (hooks2.m_pPreSnipeCallback)
			current_export2->m_pPreSnipeCallback = hooks2.m_pPreSnipeCallback;
		if (hooks2.m_pPostBulletInitCallback)
			current_export2->m_pPostBulletInitCallback = hooks2.m_pPostBulletInitCallback;
		if (hooks2.m_pPreDamageCallback)
			current_export2->m_pPreDamageCallback = hooks2.m_pPreDamageCallback;

		hooks_enabled = true;
	}

	void mission_hook::update()
	{
		// Check if you are in a mission based off of the status of the current
		// MisnExport, you can assume MisnExport2 will behave identically so
		// we only have to check one of them. Technically not thread safe so it might
		// break once in a blue moon, should probably be replaced with a hook the main thread hits
		if (exu2::GetMissionExport() == nullptr && hooks_enabled) // you left a mission
		{
			// The game automatically cleans up the current mission export so no need here
			hooks_enabled = false;
			current_export = nullptr;
			old_mission = MisnExport();
		}
		else if (exu2::GetMissionExport() && !hooks_enabled) // you entered a mission
		{
			apply_hooks();
		}
	}
}
