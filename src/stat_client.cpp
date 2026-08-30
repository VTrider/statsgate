#include "stat_client.h"

#include "command.h"

#include <ExtraUtils.h>
#include <google/protobuf/util/time_util.h>
#include <google/protobuf/util/json_util.h>
#include <google/protobuf/io/gzip_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include <Windows.h>

#define CONST const // FK OFF MICROSOFT
#include <commctrl.h>
#undef CONST

#ifdef GetCurrentTime
#undef GetCurrentTime // this interferes with protobuf
#endif

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>

//#define TRACY_ENABLE
//#define TRACY_ON_DEMAND
//#include <tracy/tracy/Tracy.hpp>

namespace statsgate
{
	MisnExport stat_client::export_funcs{
		.Update = stat_client::Update, 
		.PostRun = stat_client::PostRun,
		.ObjectKilled = stat_client::ObjectKilled
	};

	MisnExport2 stat_client::export2_funcs{
		.m_pPreOrdnanceHitCallback = stat_client::BulletHit,
		.m_pPreSnipeCallback = stat_client::PreSnipe,
		.m_pPostBulletInitCallback = stat_client::BulletInit,
		.m_pPreDamageCallback = stat_client::PreDamage,
		.m_pPostPickupPowerupCallback = stat_client::PickupPowerup,
	};

