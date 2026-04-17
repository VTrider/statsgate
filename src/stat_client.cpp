#include "stat_client.h"

#include "command.h"

#include <ExtraUtils.h>
#include <google/protobuf/util/time_util.h>
#include <google/protobuf/util/json_util.h>
#include <google/protobuf/io/gzip_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#ifdef GetCurrentTime
#undef GetCurrentTime // this interferes with protobuf
#endif

#include <chrono>
#include <filesystem>
#include <fstream>

namespace statsgate
{
	MisnExport stat_client::export_hook{
		.Update = stat_client::Update, 
		.PostRun = stat_client::PostRun,
		.ObjectKilled = stat_client::ObjectKilled
	};

	MisnExport2 stat_client::export2_hook{
		.m_pPreOrdnanceHitCallback = stat_client::BulletHit,
		.m_pPostBulletInitCallback = stat_client::BulletInit,
		.m_pPreDamageCallback = stat_client::PreDamage
	};

	void stat_client::Update()
	{
		client()->record_update();
		client()->hooks.get_mission().Update();
	}

	void stat_client::PostRun()
	{
		client()->last_tick();
		client()->hooks.get_mission().PostRun();
	}

	EjectKillRetCodes stat_client::ObjectKilled(Handle DeadObjectHandle, Handle KillersHandle)
	{
		client()->record_object_killed(DeadObjectHandle, KillersHandle);
		return client()->hooks.get_mission().ObjectKilled(DeadObjectHandle, KillersHandle);
	}

	void stat_client::BulletHit(Handle shooterHandle, Handle victimHandle, int ordnanceTeam, const char* pOrdnanceODF)
	{
		client()->record_bullet_hit(shooterHandle, victimHandle, ordnanceTeam, pOrdnanceODF);
		if (auto* cb = client()->hooks.get_mission2().m_pPreOrdnanceHitCallback)
			cb(shooterHandle, victimHandle, ordnanceTeam, pOrdnanceODF);
	}

	void stat_client::BulletInit(Handle shooterHandle, const Matrix& ordnanceMat, const Vector& ordnanceVel,
		int ordnanceTeam, float ordnanceLifespan, const char* pOrdnanceODF)
	{
		client()->record_bullet_init(shooterHandle, ordnanceMat, ordnanceVel, ordnanceTeam, ordnanceLifespan, pOrdnanceODF);
		if (auto* cb = client()->hooks.get_mission2().m_pPostBulletInitCallback)
			cb(shooterHandle, ordnanceMat, ordnanceVel, ordnanceTeam, ordnanceLifespan, pOrdnanceODF);
	}

	void stat_client::PreDamage(const int curWorld, Handle h, const char* pContext, DAMAGE& dmg)
	{
		client()->record_damage(curWorld, h, pContext, dmg);
		if (auto* cb = client()->hooks.get_mission2().m_pPreDamageCallback)
			cb(curWorld, h, pContext, dmg);
	}

	void stat_client::register_commands()
	{
        exu2::PrintConsoleMessage("statsgate.dll version v{} by VTrider", version);
        exu2::VarSys_RegisterHandler("stats", command::handler, 0);
        exu2::VarSys_RegisterHandler("stats.client", command::handler, 0);
        exu2::VarSys_RegisterHandler("stats.debug", command::handler, 0);

		command cmd_debug_allocated("stats.debug.allocations", [this]()
		{
			exu2::PrintConsoleMessage("Current allocations: {:.3f} mb", static_cast<double>(stat_session.SpaceUsedLong()) / 1e6);
		});

		command cmd_recording_active("stats.client.recording", [this]()
		{
			exu2::PrintConsoleMessage("{}", recording);
		});

		command about("stats.about", []()
		{
			exu2::PrintConsoleMessage("statsgate.dll v{} by VTrider, special thanks to F9bomber, Sev, and the rest of the VSR community!", version);
		});

		command shutdown("stats.shutdown", [this]()
		{
			if (client_type != type::freestanding)
				PrintConsoleMessage("Shutdown is not supported for hosted clients, please exit the mission");

			if (recording)
				last_tick();

			PrintConsoleMessage("Shutting down");
			running_freestanding->clear(std::memory_order::release);
		});

		// Todo: add chat rate limit bypass
		//command flip("stats.flip", []()
		//	{
		//		static std::random_device rd;
		//		static std::mt19937 gen(rd());
		//		static std::uniform_int_distribution dist(0, 1);
		//		for (int i = 0; i < 5; i++)
		//		{
		//			IFace_ConsoleCmd(std::format("network.chateditline \"{}\"", dist(gen) ? "Heads!" : "Tails!").c_str());
		//			IFace_ConsoleCmd("network.chatline.entered");
		//		}
		//	});


		const std::string client_type_str = [this]()
		{
			using enum type;
			switch (client_type)
			{
			case freestanding:
				return "freestanding";
			case hosted_dll:
				return "hosted_dll";
			case hosted_lua:
				return "hosted_lua";
			default:
				std::unreachable();
			}
		}();
		IFace_CreateString("stats.client.type", client_type_str.c_str());
		exu2::VarSys_SetVarFlag("stats.client.type", exu2::VarFlag::CONST, true);
	}

