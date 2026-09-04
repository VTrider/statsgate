#pragma once

#ifdef EXU_EXPORTS
#define EXUAPI __declspec(dllexport)
#else
#define EXUAPI __declspec(dllimport)
#endif

#include <ScriptUtils.h>

#include <Windows.h>
#include <delayimp.h>
#undef CONST

#include <concepts>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <string>
#include <type_traits>
#include <unordered_map>

namespace exu2
{
	namespace detail
	{
		inline bool noSplashText = false;
	}

	// Returns the minor version of the game, use this if your mod only supports a certain version(s)
	EXUAPI int DLLAPI GetGameMinorVersion();
	
	// Returns the patch version of the game
	EXUAPI int DLLAPI GetGamePatchVersion();

	// Returns the current dll version Major.Minor.Patch
	EXUAPI const char* DLLAPI GetDLLVersion();

#ifndef EXU_EXPORTS
	const std::filesystem::path GetWorkshopPath();

	// Use this to compare against the DLL version. You should make sure that
	// your header is up to date with the latest DLL.
	constexpr const char* HEADER_VERSION = "1.6.3";

	// WARNING: You MUST call these two functions in DLL_PROCESS_ATTACH, and DLL_PROCESS_DETACH
	// respectively if you are using this library in a dll mission. By default this uses the 
	// version of the library currently on the steam workshop. If you are using a custom or
	// development build, you must set the dll directory accordingly. See the README on
	// GitHub for more info.
	inline void ProcessAttach(const std::filesystem::path& dllDirectory = GetWorkshopPath() / "3515140097" / "Bin",
							  bool noSplashText = false)
	{
		if (strcmp(exu2::GetDLLVersion(), exu2::HEADER_VERSION) != 0)
		{
			std::string message = std::format(
				"Mission DLL is compiled with exu2 header version {},"
				"but the current workshop version is {}."
				"If the mission header is lower than the workshop version, please contact the mod author to update the mod to the latest header."
				"If the mission header is higher than the workshop version, your workshop item is out of date, verify integrity to get the latest update.",
				exu2::HEADER_VERSION, exu2::GetDLLVersion());
			MessageBoxA(NULL, message.c_str(), "Extra Utilities 2", MB_ICONERROR | MB_APPLMODAL);
			std::abort();
		}

		SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_APPLICATION_DIR |
								 LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | 
								 LOAD_LIBRARY_SEARCH_SYSTEM32 |
								 LOAD_LIBRARY_SEARCH_USER_DIRS
        );
		AddDllDirectory(dllDirectory.wstring().c_str());
		detail::noSplashText = noSplashText;
	}

	inline void ProcessDetach()
	{
		if (GetModuleHandleW(L"ExtraUtilities2.dll"))
		{
			__FUnloadDelayLoadedDLL2("ExtraUtilities2.dll");
		}
	}
#else
	constexpr int MINIMUM_REQUIRED_VERSION = 205;
