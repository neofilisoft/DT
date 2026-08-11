#pragma once

#include "core/platform/Types.h"
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>

// ---------------------------------------------------------------------------
// RenderGraph.h
//
// A Render Graph architecture for Vulkan (C++20).
// Handles resource transitions and pass scheduling automatically.
// ---------------------------------------------------------------------------

namespace dt::renderer
{
    class VulkanContext;
    class VulkanCommandPool;

    // Concept to ensure pass execution is valid
    template<typename T>
    concept RenderPassExecute = requires(T t, VkCommandBuffer cmd) {
        { t(cmd) } -> std::same_as<void>;
    };

    class RenderPass
    {
    public:
        RenderPass(std::string name) : m_name(std::move(name)) {}
        virtual ~RenderPass() = default;

        virtual void Execute(VkCommandBuffer cmd) = 0;

        const std::string& GetName() const { return m_name; }
    private:
        std::string m_name;
    };

    class RenderGraph
    {
    public:
        RenderGraph() = default;
        ~RenderGraph() = default;

        void AddPass(std::unique_ptr<RenderPass> pass);
        
        // C++20: Adding a pass with a lambda
        template<RenderPassExecute F>
        void AddLambdaPass(std::string name, F&& executeFunc)
        {
            class LambdaPass : public RenderPass
            {
            public:
                LambdaPass(std::string n, F f) : RenderPass(std::move(n)), m_func(std::move(f)) {}
                void Execute(VkCommandBuffer cmd) override { m_func(cmd); }
            private:
                F m_func;
            };
            
            AddPass(std::make_unique<LambdaPass>(std::move(name), std::forward<F>(executeFunc)));
        }

        void ExecuteAll(VkCommandBuffer cmd);
        void Clear();

    private:
        std::vector<std::unique_ptr<RenderPass>> m_passes;
    };
}
