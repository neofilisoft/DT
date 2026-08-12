#pragma once

#include "core/platform/Types.h"
#include "core/platform/Assert.h"
#include "core/filesystem/FileSystem.h"
#include "core/logging/Logger.h"

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

namespace dt
{
    // Opaque handle for any asset type. We use std::shared_ptr as the handle
    // to provide automatic reference counting - when the last Handle goes out
    // of scope, the asset is automatically freed (or unloaded).
    template <typename T>
    using AssetHandle = std::shared_ptr<T>;

    // Abstract base class for different asset types (Texture, Script, etc.)
    class IAsset
    {
    public:
        virtual ~IAsset() = default;
        virtual bool LoadFromFile(const std::string& path) = 0;
    };

    // -----------------------------------------------------------------------
    // AssetManager
    // Central registry for loading and caching assets. Prevents loading the
    // same file twice from disk.
    // -----------------------------------------------------------------------
    class AssetManager
    {
    public:
        static AssetManager& Get()
        {
            static AssetManager instance;
            return instance;
        }

        // Loads an asset of type T from `path`. If it's already loaded,
        // returns the cached handle. T must inherit from IAsset.
        template <typename T>
        AssetHandle<T> LoadAsset(const std::string& path)
        {
            static_assert(std::is_base_of_v<IAsset, T>, "T must inherit from IAsset");

            // Normalize path for consistent caching
            std::string normPath = path;
            // (Simple normalization: could replace '\\' with '/')
            for (char& c : normPath) { if (c == '\\') c = '/'; }

            auto it = m_assetCache.find(normPath);
            if (it != m_assetCache.end())
            {
                // Asset found. Since we store weak_ptrs, check if it's still alive.
                if (auto shared = it->second.lock())
                {
                    return std::static_pointer_cast<T>(shared);
                }
                else
                {
                    // Asset was expired and unloaded, remove dead weak_ptr
                    m_assetCache.erase(it);
                }
            }

            // Not loaded or expired. Load fresh.
            AssetHandle<T> newAsset = std::make_shared<T>();
            if (!newAsset->LoadFromFile(normPath))
            {
                DT_LOG_ERROR(LogCategory::Core, "AssetManager: Failed to load asset '{}'", normPath);
                return nullptr;
            }

            m_assetCache[normPath] = newAsset;
            DT_LOG_INFO(LogCategory::Core, "AssetManager: Loaded asset '{}'", normPath);
            return newAsset;
        }

        // Clears all weak references that have expired (called periodically)
        void GarbageCollect()
        {
            for (auto it = m_assetCache.begin(); it != m_assetCache.end();)
            {
                if (it->second.expired())
                {
                    it = m_assetCache.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

    private:
        AssetManager() = default;
        ~AssetManager() = default;

        AssetManager(const AssetManager&) = delete;
        AssetManager& operator=(const AssetManager&) = delete;

        // Stores weak references. If the rest of the engine drops all shared_ptrs
        // to an asset, it is automatically destroyed, and the weak_ptr here expires.
        std::unordered_map<std::string, std::weak_ptr<IAsset>> m_assetCache;
    };
}
