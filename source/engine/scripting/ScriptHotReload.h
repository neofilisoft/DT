#pragma once

#include "core/platform/Types.h"

#include <functional>
#include <string>
#include <vector>

namespace lacrima::script
{
    class ScriptEngine;

    // Watches content scripts and re-executes only changed Lua chunks.
    // Poll from the simulation thread; a ScriptEngine/Lua VM is not thread-safe.
    // This is intentionally transport-agnostic: editor file changes and a
    // verified OTA patch use the same reload path.
    class ScriptHotReload
    {
    public:
        struct ManifestEntry
        {
            std::string path;
            usize sizeBytes = 0;
            u64 contentHash = 0;
        };

        // Stable non-cryptographic content identity for OTA payload checks.
        // Authenticity remains the caller verifier's responsibility.
        static u64 HashPayload(const void* data, usize sizeBytes);
        void Watch(const std::string& path);
        void Unwatch(const std::string& path);
        void Clear();

        // Atomically installs a verified Lua payload into content. This method does not download or verify signatures.
        using Verifier = std::function<bool(const std::string& path, const void* data, usize sizeBytes)>;
        bool InstallFile(const std::string& path, const void* data, usize sizeBytes, const Verifier& verify);
        bool InstallFile(const ManifestEntry& entry, const void* data, usize sizeBytes, const Verifier& verify);

        struct BundlePayload
        {
            ManifestEntry manifest;
            const void* data = nullptr;
            usize sizeBytes = 0;
        };

        // Installs a verified bundle as one transaction. Any failed file restores all prior files.
        bool InstallBundle(const std::vector<BundlePayload>& payloads, const Verifier& verify);

        // Signed-bundle adapter. The engine validates policy and payload hashes;
        // the host supplies the actual Ed25519/ECDSA/etc. verifier.
        struct BundleMetadata
        {
            std::string bundleId;
            std::string channel;
            u64 version = 0;
            u32 minRuntimeVersion = 0;
        };

        struct BundlePolicy
        {
            std::string acceptedChannel;
            u64 installedVersion = 0;
            u32 runtimeVersion = 0;
        };

        using BundleVerifier = std::function<bool(const BundleMetadata&,
            const std::vector<BundlePayload>&, const void* signature, usize signatureBytes)>;
        bool InstallSignedBundle(const BundleMetadata& metadata,
            const std::vector<BundlePayload>& payloads,
            const void* signature, usize signatureBytes,
            const BundleVerifier& verify, const BundlePolicy& policy);
        // Returns the number of scripts successfully reloaded.
        usize Poll(ScriptEngine& engine);

    private:
        struct Entry
        {
            std::string path;
            u64 lastModifiedUnixMs = 0;
            bool knownToExist = false;
        };
        std::vector<Entry> m_entries;
    };
}
