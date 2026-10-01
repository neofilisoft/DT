// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/subsystem/ISubsystem.h"
#include "core/containers/ComponentArray.h"
#include "core/jobs/JobSystem.h"
#include "core/platform/Types.h"
#include "runtime/Entity.h"
#include "runtime/EntityAllocator.h"
#include "runtime/SimulationLoop.h"
#include "runtime/SimulationSnapshot.h"
#include "scripting/ScriptEngine.h"
#include "simulation/animation/VisualComponent.h"
#include "simulation/sprite/Sprite2DComponent.h"
#include "simulation/interaction/InteractionQueue.h"
#include "simulation/spatial/InteractableComponent.h"
#include "simulation/spatial/InteractableComponent.h"
#include "simulation/spatial/TransformComponent.h"
#include "simulation/time/SimClock.h"
#include "simulation/navigation/NavigationSystem.h"
#include "simulation/navigation/NavAgentComponent.h"
#include "simulation/spatial/SpatialHashGrid.h"
#include "simulation/spatial/SpatialSystem.h"
#include "simulation/spatial/LODComponent.h"
#include "simulation/spatial/LODSystem.h"
#include "simulation/grid/LotGridComponent.h"
#include "simulation/interaction/RoutingSlotComponent.h"
#include "simulation/spatial/SpringArmComponent.h"
#include "simulation/animation/SkeletalMeshComponent.h"
#include "simulation/animation/FootIKComponent.h"
#include "simulation/animation/VehicleComponent.h"
#include "simulation/world/WorldStreamer.h"
#include "core/serialization/Serialization.h"
#include <array>
#include <functional>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// SimulationWorld.h
//
// Owns every per-module ComponentArray and builds the fixed-shape TaskGraph
// that runs the per-tick module pipeline, executed through the real
// JobSystem work-stealing scheduler (JobSystem::Get().RunGraph), not a
// hand-rolled sequential function-call chain.
//
// Pipeline: Time -> Needs -> Autonomy -> InteractionResolve -> Snapshot.
// This covers every module that has a real implementation right now. It
// deliberately does NOT include placeholder nodes for Relationship,
// Navigation, Animation State, or Object State - adding empty no-op nodes
// for modules that don't exist yet would be exactly the kind of stub this
// project's engine-dev skill forbids. Each of those modules attaches as a
// real new TaskGraph node with a `.After(...)` edge into this same graph
// when it is actually built; the graph-construction code below is written
// so that insertion is additive (see the comments at each node call site
// marking where the next stage's dependency edge will attach).
//
// DETERMINISM: the graph is built ONCE in the constructor (not rebuilt
// every tick) and reused across ticks via graph.Reset() +
// JobSystem::RunGraph, matching JobSystem.h's documented "static graph
// shape, reused every tick" pattern.
//
// LUA INTEGRATION: SimulationWorld now owns a
// script::ScriptEngine and drives InteractionDef::luaCheckFunction /
// luaRunFunction for real through it. StepAutonomy calls the Lua check
// function to gate whether a candidate is even offered; StepInteractionResolve
// drives the front queued interaction's real Lua coroutine via
// InteractionQueue::StepFront (see InteractionQueue.h/ScriptCoroutine.h) -
// this replaces the old "instant resolution by matching
// InteractionDef::name in C++" placeholder entirely; there is no longer any
// name-string special-casing of "Rest"/"GrabASnack" anywhere in this class.
//
// Entity is registered as a sol2 usertype (RegisterLuaBindings) so Lua
// scripts receive real Entity handles as opaque objects and pass them back
// into bound API functions (dt.get_need / dt.satisfy_need) without any
// bit-packing glue.
// ---------------------------------------------------------------------------

namespace lacrima::physics { class IPhysicsSystem; }

namespace lacrima::sim
{
    // A single always-available interaction: not gated by proximity to any
    // object, since spatial/object discovery does not exist yet (the
    // explicitly separate). This is a real, honest subset
    // of the full interaction-availability model - "things a Sim can
    // always do regardless of location" - not a stand-in for the spatial
    // system. Once spatial/object discovery exists, AutonomySystem's
    // candidate list for a given entity becomes GlobalInteractions +
    // nearby object interactions, combined; this struct remains valid as
    // the "always available" half of that union rather than being
    // deleted/replaced.

    class SimulationWorld
    {
    public:
        explicit SimulationWorld(usize initialEntityCount = 12);

        // Matches lacrima::SimTickFunc exactly - hand `world.MakeTickFunc()` to
        // an Application the same way M1's Game::MakeTickFunc() worked.
        void Tick(u64 tickIndex, f64 fixedDeltaSeconds, SimSnapshot& outSnapshot);
        SimTickFunc MakeTickFunc();

        TaskGraph& GetTickGraph() { return m_tickGraph; }
        void FinalizeTickGraph();

        // M13: Serialization
        void SaveState(lacrima::BinaryWriter& writer) const;
        bool LoadState(lacrima::BinaryReader& reader);

        // State Cloning & Snapshotting (PIE Isolation & Replay)
        void CopyFrom(const SimulationWorld& other);
        std::unique_ptr<SimulationWorld> Clone() const;
        void BuildRenderSnapshot(SimSnapshot& outSnapshot);

        // M13: ZSTD Compressed File I/O
        bool SaveToFile(const std::string& path) const;
        bool LoadFromFile(const std::string& path);

