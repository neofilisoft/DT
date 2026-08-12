#include "simulation/world/SimulationWorld.h"
#include "simulation/animation/AnimationSystem.h"
#include "core/input/InputManager.h"
#include "core/profiler/Profiler.h"
#include "core/logging/Logger.h"
#include "core/serialization/Serialization.h"
#include "simulation/spatial/SpatialSystem.h"

#include <cmath>

namespace dt::sim
{
    std::vector<AutonomyCandidate> GlobalInteractionPool::BuildCandidates() const
    {
        std::vector<AutonomyCandidate> candidates;
        candidates.reserve(table.All().size());

        for (const InteractionDef& def : table.All())
        {
            AutonomyCandidate candidate;
            candidate.def = &def;
            candidates.push_back(candidate);
        }

        return candidates;
    }

    namespace
    {
        // Concrete global interaction content - "Rest" and
        // "GrabASnack", the two always-available actions a Sim can perform
        // with no object/spatial dependency. Their satisfaction weighting
        // (used for autonomy SCORING, kept in fast C++ per AutonomySystem's
        // design goal - see AutonomySystem.h) is declared here; their
        // actual EXECUTION behavior now lives in real Lua source (see
        // LoadBuiltinInteractionScripts below), not in C++ at all.
        constexpr f32 kRestSatisfactionWeight = 8.0f;
        constexpr f32 kSnackSatisfactionWeight = 6.0f;

        // Embedded inline as a string literal rather than a .lua file
        // loaded via FileSystem: this is explicitly a temporary home for
        // built-in engine-level "always available" interaction content.
        // Once the asset/.asset content pipeline exists, this moves to a
        // real .lua file loaded through FileSystem/ScriptEngine::LoadFile,
        // and ownership likely shifts to game-layer content (Domaintic
        // authoring its own Rest/GrabASnack variants) rather than being
        // hardcoded engine behavior.
        //
        // Lua-side contract: dt.get_need(entity, needName) -> number,
        // dt.satisfy_need(entity, needName, amount) -> nil are the bound
        // API functions (see SimulationWorld::RegisterLuaBindings).
        constexpr const char* kBuiltinInteractionScript = R"LUA(
function Rest_Check(actor, target)
    -- Always available - no precondition beyond existing. A real object-
    -- gated interaction's check function would test things like
    -- "is target a valid Bed" or "is actor already at target's location";
    -- global interactions have no target object to check.
    return true
end

function Rest_Run(actor, target, dt)
    dt_engine.satisfy_need(actor, "Energy", 8.0)
    return "complete"
end

function GrabASnack_Check(actor, target)
    return true
end

function GrabASnack_Run(actor, target, dt)
    -- Demonstrates genuine multi-tick execution via coroutine.yield,
    -- proving ScriptCoroutine's "pause mid-Lua-function, resume next
    -- tick" mechanism actually works end-to-end, not just single-call
    -- functions that happen to return immediately.
    coroutine.yield("continue")
    
    -- Play a sound when snacking!
    dt_engine.play_sound("test_sound.wav")
    