	stat_client::stat_client(type t, std::atomic_flag* running_freestanding)
		: client_type(t), hooks(export_hook, export2_hook), running_freestanding(running_freestanding)
	{
		register_instance(this);
		register_commands();
	}

	stat_client::~stat_client()
	{
		exu2::IFace_DeleteItem("stats");
	}

	stat_client* stat_client::client()
	{
		return current_instance;
	}

	void stat_client::register_instance(stat_client* self)
	{
		current_instance = self;
	}

	void stat_client::poll_mission_change()
	{
		if (client_type != type::freestanding) [[unlikely]]
			throw stat_exception("Automatic mission hooking isn't supported on hosted clients");
		hooks.update();
	}

	void stat_client::record_update()
	{
		if (client_type == type::freestanding && !recording && running_freestanding->test(std::memory_order::acquire) == true)
		{
			first_tick();
			recording = true;
		}

		auto* tick = stat_session.add_event_stream()->mutable_update_tick();
		tick->set_tick(GetLockstepTurn());
		for (auto& [teamnum, nick] : stat_session.header().teamnum_to_s64())
		{
			auto* player = tick->add_players();
			player->set_player(exu2::GetSteam64(teamnum));
			Handle h = GetPlayerHandle(teamnum); // teamnum should be guaranteed to be a player at this point
			Vector pos = GetPosition(h);
			Vec3 message_pos;
			message_pos.set_x(pos.x);
			message_pos.set_y(pos.y);
			message_pos.set_z(pos.z);
			*player->mutable_position() = message_pos;
			Vector veloc = GetVelocity(h);
			player->set_speed(std::hypot(veloc.x, veloc.y, veloc.z));
			player->set_health(GetCurHealth(h));
			player->set_ammo(GetCurAmmo(h));
			player->set_odf(get_odf(h));
		}
	}

	void stat_client::record_object_killed(Handle DeadObjectHandle, Handle KillersHandle)
	{
		auto* unit = stat_session.add_event_stream()->mutable_unit_destroyed();
		unit->set_tick(GetLockstepTurn());

		// appears to work fine but picks up on pods getting picked up presumably, not sure if we want this
		if (IsPlayer(KillersHandle))
			unit->set_killer(s64_from_h(KillersHandle));
		unit->set_killer_team(GetTeamNum(KillersHandle));
		unit->set_killer_odf(get_odf(KillersHandle));

		if (IsPlayer(DeadObjectHandle))
			unit->set_victim(s64_from_h(DeadObjectHandle));
		unit->set_victim_team(GetTeamNum(DeadObjectHandle));
		unit->set_victim_odf(get_odf(DeadObjectHandle));
	}

	void stat_client::record_bullet_hit(Handle shooterHandle, Handle victimHandle, int ordnanceTeam, const char* pOrdnanceODF)
	{
		// Do not record AI vs AI hits, but AI vs player hits may be interesting
		bool has_shooter = stat_session.header().s64_to_nick().contains(s64_from_h(shooterHandle));
		bool has_victim = stat_session.header().s64_to_nick().contains(s64_from_h(victimHandle));
		if (!has_shooter || !has_victim)
			return;

		auto* hit = stat_session.add_event_stream()->mutable_bullet_hit();

		hit->set_tick(GetLockstepTurn());

		if (has_shooter)
			hit->set_shooter(s64_from_h(shooterHandle));

		hit->set_ordnance_odf(pOrdnanceODF);

		if (has_victim)
			hit->set_victim(s64_from_h(victimHandle));

		hit->set_victim_odf(get_odf(victimHandle));
		hit->set_shooter_odf(get_odf(shooterHandle));
	}