#endif

	using VarSysHandler = void(__cdecl*)(unsigned long crc);

	struct IVector2
	{
		int x, y;
	};

	// Callbacks

	enum class BuildEventType
	{
		QUEUE,
		CANCEL,
		BUILD
	};

	enum class ProducerType
	{
		FACTORY, // recycler is also a factory
		CONSTRUCTOR,
		ARMORY
	};

	// TODO: implement lua api

	// This callback records any build event from producers. IMPORTANT NOTE: `buildItemOdf` will always be defined,
	// but the handle `buildItem` is ONLY defined for the event `BUILD`, it will instead be 0, due to the other events not having an object in the
	// world yet.
	// I'm pretty sure this WONT work with hover constructors, but since they are so rare I don't really care to implement it unless it's despra
	// 
	// When cancelling stacks of units from the recycler/factory, this callback will be called multiple times corresponding to how many units were in the stack.
	//
	// In multiplayer this callback will be called with duplicate events in each "world", no one really knows the implications of this but if you want to read the "real" event,
	// filter by world 0 (lockstep).
	using BuildEventCallback_t = void(*)(int curWorld, ProducerType producerType, Handle producer, int producerTeam, BuildEventType event, const char* buildItemOdf, Handle buildItem);
	EXUAPI void DLLAPI SetBuildEventCallback(BuildEventCallback_t callback);

	// Camera

	// Perspective functions warning: don't use the built in matrix functions,
	// they only work on the 3x3 portion of the matrix, perspective transformations
	// require 4x4, and you need vec4's with a W component.

	// Returns the current perspective projection matrix.
	EXUAPI Matrix DLLAPI GetPerspectiveMatrix();

	// Returns the current view matrix 
	EXUAPI Matrix DLLAPI GetViewMatrix();

	// Returns if the local player is in satellite mode. This is triggered by using the terminal
	// of a comm bunker, antenna, eyes of xyr, etc. or by a camera pod with a satellite deploy key.
	EXUAPI bool DLLAPI InSatellite();

	// Wrapper over WorldToScreen if you only want to know if something is visible.
	// Note that this only determines if a point is within the camera's view,
	// it doesn't account for map geometry or line of sight, so you should combine
	// this with a ray cast or other line of sight check.
	EXUAPI bool DLLAPI IsVisible(Handle h);
	EXUAPI bool DLLAPI IsVisible(const Matrix& m);
	EXUAPI bool DLLAPI IsVisible(const Vector& m);

	// Fills outScreen with normalized screen coordinates [0,1] that correspond to a position in the world.
	// The z component is the depth, larger values are closer to the camera.
	// Returns true if the position is visible, otherwise false.
	EXUAPI bool DLLAPI WorldToScreen(Handle h, Vector* outScreen);
	EXUAPI bool DLLAPI WorldToScreen(const Matrix& worldMat, Vector* outScreen);
	EXUAPI bool DLLAPI WorldToScreen(const Vector& worldPosition, Vector* outScreen);

	// Console

	// Gets the count of arguments supplied to the current command
	// note that it also includes the name of the command, so it will always
	// be a minimum of 1, subtract 1 for the real count of arguments supplied
	EXUAPI int DLLAPI IFace_GetArgCount();

	// Return value is true if it could parse the correct data type, false if not.
	// The out parameter will not be modified if the function returns false.
	// In C++ you should initialize the value to 0 or nullptr in the case of GetArgString,
	// in Lua it will always return nil if the function fails

	// Gets a float value at the given position if it exists
	EXUAPI bool DLLAPI IFace_GetArgFloat(int arg, float& value);

	// Gets an integer value at the given position if it exists
	EXUAPI bool DLLAPI IFace_GetArgInteger(int arg, int& value);

	// Gets a string value at the given position if it exists
	EXUAPI bool DLLAPI IFace_GetArgString(int arg, char*& value);

	// Gets a bool value at the given position if it exists.
	// Accepts "true" or "false", or,  0 or 1
	EXUAPI bool DLLAPI IFace_GetArgBool(int arg, bool& value);

	// Filesystem

	// Helpers for common directories that mods usually use
	const std::filesystem::path GetBZCCPath();
	// WARNING: if the user has the `-novista` launch option this will result in all reads and writes
	// to this directory and its children being redirected to `../steamapps/common/BZ2R`
	const std::filesystem::path GetMyDocs();
	const std::filesystem::path GetWorkshopPath();

	// Graphics

	// Gets the current viewport size in pixels (X, Y)
	EXUAPI IVector2 GetViewportSize();

	// Mission

	// Gets the current MisnExport and MisnExport2 structs if you don't
	// have access to them (maybe an injected context).
	EXUAPI const MisnExport* const DLLAPI GetMissionExport();
	EXUAPI const MisnExport2* const DLLAPI GetMissionExport2();

	// Steam

	// Gets the name of the active config mod ie. "1325933293.cfg". This will be the
	// Steam Workshop ID for Steam mods, or the name of the .cfg file of a local mod.
	EXUAPI const char* const DLLAPI GetActiveConfigMod();

	// Gets the Steam 64 ID of the local user
	EXUAPI uint64_t DLLAPI GetSteam64();

	// Gets the Steam64 ID of the user at the given team if it exists.
	// Returns 0 if there is no player associated with the team.
	EXUAPI uint64_t DLLAPI GetSteam64(int team);

	// Terrain

	enum class TerrainQueryResult : uint32_t
	{
		NOT_BUILDABLE,
		BUILDABLE,
		INVALID_ODF
	};

	// Queries if the tile closest to `pos` is valid for building the given `team` and `odf`. `front` is the direction
	// the building is facing. BUILDABLE if the tile is buildable and fills the out parameter `pos` with
	// the resulting build matrix position. Returns NOT_BUILDABLE if the tile is not buildable and `pos` is unmodified.
	// Returns INVALID_ODF if the odf does not have a GameObjectClass associated with it (probably malformed in some way)
	// IMPORTANT NOTE: if you don't want the game to stutter the first time you call this function for a given odf,
	// make sure to call PreloadODF(odf) in InitialSetup/Start or wherever you want beforehand.
	EXUAPI TerrainQueryResult DLLAPI IsTerrainBuildable(int team, const char* odf, Vector& pos, const Vector& front);

	// VarSys

	// Low level create command. Creates an "unregistered" VarSys command, it will show up in the `ls` command but
	// it will not seen by the current mission's ProcessCommand function. You can use VarSys_RegisterHandler to set
	// a custom handler that can even work outside of a game. IFace_CreateCommand should be used when in a normal mission.
	EXUAPI void DLLAPI VarSys_CreateCmd(ConstName name);

	// Deletes an IFace item and all its subdirectories, returns true if
	// successful, or false if the item does not exist
	EXUAPI bool DLLAPI IFace_DeleteItem(ConstName name);

	// Registers a handler for a VarSys scope, ie. in a command "exu.stuff.function", "exu" and "stuff" are both
	// considered a scope. The handler is a function that follows the same API as ProcessCommand in ScriptUtils.
	// The magic number is usually 0 in the game's code so I recommend passing that in, anything else is untested.
	// UNABLE TO BE BOUND IN LUA
	EXUAPI void DLLAPI VarSys_RegisterHandler(ConstName name, VarSysHandler handler, unsigned long magic);

	// Installs a global command handler. This handles anything in the "global scope" like "ls"
	// or "help"
	EXUAPI void DLLAPI VarSys_InstallGlobalHandler(VarSysHandler handler);

	// Uninstalls the installed global handler if it exists
	EXUAPI void DLLAPI VarSys_UninstallGlobalHandler();

	// TODO: Reverse the rest of the flags
	enum class VarFlag : uint32_t {
		CONST = 0x4,
		NODELETE = 0x8000
	};

	// Checks if a flag is set on an IFace variable/VarItem and stores the status in `flagStatus`.
	// Returns true if the var exists, false otherwise.
	EXUAPI bool DLLAPI VarSys_GetVarFlag(ConstName name, VarFlag flag, bool& flagStatus);

	// Sets a flag on an IFace variable/VarItem. Returns true if the var exists, false otherwise.
	EXUAPI bool DLLAPI VarSys_SetVarFlag(ConstName name, VarFlag flag, bool status);

	// DLL Only Helpers

	template <typename... Args>
	void PrintConsoleMessage(std::string_view fmt, Args&&... args)
	{
		::PrintConsoleMessage(std::vformat(fmt, std::make_format_args(args...)).c_str());
	}

	// Same as the lua function
	inline int GetTPS()
	{
		return SecondsToTurns(1.0f);
	}

	// Still not sure the implications of the different worlds but I think you should only
	// do important stuff in world 0 (lockstep world)
	inline bool IsLockstepWorld()
	{
		return GetCurWorld() == 0;
	}

	namespace detail
	{
		class CachedODF
		{
		public:
			CachedODF(const std::string& name) 
				: name(name)
			{
				OpenODF(name.c_str());
			}
			~CachedODF()
			{
				CloseODF(name.c_str());
			}

		private:
			std::string name;
		};

		inline std::unordered_map<std::string, CachedODF> odf_cache; // this will be closed automatically when the dll exits
	}

	enum class ODFError
	{
		VALUE_NOT_FOUND, // not found without inheritance
		REACHED_BASE_ODF // not found through the entire inheritance tree
	};

	inline std::expected<float, ODFError> GetODFFloat(const std::string& odf, const std::string& block, const std::string& name, bool useInheritance = true)
	{
		if (!detail::odf_cache.contains(odf))
			detail::odf_cache.emplace(odf, odf);

		float value;

		if (::GetODFFloat(odf.c_str(), block.c_str(), name.c_str(), &value))
			return value;

		if (!useInheritance)
			return std::unexpected(ODFError::VALUE_NOT_FOUND);

		std::string current_odf = odf;
		char buf[ODF_MAX_LEN];
		while (::GetODFString(current_odf.c_str(), "GameObjectClass", "classLabel", ODF_MAX_LEN, buf))
		{
			std::string parent = buf;
			parent += ".odf";

			if (!detail::odf_cache.contains(parent))
				detail::odf_cache.emplace(parent, parent);

			if (::GetODFFloat(parent.c_str(), block.c_str(), name.c_str(), &value))
				return value;

			current_odf = parent;
		}

		return std::unexpected(ODFError::REACHED_BASE_ODF);
	}

	// Do not modify

	// These are inline since it's risky to pass an stl object across dll boundaries

	inline const std::filesystem::path GetBZCCPath()
	{
		wchar_t bzccPath[MAX_PATH];
		GetModuleFileNameW(NULL, bzccPath, MAX_PATH);
		return std::filesystem::path(bzccPath).parent_path();
	}

	inline const std::filesystem::path GetMyDocs()
	{
		size_t bufSize = 0;
		GetOutputPath(bufSize, nullptr);
		std::unique_ptr<wchar_t[]> path = std::make_unique<wchar_t[]>(bufSize);
		GetOutputPath(bufSize, path.get());
		return std::filesystem::path(path.get());
	}

	inline const std::filesystem::path GetWorkshopPath()
	{
		DWORD size = MAX_PATH;
		std::wstring steamPath(MAX_PATH, L'\0');
		RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath", RRF_RT_REG_SZ, NULL, steamPath.data(), &size);

		steamPath.resize(size / sizeof(wchar_t) - 1);
		std::filesystem::path workshopPath(steamPath);
		workshopPath.append("steamapps").append("workshop").append("content").append("624970");

		return workshopPath;
	}

