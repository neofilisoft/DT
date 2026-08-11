#include "renderer/graph/RenderGraph.h"

namespace dt::renderer
{
    void RenderGraph::AddPass(std::unique_ptr<RenderPass> pass)
    {
        m_passes.push_back(std::move(pass));
    }

    void RenderGraph::ExecuteAll(VkCommandBuffer cmd)
    {
        for (const auto& pass : m_passes)
        {
            pass->Execute(cmd);
        }
    }

    void RenderGraph::Clear()
    {
        m_passes.clear();
    }
}