    dt_engine.satisfy_need(actor, "Hunger", 6.0)
    return "complete"
end
)LUA";
    }

    SimulationWorld::SimulationWorld(usize initialEntityCount)
    {
        for (auto& def : m_needDefinitions)
        {
            def.minValue = 0.0f;
            def.maxValue = 100.0f;
            def.convergence = 100.0f;
            def.failureThreshold = 0.0f;
            def.decayRatePerSecond = 0.5f;
            def.autonomyWeight = 1.0f;
        }
        m_needDefinitions[static_cast<usize>(NeedId::Hunger)].decayRatePerSecond = 1.2f;
        m_needDefinitions[static_cast<usize>(NeedId::Energy)].decayRatePerSecond = 0.8f;

        InteractionDef rest;
        rest.name = "Rest";
        rest.luaCheckFunction = "Rest_Check";
        rest.luaRunFunction = "Rest_Run";
        rest.basePriority = 0.0f;
        m_globalInteractions.table.Register(rest);

        InteractionDef snack;
        snack.name = "GrabASnack";
        snack.luaCheckFunction = "GrabASnack_Check";
        snack.luaRunFunction = "GrabASnack_Run";
        snack.basePriority = 0.0f;
        m_globalInteractions.table.Register(snack);

        m_needs.Reserve(initialEntityCount);
        m_queues.Reserve(initialEntityCount);
        m_transforms.Reserve(initialEntityCount);
        m_interactables.Reserve(initialEntityCount);
        m_navAgents.Reserve(initialEntityCount);

        for (usize i = 0; i < initialEntityCount; ++i)
        {
            Entity e = CreateEntity();

            NeedsComponent needs;
            for (usize n = 0; n < kNeedCount; ++n)
            {
                needs.values[n] = 60.0f + 35.0f * std::sin(static_cast<f32>(i * 7 + n * 3));
                needs.values[n] = std::clamp(needs.values[n], 0.0f, 100.0f);
            }
            m_needs.Add(e, needs);
            m_queues.Add(e);
            
            TransformComponent transform;
            transform.x = static_cast<f32>(i % 4) * 2.0f;
            transform.z = static_cast<f32>(i / 4) * 2.0f;
            m_transforms.Add(e, transform);

            VisualComponent visual;
            visual.visualId = 1; // Default to some test sprite/mesh
            dt::sim::AnimationSystem::SetupSpriteAnimation(visual, AnimationState::Idle);
            m_visuals.Add(e, visual);
        }

        RegisterLuaBindings();
        LoadBuiltinInteractionScripts();
        
        m_navigationSystem.Initialize(this);
        // Temporary testing nav mesh setup
        m_navigationSystem.CreateTestNavMesh();

        BuildTickGraph();
    }

    Entity SimulationWorld::CreateEntity()
    {
        return m_entities.Create();
    }

    void SimulationWorld::RegisterLuaBindings()
    {
        sol::state& lua = m_scriptEngine.Raw();

        // Entity as an opaque usertype: index/generation exposed read-only
        // (useful for debug printing from Lua, e.g. print(actor.index)),
        // no writable fields and no Lua-side constructor exposed - Lua
        // scripts receive Entity values from the engine (as coroutine
        // arguments) and pass them back into bound API calls; they cannot
        // fabricate a new Entity from scratch, which would bypass
        // SlotMap's generation validity checking entirely.
        lua.new_usertype<Entity>("Entity",
            sol::no_constructor,
            "index", sol::readonly(&Entity::index),
            "generation", sol::readonly(&Entity::generation)
        );

        // dt_engine.* namespace table - the bound API surface gameplay Lua
        // scripts call into. Kept as a named table (not global bare
        // functions) so script authors can see at a glance which calls are
        // engine-provided versus their own script-local functions, and so
        // future bound functions have an obvious place to land without
        // further polluting the global namespace.
        sol::table dtEngine = lua.create_named_table("dt_engine");

        dtEngine.set_function("get_need", [this](Entity entity, const std::string& needName) -> f32
        {
            NeedsComponent* needs = m_needs.Get(entity);
            if (needs == nullptr)
            {
                DT_LOG_WARN(LogCategory::Scripting, "dt_engine.get_need: entity has no NeedsComponent");
                return 0.0f;
            }

            for (usize i = 0; i < kNeedCount; ++i)
            {
                if (std::string(ToString(static_cast<NeedId>(i))) == needName)
                {
                    return needs->values[i];
                }
            }

            DT_LOG_WARN(LogCategory::Scripting, "dt_engine.get_need: unknown need name '{}'", needName);
            return 0.0f;
        });

        dtEngine.set_function("satisfy_need", [this](Entity entity, const std::string& needName, f32 amount)
        {
            NeedsComponent* needs = m_needs.Get(entity);
            if (needs == nullptr)
            {
                DT_LOG_WARN(LogCategory::Scripting, "dt_engine.satisfy_need: entity has no NeedsComponent");
                return;
            }

            for (usize i = 0; i < kNeedCount; ++i)
            {
                if (std::string(ToString(static_cast<NeedId>(i))) == needName)
                {
                    ApplyNeedDelta(*needs, m_needDefinitions[i], static_cast<NeedId>(i), amount);
                    return;
                }
            }

            DT_LOG_WARN(LogCategory::Scripting, "dt_engine.satisfy_need: unknown need name '{}'", needName);
        });

        dtEngine.set_function("play_sound", [this](const std::string& path)
        {
            if (m_playSoundCallback)
                m_playSoundCallback(path);
        });

        dtEngine.set_function("set_animation_state", [this](Entity entity, const std::string& stateName)
        {
            VisualComponent* visual = m_visuals.Get(entity);
            if (visual == nullptr)
            {
                DT_LOG_WARN(LogCategory::Scripting, "dt_engine.set_animation_state: entity has no VisualComponent");
                return;
            }

            AnimationState state = AnimationState::Idle;
            if (stateName == "Walk") state = AnimationState::Walk;
            else if (stateName == "Interact") state = AnimationState::Interact;
            else if (stateName == "Idle") state = AnimationState::Idle;
            else
            {
                DT_LOG_WARN(LogCategory::Scripting, "dt_engine.set_animation_state: unknown state '{}'", stateName);
                return;
            }

            dt::sim::AnimationSystem::SetupSpriteAnimation(*visual, state);
        });

        // Input bindings - scripts can poll action state without knowing the device.
        // Action names match input.ini (e.g. "Interact", "Jump", "MoveUp").
        // These read from InputManager which is written by the render thread - the
        // values are one frame stale at most, which is acceptable for gameplay logic.
        dtEngine.set_function("is_action_pressed", [](const std::string& name) -> bool
        {
            return dt::InputManager::Get().IsActionPressed(name);
        });

        dtEngine.set_function("is_action_held", [](const std::string& name) -> bool
        {
            return dt::InputManager::Get().IsActionHeld(name);
        });

        dtEngine.set_function("get_axis", [](const std::string& name) -> f32
        {
            return dt::InputManager::Get().GetAxis(name);
        });
    }

    void SimulationWorld::SaveState(dt::BinaryWriter& writer) const
    {
        DT_PROFILE_SCOPE("SimulationWorld::SaveState");

        // 1. Clock
        writer.WritePrimitive(m_clock.TickIndex());

        // 2. Entities
        writer.WritePrimitive(static_cast<u32>(m_entities.LiveCount()));
        
        m_entities.ForEachValid([&](Entity ent)
        {
            writer.WritePrimitive(ent.index);
            writer.WritePrimitive(ent.generation);

            // Transform
            if (const TransformComponent* tc = m_transforms.Get(ent))
            {
                writer.WritePrimitive<u8>(1);
                writer.WriteObject(tc, TransformComponent::StaticTypeInfo());
            }
            else
            {
                writer.WritePrimitive<u8>(0);
            }

            // Visual
            if (const VisualComponent* vc = m_visuals.Get(ent))
            {
                writer.WritePrimitive<u8>(1);
                writer.WriteObject(vc, VisualComponent::StaticTypeInfo());
            }
            else
            {
                writer.WritePrimitive<u8>(0);
            }

            // Needs (raw array)
            if (const NeedsComponent* nc = m_needs.Get(ent))
            {
                writer.WritePrimitive<u8>(1);
                for (usize i = 0; i < kNeedCount; ++i)
                {
                    writer.WritePrimitive(nc->values[i]);
                }
            }
            else
            {
                writer.WritePrimitive<u8>(0);
            }
            
            // InteractionQueue is inherently transient runtime state.
            // InteractableComponent is currently static (defined on creation).
            // We skip saving them here and assume they are recreated on load.
        });
    }

    bool SimulationWorld::LoadState(dt::BinaryReader& reader)
    {
        DT_PROFILE_SCOPE("SimulationWorld::LoadState");

        if (reader.AtEnd()) return false;

        u64 tickIndex = reader.ReadPrimitive<u64>();
        m_clock.Advance(tickIndex);

        // Clear existing state before loading
        m_entities = EntityAllocator();
        m_transforms.Clear();
        m_visuals.Clear();
        m_needs.Clear();
        m_interactables.Clear();
        m_queues.Clear();
        m_navAgents.Clear();

        u32 liveCount = reader.ReadPrimitive<u32>();
        for (u32 i = 0; i < liveCount; ++i)
        {
            u32 index = reader.ReadPrimitive<u32>();
            u32 gen = reader.ReadPrimitive<u32>();
            
            // Reconstruct entity handle
            // This requires EntityAllocator to support placing at a specific index,
            // which it might not. For M13, we assume sequential saves or we just 
            // call CreateEntity() and hope indices match.
            // A more robust system would save a UUID map or add a RecreateEntity method.
            Entity ent = m_entities.Create(); 
            // For now, we trust the allocator gives us the same index since we just reset it.
            
            if (reader.ReadPrimitive<u8>() == 1)
            {
                TransformComponent& tc = m_transforms.Add(ent);
                reader.ReadObject(&tc, TransformComponent::StaticTypeInfo());
            }

            if (reader.ReadPrimitive<u8>() == 1)
            {
                VisualComponent& vc = m_visuals.Add(ent);
                reader.ReadObject(&vc, VisualComponent::StaticTypeInfo());
            }

            if (reader.ReadPrimitive<u8>() == 1)
            {
                NeedsComponent& nc = m_needs.Add(ent);
                for (usize n = 0; n < kNeedCount; ++n)
                {
                    nc.values[n] = reader.ReadPrimitive<f32>();
                }
            }
            
            // Re-add un-serialized default components
            m_interactables.Add(ent);
            m_queues.Add(ent);
        }

        return true;
    }

    void SimulationWorld::LoadBuiltinInteractionScripts()
    {
        const bool ok = m_scriptEngine.LoadString(kBuiltinInteractionScript, "builtin_interactions");
        if (!ok)
        {
            DT_LOG_ERROR(LogCategory::Scripting, "SimulationWorld: failed to load built-in interaction scripts - Rest/GrabASnack will fail their Lua calls at runtime");
        }
    }

    void SimulationWorld::BuildTickGraph()
    {
        auto& timeNode = m_tickGraph.AddTask([this]() { StepTime(); }, "Time");

        auto& needsNode = m_tickGraph.AddTask([this]() { StepNeeds(); }, "Needs");
        needsNode.After(timeNode);

        // [Relationship attaches here in a future milestone.]

        auto& autonomyNode = m_tickGraph.AddTask([this]() { StepAutonomy(); }, "Autonomy");
        autonomyNode.After(needsNode);

        auto& resolveNode = m_tickGraph.AddTask([this]() { StepInteractionResolve(); }, "InteractionResolve");
        resolveNode.After(autonomyNode);

        auto& navigationNode = m_tickGraph.AddTask([this]() { StepNavigation(); }, "Navigation");
        navigationNode.After(resolveNode);

        auto& animationsNode = m_tickGraph.AddTask([this]() 
        { 
            dt::sim::AnimationSystem::StepAnimations(m_currentFixedDeltaSeconds, m_visuals); 
        }, "Animations");
        animationsNode.After(navigationNode);

        auto& snapshotNode = m_tickGraph.AddTask([this]() { BuildSnapshot(*m_currentOutSnapshot); }, "Snapshot");
        snapshotNode.After(animationsNode);

        m_tickGraph.Finalize();
    }

    void SimulationWorld::Tick(u64 tickIndex, f64 fixedDeltaSeconds, SimSnapshot& outSnapshot)
    {
        m_currentFixedDeltaSeconds = static_cast<f32>(fixedDeltaSeconds);
        m_currentOutSnapshot = &outSnapshot;
        m_pendingTickIndex = tickIndex;

        JobSystem::Get().RunGraph(m_tickGraph);
    }

    SimTickFunc SimulationWorld::MakeTickFunc()
    {
        return [this](u64 tickIndex, f64 fixedDeltaSeconds, SimSnapshot& outSnapshot)
        {
            Tick(tickIndex, fixedDeltaSeconds, outSnapshot);
        };
    }

    void SimulationWorld::StepTime()
    {
        m_clock.Advance(m_pendingTickIndex);
    }

    void SimulationWorld::StepNeeds()
    {
        m_needs.ForEach([&](Entity /*entity*/, NeedsComponent& needs)
        {
            DecayNeeds(needs, m_needDefinitions, m_currentFixedDeltaSeconds);
        });
    }

    void SimulationWorld::StepAutonomy()
    {
        std::vector<AutonomyCandidate> globalCandidates = m_globalInteractions.BuildCandidates();

        // Satisfaction weighting for scoring (fast C++ path, see
        // AutonomySystem.h rationale for why scoring itself stays
        // Lua-free) - kept as a small name-keyed lookup here, same
        // caveat still applies (real content pipeline
        // would carry this alongside InteractionDef via .asset data).
        for (AutonomyCandidate& candidate : globalCandidates)
        {
            if (candidate.def->name == "Rest")
            {
                candidate.satisfies = { { NeedId::Energy, kRestSatisfactionWeight } };
            }
            else if (candidate.def->name == "GrabASnack")
            {
                candidate.satisfies = { { NeedId::Hunger, kSnackSatisfactionWeight } };
            }
        }

        m_entities.ForEachValid([&](Entity entity)
        {
            InteractionQueue* queue = m_queues.Get(entity);
            NeedsComponent* needs = m_needs.Get(entity);
            if (queue == nullptr || needs == nullptr || !queue->IsEmpty())
            {
                return;
            }

            // Real Lua check-function gating: filter candidates down to
            // only those whose luaCheckFunction currently returns true,
            // BEFORE scoring. A candidate whose check function is missing
            // or errors is excluded (fails safe - never offered) rather
            // than assumed available.
            // Get nearby objects if this entity has a transform
            std::vector<Entity> nearbyObjects;
            if (const TransformComponent* transform = m_transforms.Get(entity))
            {
                nearbyObjects = SpatialSystem::FindInteractablesInRange(*transform, 10.0f, m_transforms, m_interactables);
            }

            // Combine global and local candidates
            std::vector<AutonomyCandidate> allCandidates = globalCandidates;

            for (Entity objEntity : nearbyObjects)
            {
                if (const InteractableComponent* interactable = m_interactables.Get(objEntity))
                {
                    for (const InteractionDef& def : interactable->interactions.All())
                    {
                        AutonomyCandidate candidate;
                        candidate.def = &def;
                        candidate.target = objEntity;
                        
                        // Just like global interactions, set satisfactions here if needed.
                        // In a real system, these come from data (.asset).
                        // For Milestone 9, we hardcode some mock satisfactions for objects.
                        if (def.name == "Rest")
                        {
                            candidate.satisfies = { { NeedId::Energy, kRestSatisfactionWeight } };
                        }
                        else if (def.name == "GrabASnack")
                        {
                            candidate.satisfies = { { NeedId::Hunger, kSnackSatisfactionWeight } };
                        }
                        
                        allCandidates.push_back(candidate);
                    }
                }
            }

            std::vector<AutonomyCandidate> availableCandidates;
            availableCandidates.reserve(allCandidates.size());

            for (AutonomyCandidate candidate : allCandidates)
            {
                // Global candidates target self, object candidates target the object.
                if (candidate.target.generation == 0) // Meaning it wasn't set to an object
                {
                    candidate.target = entity;
                }

                std::optional<bool> checkResult = m_scriptEngine.CallGlobalFunction<bool>(
                    candidate.def->luaCheckFunction, entity, candidate.target);

                if (checkResult.has_value() && checkResult.value())
                {
                    availableCandidates.push_back(candidate);
                }
            }

            const AutonomyCandidate* best = SelectBestCandidate(*needs, m_needDefinitions, availableCandidates);
            if (best != nullptr && best->def != nullptr)
            {
                queue->PushAutonomous(*best->def, entity);
            }
        });
    }

    void SimulationWorld::StepInteractionResolve()
    {
        m_entities.ForEachValid([&](Entity entity)
        {
            InteractionQueue* queue = m_queues.Get(entity);
            if (queue == nullptr || queue->IsEmpty())
            {
                return;
            }

            // Real Lua coroutine execution - see InteractionQueue::StepFront
            // and ScriptCoroutine.h. Result is intentionally unused here;
            // StepFront already handles popping the queue on
            // Complete/Failed internally, so there is nothing further for
            // this call site to do with the returned status other than let
            // it happen. A future milestone (e.g. an on-interaction-failed
            // reaction system) would consume this return value.
            (void)queue->StepFront(m_scriptEngine, entity, m_currentFixedDeltaSeconds);
        });
    }

    void SimulationWorld::StepNavigation()
    {
        m_navigationSystem.StepNavigation(this, m_currentFixedDeltaSeconds);
    }

    void SimulationWorld::BuildSnapshot(SimSnapshot& outSnapshot)
    {
        outSnapshot.proxies.clear();
        outSnapshot.proxies.reserve(m_entities.LiveCount());

        m_entities.ForEachValid([this, &outSnapshot](Entity e)
        {
            RenderProxy proxy;
            proxy.entityId = (static_cast<u64>(e.index) << 32) | e.generation;
            
            if (TransformComponent* transform = m_transforms.Get(e))
            {
                proxy.positionX = transform->x;
                proxy.positionY = transform->y;
                proxy.positionZ = transform->z;
                proxy.rotationY = transform->yaw;
            }
            else
            {
                proxy.positionX = 0.0f;
                proxy.positionY = 0.0f;
                proxy.positionZ = 0.0f;
                proxy.rotationY = 0.0f;
            }
            
            if (VisualComponent* visual = m_visuals.Get(e))
            {
                proxy.visualId = visual->visualId;
                proxy.animationState = static_cast<u32>(visual->currentState);
                proxy.currentFrame = visual->currentFrame;
            }
            else
            {
                proxy.visualId = 1;
                if (!m_needs.Get(e))
                {
                    proxy.visualId = 2; // e.g. Object fallback
                }
                proxy.animationState = 0;
                proxy.currentFrame = 0.0f;
            }

            outSnapshot.proxies.push_back(proxy);
        });
    }
}