	void stat_client::record_bullet_init(Handle shooterHandle, const Matrix& ordnanceMat, const Vector& ordnanceVel, int ordnanceTeam, float ordnanceLifespan, const char* pOrdnanceODF)
	{
		if (!stat_session.header().s64_to_nick().contains(s64_from_h(shooterHandle)))
			return;

		auto* init = stat_session.add_event_stream()->mutable_bullet_init();
		init->set_tick(GetLockstepTurn());
		init->set_shooter(s64_from_h(shooterHandle));
		init->set_ordnance_odf(pOrdnanceODF);
	}

	void stat_client::record_damage(const int curWorld, Handle h, const char* pContext, const DAMAGE& dmg)
	{
		// Many events for collisions are fired off which do not provide any meaningful data
		if (dmg.damageType == DAMAGE_TYPE_COLLISION)
			return;

		long current_tick = GetLockstepTurn();

		auto* damage = stat_session.add_event_stream()->mutable_damage_dealt();
		damage->set_tick(current_tick);
		if (IsPlayer(dmg.owner))
		{
			damage->set_shooter(s64_from_h(dmg.owner));
		}
		damage->set_team(GetTeamNum(dmg.owner));
		if (pContext) // pContext can be null if the damage is water and some other weird stuff
			damage->set_ordnance_odf(pContext);
		damage->set_amount(dmg.value);

		auto* d2 = stat_session.add_event_stream()->mutable_damage_received();
		d2->set_tick(current_tick);
		if (IsPlayer(h))
		{
			d2->set_victim(s64_from_h(h));
		}
		d2->set_team(GetTeamNum(h));
		if (pContext)
			d2->set_ordnance_odf(pContext);
		d2->set_amount(dmg.value);
	}

	void stat_client::first_tick()
	{
		auto now = std::chrono::system_clock::now();
		session_identifier = std::format("{:%Y-%m-%d-%H-%M-%S}", std::chrono::floor<std::chrono::seconds>(now));
		exu2::PrintConsoleMessage("Started stat session {}", session_identifier);

		StatHeader header;
		header.set_map(GetMissionFilename());
		*header.mutable_start_time() = google::protobuf::util::TimeUtil::GetCurrentTime();
		header.set_author_nickname(GetPlayerName(GetPlayerHandle()));
		header.set_author_steam64(exu2::GetSteam64());
		header.set_tick_rate(exu2::GetTPS());

		// TODO: Rescan teamnums when players leave and join
		for (int teamnum = 1; teamnum <= 10; teamnum++)
		{
			if (Handle h = GetPlayerHandle(teamnum))
			{
				uint64_t s64 = exu2::GetSteam64(teamnum);
				header.mutable_s64_to_nick()->emplace(s64, GetPlayerName(h));
				header.mutable_teamnum_to_s64()->emplace(teamnum, s64);
				header.mutable_s64_to_teamnum()->emplace(s64, teamnum);
				header.set_player_count(header.player_count() + 1);
			}
		}

		header.set_active_config_mod(exu2::GetActiveConfigMod());

		*stat_session.mutable_header() = header;
	}

	void stat_client::last_tick()
	{
		stat_session.mutable_header()->set_last_tick(GetLockstepTurn());

		std::ofstream file = std::ofstream(mod_folder / "stats" / std::format("{}.binpb.gz", session_identifier), std::ios::binary);
		google::protobuf::io::OstreamOutputStream output_stream(&file);

		google::protobuf::io::GzipOutputStream::Options options;
		options.format = google::protobuf::io::GzipOutputStream::GZIP;
		options.compression_level = 9;

		google::protobuf::io::GzipOutputStream gzip_stream(&output_stream, options);

		// This will cause a stutter in debug mode but it's still working
		if (stat_session.SerializeToZeroCopyStream(&gzip_stream))
		{
			exu2::PrintConsoleMessage("Finalized stat session {}.binpb.gz", session_identifier);
		}
		else
		{
			exu2::PrintConsoleMessage("Failed to finalize stat session {}");
		}

		// Note need to call flush here NOT close, otherwise it won't write the data to disk idk why
		gzip_stream.Flush();
		file.flush();
		stat_session.Clear();
		session_identifier.clear();
		recording = false;
	}

	uint64_t stat_client::s64_from_h(Handle h)
	{
		if (IsPlayer(h))
			return exu2::GetSteam64(GetTeamNum(h));

		return 0;
	}

	std::string stat_client::get_odf(Handle h)
	{
		char odf[ODF_MAX_LEN];
		GetObjInfo(h, Get_ODF, odf);
		return std::string(odf);
	}
}