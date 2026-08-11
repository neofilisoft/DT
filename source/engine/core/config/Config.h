#pragma once

#include "core/platform/Types.h"

#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

// ---------------------------------------------------------------------------
// Config.h
//
// Hierarchical key-value configuration with layered override resolution.
// Three layers, resolved in this priority order (highest wins):
//   1. User overrides   (per-player settings.cfg - resolution, key bindings)
//   2. Game config      (game/config/*.cfg - Domaintic-specific tuning:
//                        starting simulation speed, default household
//                        budget, etc.)
//   3. Engine defaults  (engine/config/*.cfg - renderer defaults, job
//                        system worker count override, log verbosity)
//
// This layering is what lets the engine ship sane defaults while a game
// project overrides only what it needs, and a player's local settings
// override only what THEY changed - without needing three copies of every
// key in every file. A key absent from the user/game layer transparently
// falls through to the next layer down.
//
// Format: a minimal INI-like text format (`[Section]` headers,
// `key = value` lines, `#` or `;` comments). This is deliberately simpler
// than full JSON/TOML for config specifically (as opposed to .asset
// content data, which uses the JSON diagnostic format from
// core/serialization) - config files are hand-edited far more often than
// content data, by non-programmers on a small team, and a flat INI-style
// format has near-zero syntax to get wrong (no bracket/quote-matching
// errors like a mistyped JSON file produces).
// ---------------------------------------------------------------------------

namespace dt
{
    using ConfigValue = std::variant<bool, i64, f64, std::string>;

    class ConfigLayer
    {
    public:
        bool LoadFromFile(const std::string& path);
        bool LoadFromString(const std::string& contents);

        bool HasKey(const std::string& section, const std::string& key) const;
        const ConfigValue* Find(const std::string& section, const std::string& key) const;

        void Set(const std::string& section, const std::string& key, ConfigValue value);

        std::vector<std::string> GetSectionNames() const;
        std::vector<std::string> GetKeysInSection(const std::string& section) const;

    private:
        // section -> (key -> value). Two-level map rather than a single
        // flat "section.key" string map, so GetKeysInSection can enumerate
        // a section's keys without a linear scan filtering by prefix -
        // this is what the Editor's Config inspector panel uses to render
        // one collapsible group per section.
        std::unordered_map<std::string, std::unordered_map<std::string, ConfigValue>> m_sections;
    };

    class Config
    {
    public:
        static Config& Get();

        // Loads engine defaults first, then optionally a game-layer file,
        // then optionally a user-layer file. Call again (e.g. after the
        // user changes a setting in an options menu and hits Apply) to
        // reload the user layer without restarting the process.
        void LoadEngineDefaults(const std::string& path);
        void LoadGameConfig(const std::string& path);
        void LoadUserOverrides(const std::string& path);

        bool SaveUserOverrides(const std::string& path) const;

        // Resolution order: user -> game -> engine, first hit wins.
        bool GetBool(const std::string& section, const std::string& key, bool defaultValue) const;
        i64 GetInt(const std::string& section, const std::string& key, i64 defaultValue) const;
        f64 GetFloat(const std::string& section, const std::string& key, f64 defaultValue) const;
        std::string GetString(const std::string& section, const std::string& key, const std::string& defaultValue) const;

        // Always writes to the user layer - config values a game or
        // engine reads directly from their own layers are meant to be
        // edited by hand in those files, not overwritten by a runtime
        // Set() call from arbitrary code.
        void SetUserOverride(const std::string& section, const std::string& key, ConfigValue value);

    private:
        const ConfigValue* Resolve(const std::string& section, const std::string& key) const;

        ConfigLayer m_engineDefaults;
        ConfigLayer m_gameConfig;
        ConfigLayer m_userOverrides;
    };
}