#ifndef EXU_EXPORTS
	inline FARPROC WINAPI DelayLoadHandler(unsigned int dliNotify, [[maybe_unused]] PDelayLoadInfo pdli)
	{
		if (_stricmp(pdli->szDll, "ExtraUtilities2.dll") == 0)
		{
			if (dliNotify == dliFailLoadLib)
			{
				const wchar_t* msg = L"Failed to find ExtraUtilities2.dll. This can happen if the workshop item "
									  "is not installed, or if you are running a custom build and failed to set "
									  "the dll search path. Please subscribe to the workshop item at " 
								      "https://steamcommunity.com/sharedfiles/filedetails/?id=3515140097 "
					                  "or see the README on GitHub for more info.";
				MessageBoxW(NULL, msg, L"Extra Utilities 2", MB_ICONERROR | MB_APPLMODAL);
				std::terminate();
			}
		}

		return NULL;
	}

	// The selectany attribute is like inline but it makes it into a "strong" symbol
	// which keeps it compatible with what the Windows API is expecting, inline won't work.
	extern "C" __declspec(selectany) const PfnDliHook  __pfnDliNotifyHook2 = DelayLoadHandler;
	extern "C" __declspec(selectany) const PfnDliHook  __pfnDliFailureHook2 = DelayLoadHandler;
