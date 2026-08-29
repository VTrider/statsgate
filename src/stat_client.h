#pragma once

#include "mission_hook.h"
#include "statsgate.h"
#include "statsgate.pb.h"

#include <glaze/glaze.hpp>
#include <glaze/toml.hpp>

#include <cstring>
#include <optional>
#include <unordered_set>

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

		// Freestanding client function
		void poll_mission_change();

		// Hosted client function
		void start_hosted();

		void record_update();
		void record_object_killed(Handle DeadObjectHandle, Handle KillersHandle);
		void record_bullet_hit(Handle shooterHandle, Handle victimHandle, int ordnanceTeam, const char* pOrdnanceODF);
		void record_pickup_powerup(const int curWorld, Handle me, Handle powerupHandle);
		void record_snipe(const int curWorld, Handle shooterHandle, Handle victimHandle, int ordnanceTeam, const char* pOrdnanceODF);
		void record_bullet_init(Handle shooterHandle, const Matrix& ordnanceMat, const Vector& ordnanceVel, int ordnanceTeam, float ordnanceLifespan, const char* pOrdnanceODF);
		void record_damage(const int curWorld, Handle h, const char* pContext, const DAMAGE& dmg);
		void record_build_event(exu2::ProducerType producerType, Handle producer, int producerTeam, exu2::BuildEventType event, const char* buildItemOdf, Handle buildItem);
		void record_resource_state(UpdateTick* tick);

		void first_tick();
		void poll_tick(); // this function does polling and records handles before record_update()
		void last_tick();

		// MisnExport
		static void Update();
		static void PostRun();
		static EjectKillRetCodes ObjectKilled(Handle DeadObjectHandle, Handle KillersHandle);

		// MisnExport2
		static void BulletHit(Handle shooterHandle, Handle victimHandle, int ordnanceTeam, const char* pOrdnanceODF);
		static PrePickupPowerupReturnCodes PickupPowerup(const int curWorld, Handle me, Handle powerupHandle);
		// I'm not sure if PreSnipe is the best callback to use over the ObjectSniped callback in MisnExport,
		// but I think it might be more likely to give valid data since it's before the snipe actually happens.
		static PreSnipeReturnCodes PreSnipe(const int curWorld, Handle shooterHandle, Handle victimHandle, int ordnanceTeam, const char* pOrdnanceODF);
		static void BulletInit(Handle shooterHandle, const Matrix &ordnanceMat, const Vector &ordnanceVel, int ordnanceTeam, float ordnanceLifespan, const char* pOrdnanceODF);
		static void PreDamage(const int curWorld, Handle h, const char* pContext, DAMAGE& dmg);

		// Extra Utilities 2
		static void BuildEvent(exu2::ProducerType producerType, Handle producer, int producerTeam, exu2::BuildEventType event, const char* buildItemOdf, Handle buildItem);

	private:
		enum class output_format : uint8_t
		{
			proto,
			json
		};

		struct config
		{
			bool enable_recorder;
			std::string output_directory;
			output_format output_format;
		};

		friend struct glz::meta<stat_client::config>;

		static inline const config default_config {
			.enable_recorder = true,
			.output_directory = (mod_folder / "stats").string(),
			.output_format = output_format::proto
		};

		static inline const std::string config_path = (mod_folder / "statsgate.toml").string();
		
		config client_config;
		const type client_type;
		std::atomic_flag* running_freestanding = nullptr; // for freestanding clients this will have a flag that controls the automatic hooks
		bool recording = false;

		ClientStatSession stat_session;
		std::string session_identifier; // will be a timestamp formatted like OBS recordings

		static inline stat_client* current_instance = nullptr;
		mission_hook hooks;
		static MisnExport export_funcs;
		static MisnExport2 export2_funcs;

		std::unordered_set<std::string> ignored_odfs; // any event containing these odfs will be ignored

		std::unordered_map<uint64_t, PlayerInfo> player_list; // list of players that appeared in the session
		std::vector<Handle> current_tick_handles;

		// Helper functions
		static void register_instance(stat_client* self);
		void register_commands();
		void write_default_config();
		void register_config();
		uint64_t s64_from_h(Handle h);
		std::string get_odf(Handle h);
		std::optional<uint64_t> is_player(Handle h);
		uint32_t get_pool_count(int teamnum);
		uint32_t get_upgrade_count(int teamnum);
		ScrapStatus get_scrap_status(int teamnum);
		std::string get_gameobj_class(Handle h);
	};
}

template <>
struct glz::meta<statsgate::stat_client::output_format>
{
	using enum statsgate::stat_client::output_format;
	static constexpr auto value = enumerate("proto", proto, "json", json);
};
