// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/subsystem/ISubsystem.h"
#include "core/containers/ComponentArray.h"
#include "runtime/Entity.h"
#include "needs/NeedsComponent.h"
#include "relationships/RelationshipGraph.h"
#include "genetics/GeneticsSystem.h"
#include "autonomy/AutonomySystem.h"
#include <array>
#include <memory>

namespace lacrima::sim { class SimulationWorld; }

namespace lacrima::lifesim
{
    // The optional Life Simulation module that handles Needs, Genetics, Relationships, and Autonomy.
    class LifeSimModule : public lacrima::core::ISubsystem
    {
    public:
        LifeSimModule(lacrima::sim::SimulationWorld* world);
        ~LifeSimModule() override;

        void OnInit() override;
        void OnTick(float deltaTime) override;
        void OnCleanup() override;

        lacrima::ComponentArray<lacrima::Entity, lacrima::sim::NeedsComponent>& Needs() { return m_needs; }
        lacrima::sim::RelationshipGraph& Relationships() { return m_relationships; }
        const std::array<lacrima::sim::NeedDefinition, lacrima::sim::kNeedCount>& NeedDefinitions() const { return m_definitions; }
        std::array<lacrima::sim::NeedDefinition, lacrima::sim::kNeedCount>& NeedDefinitions() { return m_definitions; }

    private:
        void StepNeeds(float dt);
        void StepGenetics();
        void StepRelationships();
        void StepAutonomy();

        lacrima::sim::SimulationWorld* m_world;
        lacrima::ComponentArray<lacrima::Entity, lacrima::sim::NeedsComponent> m_needs;
        lacrima::sim::RelationshipGraph m_relationships;
        std::array<lacrima::sim::NeedDefinition, lacrima::sim::kNeedCount> m_definitions{};
        float m_lastDt = 1.0f / 60.0f;
    };
}