#endif
}

namespace std
{
	using namespace exu2;

	template<>
	struct formatter<BuildEventType> : formatter<string>
	{
		auto format(BuildEventType t, format_context& ctx) const
		{
			using enum BuildEventType;
			std::string str;
			switch (t)
			{
			case QUEUE:
				str = "QUEUE";
				break;
			case CANCEL:
				str = "CANCEL";
				break;
			case BUILD:
				str = "BUILD";
				break;
			}
			return format_to(ctx.out(), "{}", str);
		}
	};

	template<>
	struct formatter<ProducerType> : formatter<string>
	{
		auto format(ProducerType t, format_context& ctx) const
		{
			using enum ProducerType;
			std::string str;
			switch (t)
			{
			case FACTORY:
				str = "FACTORY";
				break;
			case CONSTRUCTOR:
				str = "CONSTRUCTOR";
				break;
			case ARMORY:
				str = "ARMORY";
				break;
			}
			return format_to(ctx.out(), "{}", str);
		}
	};

	template <>
	struct formatter<TerrainQueryResult>: formatter<string>
	{
		auto format(TerrainQueryResult r, format_context& ctx) const
		{
			using enum TerrainQueryResult;
			std::string str;
			switch (r)
			{
			case NOT_BUILDABLE:
				str = "NOT_BUILDABLE";
				break;
			case BUILDABLE:
				str = "BUILDABLE";
				break;
			case INVALID_ODF:
				str = "INVALID_ODF";
				break;
			}
			return format_to(ctx.out(), "{}", str);
		}
	};
}
