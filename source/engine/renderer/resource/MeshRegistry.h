// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "renderer/resource/ModelLoader.h"
#include "renderer/resource/GPUMesh.h"
#include "renderer/vulkan/VulkanContext.h"
#include "renderer/vulkan/VulkanMemoryAllocator.h"
#include <unordered_map>
#include <memory>
#include <string>

// ---------------------------------------------------------------------------
// MeshRegistry.h
//
// Owns all GPUMesh instances for the lifetime of the renderer.
// Uploads CookedModel sub-meshes to GPU and returns a stable handle.
//
// Modular rule: only one MeshRegistry per renderer; games never touch GPU
// buffers directly - they only store MeshHandle IDs.
// ---------------------------------------------------------------------------

namespace lacrima::renderer
{
    using MeshHandle = uint32_t;
    static constexpr MeshHandle INVALID_MESH_HANDLE = 0;

    struct GPUSubMesh
    {
        std::unique_ptr<GPUMesh> gpuMesh;
        uint32_t                 materialIndex = 0;
        std::string              name;
    };

    struct GPUModel
    {
        std::vector<GPUSubMesh> subMeshes;
        std::string             sourceFilePath;
    };

    class MeshRegistry
    {
    public:
        MeshRegistry()  = default;
        ~MeshRegistry() = default;

        MeshRegistry(const MeshRegistry&)            = delete;
        MeshRegistry& operator=(const MeshRegistry&) = delete;

        /// Upload a CookedModel to the GPU and return a stable handle.
        /// The handle is valid until Shutdown() or UnloadModel() is called.
        MeshHandle Upload(VulkanContext& ctx,
                          const VulkanMemoryAllocator& allocator,
                          const CookedModel& cookedModel);

        /// Retrieve a GPUModel by handle (nullptr if invalid).
        const GPUModel* Get(MeshHandle handle) const;

        /// Release all GPU resources for a specific model.
        void UnloadModel(VulkanContext& ctx,
                         const VulkanMemoryAllocator& allocator,
                         MeshHandle handle);

        /// Release all GPU resources.
        void Shutdown(VulkanContext& ctx, const VulkanMemoryAllocator& allocator);

    private:
        MeshHandle                               m_nextHandle = 1;
        std::unordered_map<MeshHandle, GPUModel> m_models;
    };

} // namespace lacrima::renderer