        usize LiveEntityCount() const { return m_entities.LiveCount(); }
        const SimClock& Clock() const { return m_clock; }

        // Exposed for tests/tools that want to inspect a specific entity's
        // state directly without going through a snapshot.
        Entity CreateEntity();
        void DestroyEntity(Entity entity);
        TransformComponent* GetTransform(Entity entity) { return m_transforms.Get(entity); }
        InteractableComponent* GetInteractable(Entity entity) { return m_interactables.Get(entity); }

        ComponentArray<Entity, TransformComponent>& Transforms() { return m_transforms; }
        ComponentArray<Entity, InteractableComponent>& Interactables() { return m_interactables; }
        ComponentArray<Entity, VisualComponent>& Visuals() { return m_visuals; }
        ComponentArray<Entity, Sprite2DComponent>& Sprites() { return m_sprites; }
        ComponentArray<Entity, NavAgentComponent>& NavAgents() { return m_navAgents; }
        ComponentArray<Entity, LODComponent>& LODs() { return m_lods; }
        ComponentArray<Entity, LotGridComponent>& LotGrids() { return m_lotGrids; }
        ComponentArray<Entity, RoutingSlotComponent>& RoutingSlots() { return m_routingSlots; }
        ComponentArray<Entity, SpringArmComponent>& SpringArms() { return m_springArms; }
        ComponentArray<Entity, SkeletalMeshComponent>& SkeletalMeshes() { return m_skeletalMeshes; }
        ComponentArray<Entity, FootIKComponent>& FootIKs() { return m_footIKs; }
        ComponentArray<Entity, VehicleComponent>& Vehicles() { return m_vehicles; }

        void SetPhysicsSystem(lacrima::physics::IPhysicsSystem* ps) { m_physicsSystem = ps; }
        lacrima::physics::IPhysicsSystem* GetPhysicsSystem() const { return m_physicsSystem; }

        NavigationSystem& GetNavigationSystem() { return m_navigationSystem; }
        WorldStreamer& Streamer() { return m_worldStreamer; }
        const WorldStreamer& Streamer() const { return m_worldStreamer; }
        LODSystem& GetLODSystem() { return m_lodSystem; }

        script::ScriptEngine& Scripting() { return m_scriptEngine; }

        // Optional audio callback injected by game layer.
        // Simulation does not link against dt_audio - instead the game or
        // application layer calls SetPlaySoundCallback once at startup to
        // wire in whatever audio backend it uses. If not set, play_sound
        // calls from Lua scripts are silently ignored.
        using PlaySoundCallback = std::function<void(const std::string&)>;
        using PlaySoundVolumeCallback = std::function<void(const std::string&, f32)>;
        void SetPlaySoundVolumeCallback(PlaySoundVolumeCallback cb) { m_playSoundVolumeCallback = std::move(cb); }        void SetPlaySoundCallback(PlaySoundCallback cb) { m_playSoundCallback = std::move(cb); }

    private:
        std::vector<std::unique_ptr<lacrima::core::ISubsystem>> m_subsystems;
        void BuildTickGraph();
        void RegisterLuaBindings();

        // Per-node step implementations. Each iterates its own
        // ComponentArray directly (see ComponentArray.h's file comment on
        // why this is the cache-friendly bulk-iteration path) - these are
        // called from inside TaskGraph node lambdas, not exposed as public
        // API, since their correct call order is entirely owned by the
        // graph wiring in BuildTickGraph.
        void StepTime();
        void StepNavigation();
        void BuildSnapshot(SimSnapshot& outSnapshot);

        EntityAllocator m_entities;
        SimClock m_clock;
        script::ScriptEngine m_scriptEngine;
        ComponentArray<Entity, TransformComponent> m_transforms;
        ComponentArray<Entity, InteractableComponent> m_interactables;
        ComponentArray<Entity, VisualComponent> m_visuals;
        ComponentArray<Entity, Sprite2DComponent> m_sprites;
        ComponentArray<Entity, NavAgentComponent> m_navAgents;
        ComponentArray<Entity, LODComponent> m_lods;
        ComponentArray<Entity, LotGridComponent> m_lotGrids;
        ComponentArray<Entity, RoutingSlotComponent> m_routingSlots;
        ComponentArray<Entity, SpringArmComponent> m_springArms;
        ComponentArray<Entity, SkeletalMeshComponent> m_skeletalMeshes;
        ComponentArray<Entity, FootIKComponent> m_footIKs;
        ComponentArray<Entity, VehicleComponent> m_vehicles;
        lacrima::physics::IPhysicsSystem* m_physicsSystem = nullptr;
        SpatialHashGrid m_spatialGrid;
        WorldStreamer m_worldStreamer;

        NavigationSystem m_navigationSystem;
        LODSystem m_lodSystem;

        TaskGraph m_tickGraph;

        // Optional callback for dt_engine.play_sound() from Lua
        // (set by game layer via SetPlaySoundCallback).
        PlaySoundCallback m_playSoundCallback;
        PlaySoundVolumeCallback m_playSoundVolumeCallback;

        // Set by Tick() immediately before RunGraph, read by node lambdas
        // during graph execution - see .cpp Tick() comment on why the
        // fixed-shape reused graph reads mutable members rather than
        // capturing tick-specific values at graph-build time.
        f32 m_currentFixedDeltaSeconds = 0.0f;
        SimSnapshot* m_currentOutSnapshot = nullptr;
        u64 m_pendingTickIndex = 0;
    };
}











