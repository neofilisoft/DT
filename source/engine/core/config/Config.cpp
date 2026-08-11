#include "core/config/Config.h"
#include "core/filesystem/FileSystem.h"
#include "core/logging/Logger.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace dt
{
    namespace
    {
        std::string Trim(const std::string& s)
        {
            const auto first = s.find_first_not_of(" \t\r\n");
            if (first == std::string::npos)
            {
                return "";
            }
            const auto last = s.find_last_not_of(" \t\r\n");
            return s.substr(first, last - first + 1);
        }

        // Parses a raw value string into the narrowest matching
        // ConfigValue alternative: "true"/"false" -> bool, a string that
        // parses fully as an integer -> i64, a string that parses fully as
        // a float -> f64, otherwise the raw string (with surrounding
        // quotes stripped if present). Trying bool/int/float in that order
        // before falling back to string means a config author writing
        // `fullscreen = true` gets an actual bool at the API rather than
        // having to remember to call GetString and compare against "true"
        // themselves.
        ConfigValue ParseValue(const std::string& raw)
        {
            std::string value = Trim(raw);

            if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
            {
                return value.substr(1, value.size() - 2);
            }

            std::string lower = value;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
            if (lower == "true") return true;
            if (lower == "false") return false;

            if (!value.empty())
            {
                char* endPtr = nullptr;
                const i64 asInt = std::strtoll(value.c_str(), &endPtr, 10);
                if (endPtr == value.c_str() + value.size())
                {
                    return asInt;
                }

                char* endPtrF = nullptr;
                const f64 asFloat = std::strtod(value.c_str(), &endPtrF);
                if (endPtrF == value.c_str() + value.size())
                {
                    return asFloat;
                }
            }

            return value;
        }

        std::string FormatValue(const ConfigValue& value)
        {
            return std::visit([](auto&& v) -> std::string {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, bool>)
                {
                    return v ? "true" : "false";
                }
                else if constexpr (std::is_same_v<T, std::string>)
                {
                    return "\"" + v + "\"";
                }
                else
                {
                    return std::to_string(v);
                }
            }, value);
        }
    }

    bool ConfigLayer::LoadFromFile(const std::string& path)
    {
        auto contents = FileSystem::Get().ReadEntireFile(path);
        if (!contents.has_value())
        {
            DT_LOG_WARN(LogCategory::Config, "ConfigLayer: could not read file '{}'", path);
            return false;
        }

        std::string text(reinterpret_cast<const char*>(contents->data()), contents->size());
        return LoadFromString(text);
    }

    bool ConfigLayer::LoadFromString(const std::string& contents)
    {
        std::istringstream stream(contents);
        std::string line;
        std::string currentSection = "";

        while (std::getline(stream, line))
        {
            const std::string trimmed = Trim(line);

            if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';')
            {
                continue;
            }

            if (trimmed.front() == '[' && trimmed.back() == ']')
            {
                currentSection = trimmed.substr(1, trimmed.size() - 2);
                continue;
            }

            const auto eqPos = trimmed.find('=');
            if (eqPos == std::string::npos)
            {
                DT_LOG_WARN(LogCategory::Config, "ConfigLayer: malformed line (no '='): '{}'", trimmed);
                continue;
            }

            const std::string key = Trim(trimmed.substr(0, eqPos));
            const std::string rawValue = trimmed.substr(eqPos + 1);

            m_sections[currentSection][key] = ParseValue(rawValue);
        }

        return true;
    }

    bool ConfigLayer::HasKey(const std::string& section, const std::string& key) const
    {
        return Find(section, key) != nullptr;
    }

    const ConfigValue* ConfigLayer::Find(const std::string& section, const std::string& key) const
    {
        auto sectionIt = m_sections.find(section);
        if (sectionIt == m_sections.end())
        {
            return nullptr;
        }
        auto keyIt = sectionIt->second.find(key);
        if (keyIt == sectionIt->second.end())
        {
            return nullptr;
        }
        return &keyIt->second;
    }

    void ConfigLayer::Set(const std::string& section, const std::string& key, ConfigValue value)
    {
        m_sections[section][key] = std::move(value);
    }

    std::vector<std::string> ConfigLayer::GetSectionNames() const
    {
        std::vector<std::string> names;
        names.reserve(m_sections.size());
        for (const auto& [name, _] : m_sections)
        {
            names.push_back(name);
        }
        return names;
    }

    std::vector<std::string> ConfigLayer::GetKeysInSection(const std::string& section) const
    {
        std::vector<std::string> keys;
        auto it = m_sections.find(section);
        if (it != m_sections.end())
        {
            keys.reserve(it->second.size());
            for (const auto& [key, _] : it->second)
            {
                keys.push_back(key);
            }
        }
        return keys;
    }

    // --- Config ----------------------------------------------------------

    Config& Config::Get()
    {
        static Config instance;
        return instance;
    }

    void Config::LoadEngineDefaults(const std::string& path)
    {
        if (!m_engineDefaults.LoadFromFile(path))
        {
            DT_LOG_ERROR(LogCategory::Config, "Failed to load engine defaults from '{}'", path);
        }
    }

    void Config::LoadGameConfig(const std::string& path)
    {
        if (!m_gameConfig.LoadFromFile(path))
        {
            DT_LOG_WARN(LogCategory::Config, "No game config loaded from '{}' (using engine defaults only)", path);
        }
    }

    void Config::LoadUserOverrides(const std::string& path)
    {
        if (!m_userOverrides.LoadFromFile(path))
        {
            DT_LOG_INFO(LogCategory::Config, "No user overrides found at '{}' (first run, using defaults)", path);
        }
    }

    bool Config::SaveUserOverrides(const std::string& path) const
    {
        std::ostringstream out;
        for (const std::string& section : m_userOverrides.GetSectionNames())
        {
            out << "[" << section << "]\n";
            for (const std::string& key : m_userOverrides.GetKeysInSection(section))
            {
                const ConfigValue* value = m_userOverrides.Find(section, key);
                if (value != nullptr)
                {
                    out << key << " = " << FormatValue(*value) << "\n";
                }
            }
            out << "\n";
        }

        const std::string text = out.str();
        return FileSystem::Get().WriteEntireFile(path, text.data(), text.size());
    }

    const ConfigValue* Config::Resolve(const std::string& section, const std::string& key) const
    {
        if (const ConfigValue* v = m_userOverrides.Find(section, key)) return v;
        if (const ConfigValue* v = m_gameConfig.Find(section, key)) return v;
        if (const ConfigValue* v = m_engineDefaults.Find(section, key)) return v;
        return nullptr;
    }

    bool Config::GetBool(const std::string& section, const std::string& key, bool defaultValue) const
    {
        const ConfigValue* v = Resolve(section, key);
        if (v == nullptr) return defaultValue;
        if (const bool* b = std::get_if<bool>(v)) return *b;
        if (const i64* i = std::get_if<i64>(v)) return *i != 0;
        return defaultValue;
    }

    i64 Config::GetInt(const std::string& section, const std::string& key, i64 defaultValue) const
    {
        const ConfigValue* v = Resolve(section, key);
        if (v == nullptr) return defaultValue;
        if (const i64* i = std::get_if<i64>(v)) return *i;
        if (const f64* f = std::get_if<f64>(v)) return static_cast<i64>(*f);
        return defaultValue;
    }

    f64 Config::GetFloat(const std::string& section, const std::string& key, f64 defaultValue) const
    {
        const ConfigValue* v = Resolve(section, key);
        if (v == nullptr) return defaultValue;
        if (const f64* f = std::get_if<f64>(v)) return *f;
        if (const i64* i = std::get_if<i64>(v)) return static_cast<f64>(*i);
        return defaultValue;
    }

    std::string Config::GetString(const std::string& section, const std::string& key, const std::string& defaultValue) const
    {
        const ConfigValue* v = Resolve(section, key);
        if (v == nullptr) return defaultValue;
        if (const std::string* s = std::get_if<std::string>(v)) return *s;
        return defaultValue;
    }

    void Config::SetUserOverride(const std::string& section, const std::string& key, ConfigValue value)
    {
        m_userOverrides.Set(section, key, std::move(value));
    }
}
