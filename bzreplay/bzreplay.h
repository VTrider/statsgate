#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <ScriptUtils.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <optional>
#include <variant>
#include <vector>

namespace bzreplay
{
    // Can't serialize glm classes directly for some reason
    struct vec3
    {
        float x, y, z;

        operator glm::vec3() const
        {
            return glm::vec3(x, y, z);
        }
    };

    struct quat
    {
        float w, x, y, z;

        operator glm::quat() const
        {
            return glm::quat(w, x, y, z);
        }
    };

    enum class race
    {
        isdf,
        scion,
        hadean
    };

    enum class outcome
    {
        team1_win,
        team2_win,
        draw,
        game_cancelled
    };

    struct player_info
    {
        uint64_t steam64;
        uint32_t teamnum;
        std::string nickname;
    };

    enum class recorder_type
    {
        freestanding, // injected dll
        hosted_dll,   // integration into DLL mission by map author
        hosted_lua    // integration into Lua mission by map author
    };

    using string_table_key = uint16_t;
    using player_table_key = uint8_t;

    struct header
    {
        std::string magic = "BZREPLAY";
        uint16_t schema_version = 1;

        std::string map;
        uint64_t start_time;
        std::string author_nickname;
        uint64_t author_steam64;
        uint32_t tick_rate;
        std::string active_config_mod;
        uint32_t last_tick;

        float terrain_min_x;
        float terrain_max_x;
        float terrain_min_y;
        float terrain_max_y;
        float terrain_min_z;
        float terrain_max_z;

        bool shutdown_requested;

        race team1_race;
        race team2_race;

        outcome game_outcome;

        std::vector<player_info> players;

        recorder_type recorder_type;
    };

    // Either shooter or victim must be a player for this event to be recorded right now,
    // but if there is interest in AI vs AI hits we can implement that
    struct bullet_hit
    {
        uint32_t tick;
        uint64_t shooter;
        string_table_key ordnance_odf;
        uint64_t victim;
        string_table_key victim_odf;
        string_table_key shooter_odf;
        float distance_to_target;
    };

    struct object_state
    {
        Handle handle;
        int team;
        std::optional<uint64_t> player;
        string_table_key odf;
        vec3 position;
        quat orientation;
        float health;
        float ammo;
        std::optional<Handle> target;

        // constexpr static auto serialize(auto& archive, object_state& o)
        // {
        //     return archive(o.handle, o.team, o.player, 
        //             o.odf, o.position, o.orientation,
        //             o.health, o.ammo, o.target
        //     );
        // }

        // constexpr static auto serialize(auto& archive, const object_state& o)
        // {
        //     return archive(o.handle, o.team, o.player, 
        //             o.odf, o.position, o.orientation,
        //             o.health, o.ammo, o.target
        //     );
        // }
    };

    struct ordnance_state
    {
        uint32_t handle;
        string_table_key odf;
        glm::vec3 position;
        glm::quat orientation;
        float life_span;
    };

    class replay
    {
    public:
        replay();

        void write_file(const std::filesystem::path& path);

    // private:
        using event_type = std::variant<bullet_hit>;

        struct object_state_soa
        {
            std::vector<Handle> handle;
            std::vector<int> team;
            std::vector<std::optional<uint64_t>> player;
            std::vector<string_table_key> odf;
            std::vector<vec3> position;
            std::vector<quat> orientation;
            std::vector<float> health;
            std::vector<float> ammo;
            std::vector<std::optional<Handle>> target;
        };

        struct keyframe
        {
            uint32_t tick;
            object_state_soa objects;
        };

        struct delta
        {
            uint32_t tick;
            object_state_soa objects;
        };

        struct chunk
        {
            keyframe keyframe;
            std::vector<delta> deltas;
            std::vector<event_type> events;
        };

        struct compressed_block
        {
            uint32_t uncompressed_size;
            uint32_t compressed_size;
            std::vector<std::byte> data;
        };

        header header;
        std::vector<chunk> data_segment;
        std::vector<std::string> string_table;
        std::vector<uint64_t> player_table;

        uint8_t keyframe_interval = 5;
    };

};
