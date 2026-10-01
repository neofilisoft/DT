#include "scripting/ScriptHotReload.h"
#include "scripting/ScriptEngine.h"
#include "core/filesystem/FileSystem.h"

#include <algorithm>

namespace lacrima::script
{
    namespace
    {
        bool IsSafeLuaPath(const std::string& path)
        {
            if (path.empty() || path.front() == '/' || path.front() == '\\' || path.find(':') != std::string::npos)
                return false;

            std::string segment;
            for (usize i = 0; i <= path.size(); ++i)
            {
                const char c = i < path.size() ? path[i] : '/';
                if (c == '/' || c == '\\')
                {
                    if (segment == ".." || segment.empty())
                        return segment.empty() && i == path.size();
                    segment.clear();
                }
                else
                {
                    segment.push_back(c);
                }
            }
            return true;
        }
    }
    void ScriptHotReload::Watch(const std::string& path)
    {
        const auto found = std::find_if(m_entries.begin(), m_entries.end(),
            [&](const Entry& item) { return item.path == path; });
        if (found != m_entries.end())
            return;

        const FileStats stats = FileSystem::Get().Stat(path);
        m_entries.push_back({path, stats.lastModifiedUnixMs, stats.exists});
    }

    void ScriptHotReload::Unwatch(const std::string& path)
    {
        m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(),
            [&](const Entry& item) { return item.path == path; }), m_entries.end());
    }

    void ScriptHotReload::Clear()
    {
        m_entries.clear();
    }

    u64 ScriptHotReload::HashPayload(const void* data, usize sizeBytes)
    {
        constexpr u64 kOffset = 1469598103934665603ull;
        constexpr u64 kPrime = 1099511628211ull;
        const auto* bytes = static_cast<const u8*>(data);
        u64 hash = kOffset;
        for (usize i = 0; i < sizeBytes; ++i)
        {
            hash ^= static_cast<u64>(bytes[i]);
            hash *= kPrime;
        }
        return hash;
    }

    bool ScriptHotReload::InstallFile(const ManifestEntry& entry, const void* data, usize sizeBytes, const Verifier& verify)
    {
        if (entry.path.empty() || entry.sizeBytes != sizeBytes || entry.contentHash != HashPayload(data, sizeBytes))
            return false;
        return InstallFile(entry.path, data, sizeBytes, verify);
    }

    bool ScriptHotReload::InstallFile(const std::string& path, const void* data, usize sizeBytes, const Verifier& verify)
    {
        if (!IsSafeLuaPath(path) || FileSystem::Get().GetExtension(path) != ".lua" || !verify || !verify(path, data, sizeBytes))
            return false;
        return FileSystem::Get().WriteEntireFileAtomic(path, data, sizeBytes);
    }
    bool ScriptHotReload::InstallBundle(const std::vector<BundlePayload>& payloads, const Verifier& verify)
    {
        if (payloads.empty() || !verify)
            return false;

        std::vector<std::string> paths;
        paths.reserve(payloads.size());
        for (const BundlePayload& payload : payloads)
        {
            if (!IsSafeLuaPath(payload.manifest.path) || std::find(paths.begin(), paths.end(), payload.manifest.path) != paths.end()
                || (payload.data == nullptr && payload.sizeBytes != 0)
                || payload.manifest.sizeBytes != payload.sizeBytes
                || payload.manifest.contentHash != HashPayload(payload.data, payload.sizeBytes)
                || !verify(payload.manifest.path, payload.data, payload.sizeBytes))
                return false;
            paths.push_back(payload.manifest.path);
        }

        struct Backup
        {
            std::string path;
            bool existed = false;
            std::vector<u8> bytes;
        };
        std::vector<Backup> backups;
        backups.reserve(payloads.size());
        for (const BundlePayload& payload : payloads)
        {
            Backup backup;
            backup.path = payload.manifest.path;
            backup.existed = FileSystem::Get().Exists(backup.path);
            if (backup.existed)
            {
                auto bytes = FileSystem::Get().ReadEntireFile(backup.path);
                if (!bytes.has_value())
                    return false;
                backup.bytes = std::move(*bytes);
            }
            backups.push_back(std::move(backup));
        }

        usize installed = 0;
        for (const BundlePayload& payload : payloads)
        {
            if (!InstallFile(payload.manifest, payload.data, payload.sizeBytes, verify))
            {
                for (usize i = 0; i < installed; ++i)
                {
                    const Backup& backup = backups[i];
                    if (backup.existed)
                        FileSystem::Get().WriteEntireFileAtomic(backup.path, backup.bytes.data(), backup.bytes.size());
                    else
                        FileSystem::Get().RemoveFile(backup.path);
                }
                return false;
            }
            ++installed;
        }
        return true;
    }

    bool ScriptHotReload::InstallSignedBundle(const BundleMetadata& metadata,
        const std::vector<BundlePayload>& payloads,
        const void* signature, usize signatureBytes,
        const BundleVerifier& verify, const BundlePolicy& policy)
    {
        if (metadata.bundleId.empty() || metadata.channel.empty()
            || payloads.empty() || signature == nullptr || signatureBytes == 0 || !verify)
            return false;
        if ((!policy.acceptedChannel.empty() && metadata.channel != policy.acceptedChannel)
            || metadata.version <= policy.installedVersion
            || (policy.runtimeVersion != 0 && metadata.minRuntimeVersion > policy.runtimeVersion))
            return false;

        // The host verifier should authenticate a canonical representation of
        // metadata, manifests, payload hashes, and the detached signature.
        if (!verify(metadata, payloads, signature, signatureBytes))
            return false;

        // ManifestEntry hashes/sizes and the transactional writer still run
        // after signature verification. The opaque signature is intentionally
        // not interpreted by the engine; the host can use Ed25519, ECDSA, or
        // a platform keystore without changing this runtime module.
        return InstallBundle(payloads,
            [](const std::string&, const void*, usize) { return true; });
    }
    usize ScriptHotReload::Poll(ScriptEngine& engine)
    {
        usize reloaded = 0;
        for (Entry& entry : m_entries)
        {
            const FileStats stats = FileSystem::Get().Stat(entry.path);
            if (!stats.exists)
            {
                entry.knownToExist = false;
                continue;
            }

            const bool changed = !entry.knownToExist ||
                                  stats.lastModifiedUnixMs != entry.lastModifiedUnixMs;
            if (!changed)
                continue;

            // Execute the replacement only after the file exists completely.
            // The caller should stage downloads atomically before this poll.
            if (engine.LoadFile(entry.path))
            {
                ++reloaded;
                entry.lastModifiedUnixMs = stats.lastModifiedUnixMs;
                entry.knownToExist = true;
            }
        }
        return reloaded;
    }
}
