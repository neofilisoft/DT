// Copyright Neofilisoft. All Rights Reserved.
#include "LifeSimModule.h"
#include "simulation/world/SimulationWorld.h"

namespace lacrima::lifesim
{
    LifeSimModule::LifeSimModule(lacrima::sim::SimulationWorld* world)
        : m_world(world)
    {
        // Initialize default NeedDefinitions: drift toward 0 (dissatisfaction) with realistic rates
        m_definitions[static_cast<size_t>(lacrima::sim::NeedId::Hunger)]      = { 0.0f, 100.0f, 0.0f, 0.10f, 0.0f, 1.5f };
        m_definitions[static_cast<size_t>(lacrima::sim::NeedId::Bladder)]     = { 0.0f, 100.0f, 0.0f, 0.15f, 0.0f, 1.2f };
        m_definitions[static_cast<size_t>(lacrima::sim::NeedId::Energy)]      = { 0.0f, 100.0f, 0.0f, 0.08f, 0.0f, 1.0f };
        m_definitions[static_cast<size_t>(lacrima::sim::NeedId::Hygiene)]     = { 0.0f, 100.0f, 0.0f, 0.05f, 0.0f, 0.8f };
        m_definitions[static_cast<size_t>(lacrima::sim::NeedId::Social)]      = { 0.0f, 100.0f, 0.0f, 0.06f, 0.0f, 0.7f };
        m_definitions[static_cast<size_t>(lacrima::sim::NeedId::Fun)]         = { 0.0f, 100.0f, 0.0f, 0.07f, 0.0f, 0.6f };
        m_definitions[static_cast<size_t>(lacrima::sim::NeedId::Comfort)]     = { 0.0f, 100.0f, 0.0f, 0.05f, 0.0f, 0.5f };
        m_definitions[static_cast<size_t>(lacrima::sim::NeedId::Environment)] = { 0.0f, 100.0f, 0.0f, 0.04f, 0.0f, 0.5f };
    }

    LifeSimModule::~LifeSimModule() = default;

    void LifeSimModule::OnInit()
    {
        lacrima::TaskGraph& graph = m_world->GetTickGraph();

        auto& needsNode = graph.AddTask([this]() { StepNeeds(m_lastDt); }, "Needs");
        auto& geneticsNode = graph.AddTask([this]() { StepGenetics(); }, "Genetics");
        auto& relNode = graph.AddTask([this]() { StepRelationships(); }, "Relationships");
        auto& autonomyNode = graph.AddTask([this]() { StepAutonomy(); }, "Autonomy");

        autonomyNode.After(needsNode);
        autonomyNode.After(relNode);
    }

    void LifeSimModule::OnTick(float deltaTime)
    {
        m_lastDt = deltaTime;
    }

    void LifeSimModule::StepNeeds(float dt)
    {
        m_needs.ForEach([this, dt](lacrima::Entity ent, lacrima::sim::NeedsComponent& needs)
        {
            lacrima::sim::DecayNeeds(needs, m_definitions, dt);
        });
    }

    void LifeSimModule::StepGenetics()
    {
        // Genetics inheritance is primarily event-driven when entities reproduce.
    }

    void LifeSimModule::StepRelationships()
    {
        // Relationship decay is lazily evaluated on read via RelationshipGraph::GetScore.
    }

    void LifeSimModule::StepAutonomy()
    {
        // Evaluate autonomy desires for entities based on current needs
        std::vector<lacrima::sim::AutonomyCandidate> candidates;
        m_needs.ForEach([this, &candidates](lacrima::Entity ent, lacrima::sim::NeedsComponent& needs)
        {
            if (!candidates.empty())
            {
                const auto* best = lacrima::sim::SelectBestCandidate(needs, m_definitions, candidates);
                (void)best;
            }
        });
    }

    void LifeSimModule::OnCleanup()
    {
        m_needs.Clear();
    }
}