	void stat_client::Update()
	{
		{
			//ZoneScopedN("stat_client::Update")
			client()->poll_tick();
			client()->record_update();
		}
		{
			//ZoneScopedN("Strategy02::Update")
			client()->hooks.get_mission().Update();
		}
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

	void stat_client::PickupPowerup(const int curWorld, Handle me, Handle powerupHandle)
	{
		client()->record_pickup_powerup(curWorld, me, powerupHandle);
		if (auto* cb = client()->hooks.get_mission2().m_pPostPickupPowerupCallback)
			return cb(curWorld, me, powerupHandle);
	}

	PreSnipeReturnCodes stat_client::PreSnipe(const int curWorld, Handle shooterHandle, Handle victimHandle, int ordnanceTeam, const char* pOrdnanceODF)
	{
		client()->record_snipe(curWorld, shooterHandle, victimHandle, ordnanceTeam, pOrdnanceODF);
		if (auto* cb = client()->hooks.get_mission2().m_pPreSnipeCallback)
			return cb(curWorld, shooterHandle, victimHandle, ordnanceTeam, pOrdnanceODF);
		return PRESNIPE_KILLPILOT; // appears to be the default if no callback is registered, no resyncs so far
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

	void stat_client::BuildEvent(exu2::ProducerType producerType, Handle producer, int producerTeam, exu2::BuildEventType event, const char* buildItemOdf, Handle buildItem)
	{
		client()->record_build_event(producerType, producer, producerTeam, event, buildItemOdf, buildItem);
	}

	stat_client::stat_client(type t, std::atomic_flag* running_freestanding)
		: client_type(t), hooks(export_funcs, export2_funcs), running_freestanding(running_freestanding)
	{
		register_instance(this);
		register_commands();
		std::filesystem::create_directories(mod_folder);
		std::filesystem::create_directories(mod_folder / "stats");
		register_config();
	}

	stat_client::~stat_client()
	{
		exu2::IFace_DeleteItem("stats");
	}

	stat_client* stat_client::client()
	{
		return current_instance;
	}

	void stat_client::poll_mission_change()
	{
		if (client_type != type::freestanding) [[unlikely]]
			throw stat_exception("Automatic mission hooking isn't supported on hosted clients");

		if (client_config.enable_recorder == false)
			return;

		hooks.update();
	}

	void stat_client::start_hosted()
	{
		if (client_type != type::hosted_dll &&
			client_type != type::hosted_lua) [[unlikely]]
			throw stat_exception("This function isn't supported on non-hosted clients"); // TODO: maybe don't throw exception across dll lol idk

		if (client_config.enable_recorder == false)
			return;

		hooks.apply_hooks();
	}

	void stat_client::record_update()
	{
		if (!recording)
		{
			switch (client_type)
			{
				case type::freestanding:
				{
					if (running_freestanding->test(std::memory_order::acquire) == false)
						break;
					[[fallthrough]];
				}
				case type::hosted_dll:
				case type::hosted_lua:
				{
					first_tick();
					recording = true;
				}
			}
		}

		auto* tick = stat_session.add_event_stream()->mutable_update_tick();
		long cur_turn = GetLockstepTurn();
		tick->set_tick(cur_turn);

		// It's easier to just scan every team slot to see if a player is innit
		for (int teamnum = 1; teamnum <= 10; teamnum++)
		{
			uint64_t steamid = exu2::GetSteam64(teamnum);
			if (!steamid)
				continue;

			if (!player_list.contains(steamid))
			{
				PlayerInfo info;
				info.set_steam64(steamid);
				info.set_teamnum(teamnum);
				info.set_nickname(GetPlayerName(GetPlayerHandle(teamnum)));
				player_list.emplace(steamid, info);
			}

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

			// I didn't realize these were integers but it's probably better to process this data as float
			player->set_health(static_cast<float>(GetCurHealth(h)));
			player->set_ammo(static_cast<float>(GetCurAmmo(h)));
			player->set_odf(get_odf(h));

			player->set_has_target(GetUserTarget(teamnum) ? true : false);
		}

		record_resource_state(tick);

		stat_session.mutable_header()->set_last_tick(cur_turn);
	}

	void stat_client::record_object_killed(Handle DeadObjectHandle, Handle KillersHandle)
	{
		auto* unit = stat_session.add_event_stream()->mutable_unit_destroyed();
		unit->set_tick(GetLockstepTurn());

		int killer_team = GetTeamNum(KillersHandle);
		std::string killer_odf = get_odf(KillersHandle);
		std::string victim_odf = get_odf(DeadObjectHandle);

		if (ignored_odfs.contains(killer_odf) || ignored_odfs.contains(victim_odf))
			return;

		// This filters out pod/crate pickups, flame mine impacts and other misc things we shouldn't be recording
		if (killer_team == 0 && killer_odf.empty())
			return;

		if (auto killer = is_player(KillersHandle))
			unit->set_killer(*killer);
		unit->set_killer_team(killer_team);
		unit->set_killer_odf(std::move(killer_odf));

		if (auto victim = is_player(DeadObjectHandle))
			unit->set_victim(*victim);
		unit->set_victim_team(GetTeamNum(DeadObjectHandle));
		unit->set_victim_odf(std::move(victim_odf));
	}

	void stat_client::record_bullet_hit(Handle shooterHandle, Handle victimHandle, int ordnanceTeam, const char* pOrdnanceODF)
	{
		// Do not record AI vs AI hits, but AI vs player hits may be interesting
		auto shooter = is_player(shooterHandle);
		auto victim = is_player(victimHandle);
		if (!shooter || !victim)
			return;

		auto* hit = stat_session.add_event_stream()->mutable_bullet_hit();

		hit->set_tick(GetLockstepTurn());

		if (shooter)
			hit->set_shooter(*shooter);

		hit->set_ordnance_odf(pOrdnanceODF);

		if (victim)
			hit->set_victim(*victim);

		hit->set_victim_odf(get_odf(victimHandle));
		hit->set_shooter_odf(get_odf(shooterHandle));

		Vector sp = GetPosition(shooterHandle);
		Vector vp = GetPosition(victimHandle);
		hit->set_distance_to_target(std::hypot(vp.x - sp.x, vp.y - sp.y, vp.z - sp.z));
	}

	void stat_client::record_pickup_powerup(const int curWorld, Handle me, Handle powerupHandle)
	{
		// This callback is triggered on visual worlds for some reason and sends duplicate events
		if (curWorld != 0)
			return;

		auto* pickup = stat_session.add_event_stream()->mutable_pickup_powerup();

		pickup->set_tick(GetLockstepTurn());

		if (auto p = is_player(me))
			pickup->set_picker(*p);
		pickup->set_picker_team(GetTeamNum(me));
		pickup->set_picker_odf(get_odf(me));

		pickup->set_powerup_team(GetTeamNum(powerupHandle));
		pickup->set_powerup_odf(get_odf(powerupHandle));
	}

	void stat_client::record_snipe(const int curWorld, Handle shooterHandle, Handle victimHandle, int ordnanceTeam, const char* pOrdnanceODF)
	{
		if (curWorld != 0)
			return;

		auto* snipe = stat_session.add_event_stream()->mutable_unit_sniped();

		snipe->set_tick(GetLockstepTurn());
		
		if (auto p = is_player(shooterHandle))
			snipe->set_shooter(*p);
		snipe->set_shooter_team(GetTeamNum(shooterHandle));
		snipe->set_shooter_odf(get_odf(shooterHandle));

		if (auto p = is_player(victimHandle))
			snipe->set_victim(*p);
		snipe->set_victim_team(GetTeamNum(victimHandle));
		snipe->set_victim_odf(get_odf(victimHandle));
	}

	void stat_client::record_bullet_init(Handle shooterHandle, const Matrix& ordnanceMat, const Vector& ordnanceVel, int ordnanceTeam, float ordnanceLifespan, const char* pOrdnanceODF)
	{
		if (!is_player(shooterHandle))
			return;

		auto* init = stat_session.add_event_stream()->mutable_bullet_init();
		init->set_tick(GetLockstepTurn());
		init->set_shooter(s64_from_h(shooterHandle));
		init->set_ordnance_odf(pOrdnanceODF);
	}

	void stat_client::record_damage(const int curWorld, Handle h, const char* pContext, const DAMAGE& dmg)
	{
		if (curWorld != 0)
			return;

		// Many events for collisions are fired off which do not provide any meaningful data
		if (dmg.damageType == DAMAGE_TYPE_COLLISION)
			return;

		// Unless there's evidence this is important we should ignore it
		if (dmg.damageType == DAMAGE_TYPE_UNKNOWN)
			return;

		long current_tick = GetLockstepTurn();

		auto* damage = stat_session.add_event_stream()->mutable_damage_dealt();
		damage->set_tick(current_tick);

		if (auto shooter = is_player(dmg.owner))
			damage->set_shooter(*shooter);

		damage->set_shooter_team(GetTeamNum(dmg.owner));
		damage->set_shooter_odf(get_odf(dmg.owner));

		if (auto victim = is_player(h))
			damage->set_victim(*victim);

		damage->set_victim_team(GetTeamNum(h));
		damage->set_victim_odf(get_odf(h));

		if (pContext) // pContext can be null if the damage is water and some other weird stuff
			damage->set_ordnance_odf(pContext);
		damage->set_amount(dmg.value);
	}

	// Todo put this somewhere else idek
	template <typename ProtoEnum, typename ExuEnum>
	ProtoEnum translate_enum(ExuEnum e)
	{
		return static_cast<ProtoEnum>(std::to_underlying(e) + 1); // this is dumb I can't wait for reflection
	}

	void stat_client::record_build_event(exu2::ProducerType producerType, Handle producer, int producerTeam, exu2::BuildEventType event, const char* buildItemOdf, Handle buildItem)
	{
		auto* build_event = stat_session.add_event_stream()->mutable_build_event();

		build_event->set_tick(GetLockstepTurn());
		build_event->set_type(translate_enum<statsgate::BuildEventType>(event));
		build_event->set_producer(translate_enum<statsgate::ProducerType>(producerType));
		build_event->set_teamnum(producerTeam);
		build_event->set_build_odf(buildItemOdf);
	}

	void stat_client::record_resource_state(UpdateTick* tick)
	{
		auto* team1 = tick->mutable_team1_resources();
		team1->set_current_scrap(GetScrap(1));
		team1->set_max_scrap(GetMaxScrap(1));
		team1->set_scrap_status(get_scrap_status(1));
		team1->set_pool_count(get_pool_count(1));
		team1->set_upgrade_count(get_upgrade_count(1));

		auto* team2 = tick->mutable_team2_resources();
		team2->set_current_scrap(GetScrap(6));
		team2->set_max_scrap(GetMaxScrap(6));
		team2->set_scrap_status(get_scrap_status(6));
		team2->set_pool_count(get_pool_count(6));
		team2->set_upgrade_count(get_upgrade_count(6));
	}

	void stat_client::first_tick()
	{
		auto now = std::chrono::system_clock::now();
		session_identifier = std::format("{:%Y-%m-%d-%H-%M-%S}", std::chrono::floor<std::chrono::seconds>(now));
		exu2::PrintConsoleMessage("Started stat session {}", session_identifier);

		exu2::SetBuildEventCallback(stat_client::BuildEvent);

		StatHeader header;
		header.set_map(GetMissionFilename());
		*header.mutable_start_time() = google::protobuf::util::TimeUtil::GetCurrentTime();
		header.set_tick_rate(exu2::GetTPS());

		header.set_active_config_mod(exu2::GetActiveConfigMod());

		header.set_terrain_min_x(GetTerrainMinX());
		header.set_terrain_max_x(GetTerrainMaxX());
		header.set_terrain_min_y(GetTerrainMinY());
		header.set_terrain_max_y(GetTerrainMaxY());
		header.set_terrain_min_z(GetTerrainMinZ());
		header.set_terrain_max_z(GetTerrainMaxZ());

		header.set_shutdown_requested(false);

		switch (GetRaceOfTeam(1))
		{
			case 'i':
			{
				header.set_team1_race(RACE_ISDF);
				break;
			}
			case 'f':
			{
				header.set_team1_race(RACE_SCION);
				break;
			}
			case 'e':
			{
				header.set_team1_race(RACE_HADEAN);
				break;
			}
			default:
				header.set_team1_race(RACE_UNSPECIFIED);
		}

		switch (GetRaceOfTeam(6))
		{
			case 'i':
			{
				header.set_team2_race(RACE_ISDF);
				break;
			}
			case 'f':
			{
				header.set_team2_race(RACE_SCION);
				break;
			}
			case 'e':
			{
				header.set_team2_race(RACE_HADEAN);
				break;
			}
			default:
				header.set_team2_race(RACE_UNSPECIFIED);
		}

		*stat_session.mutable_header() = header;
	}

	void stat_client::poll_tick()
	{
		size_t buf_size;
		GetAllGameObjectHandles(buf_size, nullptr);
		current_tick_handles.resize(buf_size);
		GetAllGameObjectHandles(buf_size, current_tick_handles.data());
	}

	void stat_client::last_tick()
	{
		HWND hwnd = FindWindowW(L"BZCC Main Window", nullptr);

		TASKDIALOG_BUTTON buttons[4];
		buttons[0] = { .nButtonID = OUTCOME_TEAM1_WIN, .pszButtonText = L"Team 1 Win" };
		buttons[1] = { .nButtonID = OUTCOME_TEAM2_WIN, .pszButtonText = L"Team 2 Win" };
		buttons[2] = { .nButtonID = OUTCOME_DRAW, .pszButtonText = L"Draw"};
		buttons[3] = { .nButtonID = OUTCOME_GAME_CANCELLED, .pszButtonText = L"Game Cancelled"};

		// This doesn't account for multiple commanders in one game and I don't really care about that edge case
		std::string cmdr_t1, cmdr_t2;
		for (const auto& [_, player] : player_list)
		{
			if (player.teamnum() == 1)
				cmdr_t1 = player.nickname();

			if (player.teamnum() == 6)
				cmdr_t2 = player.nickname();
		}

		std::wstring team_overview = std::format(L"Team 1 Cmdr: {} - Team 2 Cmdr: {}",
			std::wstring(cmdr_t1.begin(), cmdr_t1.end()),
			std::wstring(cmdr_t2.begin(), cmdr_t2.end())
		);

		TASKDIALOGCONFIG cfg{};
		cfg.cbSize = sizeof(TASKDIALOGCONFIG);
		cfg.hwndParent = hwnd;
		cfg.pszWindowTitle = L"statsgate";
		cfg.cButtons = 4;
		cfg.pButtons = buttons;
		cfg.pszMainInstruction = L"Please select the outcome of the game";
		cfg.pszContent = team_overview.c_str();
		cfg.nDefaultButton = 0;

		Outcome outcome = OUTCOME_UNSPECIFIED;
		TaskDialogIndirect(&cfg, reinterpret_cast<int*>(&outcome), nullptr, nullptr);
		// Todo: directx fullscreen stuff these functions dont work to fix black screen after prompt
		// InvalidateRect(hwnd, nullptr, true);
		// UpdateWindow(hwnd);

		auto* header = stat_session.mutable_header();

		header->set_game_outcome(outcome);

		// These need to be in the last tick because it can sometimes be undefined if the host is recording stats in the first tick
		header->set_author_nickname(GetPlayerName(GetPlayerHandle()));
		header->set_author_steam64(exu2::GetSteam64());

		for (const auto& [_, player] : player_list)
		{
			auto* recorded_player = header->add_players();
			*recorded_player = player;
		}

		std::ofstream file = std::ofstream(std::filesystem::path(client_config.output_directory) / std::format("{}.binpb.gz", session_identifier), std::ios::binary);
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
			exu2::PrintConsoleMessage("Failed to finalize stat session {}", session_identifier);
		}

		// Note need to call flush here NOT close, otherwise it won't write the data to disk idk why
		gzip_stream.Flush();
		file.flush();
		stat_session.Clear();
		session_identifier.clear();
		recording = false;
	}

	// Helper functions

	void stat_client::register_instance(stat_client* self)
	{
		current_instance = self;
	}

	void stat_client::register_commands()
	{
		const char* splash_message = "statsgate.dll v{} by VTrider, special thanks to F9bomber, Sev, and the rest of the VSR community! "
								     "Be sure to visit sevsunday.github.io/vt-stats for the latest data!";

        exu2::PrintConsoleMessage(splash_message, version);
        exu2::VarSys_RegisterHandler("stats", command::handler, 0);
        exu2::VarSys_RegisterHandler("stats.client", command::handler, 0);
        exu2::VarSys_RegisterHandler("stats.config", command::handler, 0);
        exu2::VarSys_RegisterHandler("stats.debug", command::handler, 0);

		command cmd_debug_allocated("stats.debug.allocations", [this]()
		{
			exu2::PrintConsoleMessage("Current allocations: {:.3f} mb", static_cast<double>(stat_session.SpaceUsedLong()) / 1e6);
		});

		command cmd_recording_active("stats.client.recording", [this]()
		{
			exu2::PrintConsoleMessage("{}", recording);
		});

		command about("stats.about", [splash_message]()
		{
			exu2::PrintConsoleMessage(splash_message, version);
		});

		command shutdown("stats.shutdown", [this]()
		{
			if (client_type != type::freestanding)
				PrintConsoleMessage("Shutdown is not supported for hosted clients, please exit the mission");

			if (recording)
			{
				stat_session.mutable_header()->set_shutdown_requested(true);
				last_tick();
			}

			PrintConsoleMessage("Shutting down");
			running_freestanding->clear(std::memory_order::release);
		});

		command enabled("stats.config.enabled", [this]()
		{
			exu2::PrintConsoleMessage("{}", client_config.enable_recorder);
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

	void stat_client::write_default_config()
	{
		std::string _;
		if (auto error = glz::write_file_toml(default_config, config_path, _))
		{
			exu2::PrintConsoleMessage("statsgate: Failed to regenerate default config, {}", glz::format_error(error, _));
		}
	}

	void stat_client::register_config()
	{
		std::string _;
		if (!std::filesystem::exists(mod_folder / "statsgate.toml"))
			write_default_config();

		if (auto error = glz::read_file_toml(client_config, config_path, _))
		{
			exu2::PrintConsoleMessage("statsgate: Failed to parse config, please delete the file, falling back to default config: {}", glz::format_error(error, _));
			client_config = default_config;
		}
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

	std::optional<uint64_t> stat_client::is_player(Handle h)
	{
		uint64_t maybe_s64 = s64_from_h(h);
		if (!maybe_s64)
			return std::nullopt;

		return maybe_s64;
	}

	uint32_t stat_client::get_pool_count(int teamnum)
	{
		uint32_t count = 0;
		for (Handle h : current_tick_handles)
		{
			if (GetTeamNum(h) != teamnum)
				continue;

			if (get_gameobj_class(h) == "CLASS_EXTRACTOR")
				count++;
		}

		return count;
	}

	uint32_t stat_client::get_upgrade_count(int teamnum)
	{
		uint32_t count = 0;
		for (Handle h : current_tick_handles)
		{
			if (GetTeamNum(h) != teamnum)
				continue;

			// There's probably a better way to detect an upgrade but the stock and hadean upgrades don't share the same
			// virtual classes so this is the only way I can think of. There might be a config in the future to set this to a custom
			// value for mods outside of stock and VSR.
			if (get_gameobj_class(h) == "CLASS_EXTRACTOR")
			{
				if (auto result = exu2::GetODFFloat(get_odf(h), "ExtractorClass", "scrapDelay"); result.has_value())
					if (result == 0.5f)
						count++;
			}
		}
		return count;
	}

	ScrapStatus stat_client::get_scrap_status(int teamnum)
	{
		uint32_t pool_count = get_pool_count(teamnum);
		uint32_t upg_count = get_upgrade_count(teamnum);
		uint32_t current_scrap = GetScrap(teamnum);

		// Hardcoded for stock/VSR like upg detection for now
		constexpr uint32_t pool_scrap_hold = 20;

		if (current_scrap < upg_count * pool_scrap_hold)
		{
			return SCRAP_STATUS_RED;
		}
		else if (current_scrap < pool_count * pool_scrap_hold)
		{
			return SCRAP_STATUS_YELLOW;
		}
		else
		{
			return SCRAP_STATUS_GREEN;
		}
	}

	std::string stat_client::get_gameobj_class(Handle h)
	{
		char odf[ODF_MAX_LEN];
		GetObjInfo(h, Get_GOClass, odf);
		return std::string(odf);
	}
}