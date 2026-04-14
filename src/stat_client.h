#pragma once

#include "mission_hook.h"
#include "statsgate.h"
#include "statsgate.pb.h"

#include <cstring>
#include <fstream>

namespace statsgate
{
	// MisnExport2 without default constructor to allow designated initializer
	struct MisnExport2 
	{
		ChatMessageSentCallback		m_pChatMessageSentCallback = nullptr;
		PostTargetChangedCallback	m_pPostTargetChangedCallback = nullptr;
		PreGetInCallback			m_pPreGetInCallback = nullptr;
		PreOrdnanceHitCallback		m_pPreOrdnanceHitCallback = nullptr;
		PrePickupPowerupCallback	m_pPrePickupPowerupCallback = nullptr;
		PreSnipeCallback			m_pPreSnipeCallback = nullptr;
		PostBulletInitCallback		m_pPostBulletInitCallback = nullptr;
		PreDamageCallback			m_pPreDamageCallback = nullptr;

		// Make it convertable to stock this is stupid lol idk why you can't just cast
		// it has the same memory layout
		operator ::MisnExport2() const
		{
			::MisnExport2 e;
			std::memcpy(&e, this, sizeof(MisnExport2));
			return e;
		}
	};

	class stat_client
	{
	public:
		enum class type
		{
			freestanding, // injected dll to record autonomously
			hosted_dll,   // integration into DLL mission by map author
			hosted_lua    // integration into LuaMission by map author
		};

		stat_client(type t, std::atomic_flag* running_freestanding);
		~stat_client();
		static stat_client* client();
		static void register_instance(stat_client* self);

		void poll_mission_change();

		void record_update();
		void record_bullet_hit(Handle shooterHandle, Handle victimHandle, int ordnanceTeam, const char* pOrdnanceODF);
		void record_bullet_init(Handle shooterHandle, const Matrix& ordnanceMat, const Vector& ordnanceVel, int ordnanceTeam, float ordnanceLifespan, const char* pOrdnanceODF);
		void record_damage(const int curWorld, Handle h, const char* pContext, const DAMAGE& dmg);

		void first_tick();
		void last_tick();

		// MisnExport
		static void Update();
		static void PostRun();

		// MisnExport2
		static void BulletHit(Handle shooterHandle, Handle victimHandle, int ordnanceTeam, const char* pOrdnanceODF);
		static void BulletInit(Handle shooterHandle, const Matrix &ordnanceMat, const Vector &ordnanceVel, int ordnanceTeam, float ordnanceLifespan, const char* pOrdnanceODF);
		static void PreDamage(const int curWorld, Handle h, const char* pContext, DAMAGE& dmg);

	private:
		const type client_type;
		std::atomic_flag* running_freestanding = nullptr; // for freestanding clients this will have a flag that controls the automatic hooks
		bool recording = false;

		ClientStatSession stat_session;
		std::string session_identifier; // will be a timestamp formatted like OBS recordings

		static inline stat_client* current_instance = nullptr;
		mission_hook hooks;
		static MisnExport export_hook;
		static MisnExport2 export2_hook;

		void register_commands();
		uint64_t s64_from_h(Handle h); // no validation, ensure this is actually a player or it will return 0
	};
}
