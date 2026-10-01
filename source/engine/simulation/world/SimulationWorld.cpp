// Copyright Neofilisoft. All Rights Reserved.
#include "simulation/world/SimulationWorld.h"
#include "simulation/spatial/SpringArmSystem.h"
#include "simulation/animation/SkeletalAnimationSystem.h"
#include "simulation/animation/VehicleSystem.h"
#include "simulation/animation/FootIKSystem.h"
#include "physics/include/physics/IPhysicsSystem.h"
#include "simulation/animation/AnimationSystem.h"
#include "simulation/sprite/SpriteAnimationSystem.h"
#include "core/input/InputManager.h"
#include "core/profiler/Profiler.h"
#include "core/logging/Logger.h"
#include "core/serialization/Serialization.h"
#include "core/io/Compression.h"
#include "core/jobs/JobSystem.h"
#include <fstream>
#include <iostream>
#include "simulation/spatial/SpatialSystem.h"
#include "simulation/spatial/LODSystem.h"

#include <cmath>
#include <cstdio>

namespace lacrima::sim
{
    SimulationWorld::SimulationWorld(usize initialEntityCount)
    {
        m_worldStreamer.Init();
        RegisterLuaBindings();
        
        m_navigationSystem.Initialize(this);
        m_navigationSystem.CreateTestNavMesh();

        BuildTickGraph();
    }

    Entity SimulationWorld::CreateEntity()
    {
        return m_entities.Create();
    }

    void SimulationWorld::DestroyEntity(Entity entity)
    {
        m_transforms.Remove(entity);
        m_visuals.Remove(entity);
        m_sprites.Remove(entity);
        m_interactables.Remove(entity);
        m_navAgents.Remove(entity);
        m_lotGrids.Remove(entity);
        m_routingSlots.Remove(entity);
        m_springArms.Remove(entity);
        m_entities.Destroy(entity);
    }

    void SimulationWorld::RegisterLuaBindings()
    {
        sol::state& lua = m_scriptEngine.Raw();

        lua.new_usertype<Entity>("Entity",
            sol::no_constructor,
            "index", sol::readonly(&Entity::index),
            "generation", sol::readonly(&Entity::generation)
        );

        sol::table dtEngine = lua.create_named_table("dt_engine");

        dtEngine.set_function("play_sound", [this](const std::string& path)
        {
            if (m_playSoundCallback)
                m_playSoundCallback(path);
        });

        dtEngine.set_function("play_sound_2d", [this](const std::string& path, f32 volume)
        {
            if (m_playSoundVolumeCallback)
                m_playSoundVolumeCallback(path, volume);
            else if (m_playSoundCallback)
                m_playSoundCallback(path);
        });

        dtEngine.set_function("set_animation_state", [this](Entity entity, const std::string& stateName)
        {
            VisualComponent* visual = m_visuals.Get(entity);
            if (visual == nullptr)
            {
                LACRIMA_LOG_WARN(LogCategory::Scripting, "dt_engine.set_animation_state: entity has no VisualComponent");
                return;
            }

            AnimationState state = AnimationState::Idle;
            if (stateName == "Walk") state = AnimationState::Walk;
            else if (stateName == "Interact") state = AnimationState::Interact;
            else if (stateName == "Idle") state = AnimationState::Idle;
            else
            {
                LACRIMA_LOG_WARN(LogCategory::Scripting, "dt_engine.set_animation_state: unknown state '%s'", stateName.c_str());
                return;
            }

            lacrima::sim::AnimationSystem::SetupSpriteAnimation(*visual, state);
        });

        dtEngine.set_function("set_sprite_texture", [this](Entity entity, const std::string& asset)
        {
            Sprite2DComponent* sprite = m_sprites.Get(entity);
            if (sprite != nullptr)
                sprite->textureAsset = StringID(asset);
        });

        dtEngine.set_function("set_sprite_animation", [this](Entity entity, const std::string& texture, u32 firstFrame, u32 frameCount, f32 framesPerSecond, bool looping, u32 atlasColumns, u32 atlasRows)
        {
            Sprite2DComponent* sprite = m_sprites.Get(entity);
            if (sprite == nullptr)
                return;

            sprite->animationEnabled = true;
            sprite->animationClip.id = StringID(texture + "#animation");
            sprite->animationClip.atlasTexture = StringID(texture);
            sprite->animationClip.firstFrame = firstFrame;
            sprite->animationClip.frameCount = std::max(1u, frameCount);
            sprite->animationClip.framesPerSecond = std::max(0.0f, framesPerSecond);
            sprite->animationClip.looping = looping;
            sprite->animationClip.atlasColumns = std::max(1u, atlasColumns);
            sprite->animationClip.atlasRows = std::max(1u, atlasRows);
            sprite->animationPlayer.Play(sprite->animationClip, true);
        });

        dtEngine.set_function("set_sprite_uv", [this](Entity entity, f32 u, f32 v, f32 width, f32 height)
        {
            Sprite2DComponent* sprite = m_sprites.Get(entity);
            if (sprite != nullptr)
            {
                sprite->uv.u = u;
                sprite->uv.v = v;
                sprite->uv.width = width;
                sprite->uv.height = height;
            }
        });

        dtEngine.set_function("set_sprite_size", [this](Entity entity, f32 width, f32 height)
        {
            Sprite2DComponent* sprite = m_sprites.Get(entity);
            if (sprite != nullptr)
            {
                sprite->width = width;
                sprite->height = height;
            }
        });

        dtEngine.set_function("set_sprite_tint", [this](Entity entity, f32 r, f32 g, f32 b, f32 a)
        {
            Sprite2DComponent* sprite = m_sprites.Get(entity);
            if (sprite != nullptr)
            {
                sprite->tintR = r;
                sprite->tintG = g;
                sprite->tintB = b;
                sprite->tintA = a;
            }
        });

        dtEngine.set_function("is_action_pressed", [](const std::string& name) -> bool
        {
            return lacrima::InputManager::Get().IsActionPressed(name);
        });

        dtEngine.set_function("is_action_held", [](const std::string& name) -> bool
        {
            return lacrima::InputManager::Get().IsActionHeld(name);
        });

        dtEngine.set_function("get_axis", [](const std::string& name) -> f32
        {
            return lacrima::InputManager::Get().GetAxis(name);
        });
    }

    void SimulationWorld::CopyFrom(const SimulationWorld& other)
    {
        m_clock = other.m_clock;
        m_entities = other.m_entities;
        m_transforms = other.m_transforms;
        m_interactables = other.m_interactables;
        m_visuals = other.m_visuals;
        m_sprites = other.m_sprites;
        m_navAgents = other.m_navAgents;
        m_lods = other.m_lods;
        m_lotGrids = other.m_lotGrids;
        m_routingSlots = other.m_routingSlots;
        m_springArms = other.m_springArms;
        m_skeletalMeshes = other.m_skeletalMeshes;
        m_footIKs  = other.m_footIKs;
        // Note: VehicleComponent.controller is a raw ptr owned by IPhysicsSystem.
        // On Clone (PIE start) we copy the authored parameters but NOT the live controller ptr.
        // VehicleSystem will lazily re-create controllers for the cloned world.
        m_vehicles = other.m_vehicles;
        // Reset live controller ptrs - VehicleSystem lazily re-creates them for the cloned world (PIE).
        m_vehicles.ForEach([](Entity, VehicleComponent& vc) { vc.controller = nullptr; });

        m_physicsSystem = other.m_physicsSystem;
        m_playSoundCallback = other.m_playSoundCallback;
        m_playSoundVolumeCallback = other.m_playSoundVolumeCallback;
    }

    std::unique_ptr<SimulationWorld> SimulationWorld::Clone() const
    {
        auto clone = std::make_unique<SimulationWorld>(m_entities.LiveCount() > 0 ? m_entities.LiveCount() : 12);
        clone->CopyFrom(*this);
        clone->FinalizeTickGraph();
        return clone;
    }

    void SimulationWorld::BuildRenderSnapshot(SimSnapshot& outSnapshot)
    {
        BuildSnapshot(outSnapshot);
    }

    void SimulationWorld::SaveState(lacrima::BinaryWriter& writer) const
    {
        DT_PROFILE_SCOPE("SimulationWorld::SaveState");
        writer.WritePrimitive<u64>(m_clock.TickIndex());

        m_entities.Serialize(writer);
        m_transforms.Serialize(writer);
        m_visuals.Serialize(writer);
        m_sprites.Serialize(writer);
        m_navAgents.Serialize(writer);

        // Serialize LotGrids
        u32 lotGridCount = 0;
        const_cast<ComponentArray<Entity, LotGridComponent>&>(m_lotGrids).ForEach([&lotGridCount](Entity, const LotGridComponent&) {
            ++lotGridCount;
        });
        writer.WritePrimitive<u32>(lotGridCount);
        const_cast<ComponentArray<Entity, LotGridComponent>&>(m_lotGrids).ForEach([&writer](Entity ent, const LotGridComponent& lot) {
            writer.WritePrimitive<Entity>(ent);
            writer.WritePrimitive<u32>(lot.width);
            writer.WritePrimitive<u32>(lot.height);
            writer.WritePrimitive<f32>(lot.tileWidth);
            writer.WritePrimitive<f32>(lot.tileHeight);

            u32 flagsCount = static_cast<u32>(lot.flags.size());
            writer.WritePrimitive<u32>(flagsCount);
            if (flagsCount > 0)
                writer.WriteBytes(lot.flags.data(), flagsCount * sizeof(u8));

            u32 floorCount = static_cast<u32>(lot.floorTextureIds.size());
            writer.WritePrimitive<u32>(floorCount);
            if (floorCount > 0)
                writer.WriteBytes(lot.floorTextureIds.data(), floorCount * sizeof(u16));

            u32 roomCount = static_cast<u32>(lot.roomIds.size());
            writer.WritePrimitive<u32>(roomCount);
            if (roomCount > 0)
                writer.WriteBytes(lot.roomIds.data(), roomCount * sizeof(u16));
        });

        // Serialize SpringArms
        u32 springArmCount = 0;
        const_cast<ComponentArray<Entity, SpringArmComponent>&>(m_springArms).ForEach([&springArmCount](Entity, const SpringArmComponent&) {
            ++springArmCount;
        });
        writer.WritePrimitive<u32>(springArmCount);
        const_cast<ComponentArray<Entity, SpringArmComponent>&>(m_springArms).ForEach([&writer](Entity ent, const SpringArmComponent& sa) {
            writer.WritePrimitive<Entity>(ent);
            writer.WritePrimitive<f32>(sa.targetArmLength);
            writer.WritePrimitive<f32>(sa.currentArmLength);
            writer.WritePrimitive<f32>(sa.minArmLength);
            writer.WritePrimitive<f32>(sa.maxArmLength);
            writer.WritePrimitive<f32>(sa.probeRadius);
            writer.WritePrimitive<f32>(sa.collisionPadding);
            writer.WritePrimitive<f32>(sa.targetPitch);
            writer.WritePrimitive<f32>(sa.targetYaw);
            writer.WritePrimitive<f32>(sa.currentPitch);
            writer.WritePrimitive<f32>(sa.currentYaw);
            writer.WritePrimitive<f32>(sa.cameraLagSpeed);
            writer.WritePrimitive<bool>(sa.enableCameraLag);
            writer.WritePrimitive<bool>(sa.doCollisionTest);
            writer.WritePrimitive<f32>(sa.targetOffset.x);
            writer.WritePrimitive<f32>(sa.targetOffset.y);
            writer.WritePrimitive<f32>(sa.targetOffset.z);
            writer.WritePrimitive<f32>(sa.computedCameraPosition.x);
            writer.WritePrimitive<f32>(sa.computedCameraPosition.y);
            writer.WritePrimitive<f32>(sa.computedCameraPosition.z);
            writer.WritePrimitive<f32>(sa.computedLookAtTarget.x);
            writer.WritePrimitive<f32>(sa.computedLookAtTarget.y);
            writer.WritePrimitive<f32>(sa.computedLookAtTarget.z);
        });

        // Serialize RoutingSlots
        u32 routingCount = 0;
        const_cast<ComponentArray<Entity, RoutingSlotComponent>&>(m_routingSlots).ForEach([&routingCount](Entity, const RoutingSlotComponent&) {
            ++routingCount;
        });
        writer.WritePrimitive<u32>(routingCount);
        const_cast<ComponentArray<Entity, RoutingSlotComponent>&>(m_routingSlots).ForEach([&writer](Entity ent, const RoutingSlotComponent& comp) {
            writer.WritePrimitive<Entity>(ent);
            writer.WritePrimitive<i32>(comp.footprint.sizeX);
            writer.WritePrimitive<i32>(comp.footprint.sizeY);

            u32 slotCount = static_cast<u32>(comp.slots.size());
            writer.WritePrimitive<u32>(slotCount);
            for (const auto& s : comp.slots)
            {
                writer.WritePrimitive<i32>(s.relativePos.x);
                writer.WritePrimitive<i32>(s.relativePos.y);
                writer.WritePrimitive<bool>(s.isReserved);
                writer.WritePrimitive<Entity>(s.reservedBy);
            }
        });
    }

    bool SimulationWorld::LoadState(lacrima::BinaryReader& reader)
    {
        DT_PROFILE_SCOPE("SimulationWorld::LoadState");

        if (reader.AtEnd()) return false;

        u64 tickIndex = reader.ReadPrimitive<u64>();
        m_clock.Advance(tickIndex);

        if (!m_entities.Deserialize(reader)) return false;

        m_interactables.Clear();
        m_lods.Clear();

        if (!m_transforms.Deserialize(reader)) return false;
        if (!m_visuals.Deserialize(reader)) return false;
        if (!m_sprites.Deserialize(reader)) return false;
        if (!m_navAgents.Deserialize(reader)) return false;

        m_entities.ForEachValid([this](Entity ent)
        {
            m_interactables.Add(ent);
            m_lods.Add(ent);
        });

        // Load LotGrids
        m_lotGrids.Clear();
        if (!reader.AtEnd())
        {
            u32 lotGridCount = reader.ReadPrimitive<u32>();
            for (u32 i = 0; i < lotGridCount; ++i)
            {
                Entity ent = reader.ReadPrimitive<Entity>();
                LotGridComponent& lot = m_lotGrids.Add(ent);
                lot.width = reader.ReadPrimitive<u32>();
                lot.height = reader.ReadPrimitive<u32>();
                lot.tileWidth = reader.ReadPrimitive<f32>();
                lot.tileHeight = reader.ReadPrimitive<f32>();

                u32 flagsCount = reader.ReadPrimitive<u32>();
                lot.flags.resize(flagsCount);
                if (flagsCount > 0)
                    reader.ReadBytes(lot.flags.data(), flagsCount * sizeof(u8));

                u32 floorCount = reader.ReadPrimitive<u32>();
                lot.floorTextureIds.resize(floorCount);
                if (floorCount > 0)
                    reader.ReadBytes(lot.floorTextureIds.data(), floorCount * sizeof(u16));

                u32 roomCount = reader.ReadPrimitive<u32>();
                lot.roomIds.resize(roomCount);
                if (roomCount > 0)
                    reader.ReadBytes(lot.roomIds.data(), roomCount * sizeof(u16));
            }
        }

        // Load SpringArms
        m_springArms.Clear();
        if (!reader.AtEnd())
        {
            u32 count = reader.ReadPrimitive<u32>();
            for (u32 i = 0; i < count; ++i)
            {
                Entity ent = reader.ReadPrimitive<Entity>();
                SpringArmComponent& sa = m_springArms.Add(ent);
                sa.targetArmLength = reader.ReadPrimitive<f32>();
                sa.currentArmLength = reader.ReadPrimitive<f32>();
                sa.minArmLength = reader.ReadPrimitive<f32>();
                sa.maxArmLength = reader.ReadPrimitive<f32>();
                sa.probeRadius = reader.ReadPrimitive<f32>();
                sa.collisionPadding = reader.ReadPrimitive<f32>();
                sa.targetPitch = reader.ReadPrimitive<f32>();
                sa.targetYaw = reader.ReadPrimitive<f32>();
                sa.currentPitch = reader.ReadPrimitive<f32>();
                sa.currentYaw = reader.ReadPrimitive<f32>();
                sa.cameraLagSpeed = reader.ReadPrimitive<f32>();
                sa.enableCameraLag = reader.ReadPrimitive<bool>();
                sa.doCollisionTest = reader.ReadPrimitive<bool>();
                sa.targetOffset.x = reader.ReadPrimitive<f32>();
                sa.targetOffset.y = reader.ReadPrimitive<f32>();
                sa.targetOffset.z = reader.ReadPrimitive<f32>();
                sa.computedCameraPosition.x = reader.ReadPrimitive<f32>();
                sa.computedCameraPosition.y = reader.ReadPrimitive<f32>();
                sa.computedCameraPosition.z = reader.ReadPrimitive<f32>();
                sa.computedLookAtTarget.x = reader.ReadPrimitive<f32>();
                sa.computedLookAtTarget.y = reader.ReadPrimitive<f32>();
                sa.computedLookAtTarget.z = reader.ReadPrimitive<f32>();
            }
        }

        // Load RoutingSlots
        m_routingSlots.Clear();
        if (!reader.AtEnd())
        {
            u32 routingCount = reader.ReadPrimitive<u32>();
            for (u32 i = 0; i < routingCount; ++i)
            {
                Entity ent = reader.ReadPrimitive<Entity>();
                RoutingSlotComponent& comp = m_routingSlots.Add(ent);
                comp.footprint.sizeX = reader.ReadPrimitive<i32>();
                comp.footprint.sizeY = reader.ReadPrimitive<i32>();

                u32 slotCount = reader.ReadPrimitive<u32>();
                comp.slots.resize(slotCount);
                for (u32 s = 0; s < slotCount; ++s)
                {
                    comp.slots[s].relativePos.x = reader.ReadPrimitive<i32>();
                    comp.slots[s].relativePos.y = reader.ReadPrimitive<i32>();
                    comp.slots[s].isReserved = reader.ReadPrimitive<bool>();
                    comp.slots[s].reservedBy = reader.ReadPrimitive<Entity>();
                }
            }
        }

        return !reader.HasError();
    }

    void SimulationWorld::BuildTickGraph()
    {
        auto& springArmNode = m_tickGraph.AddTask([this]() {
            SpringArmSystem::Update(m_currentFixedDeltaSeconds, m_transforms, m_springArms, m_physicsSystem);
        }, "SpringArm");
        auto& lodNode = m_tickGraph.AddTask([this]() { m_lodSystem.Step(*this); }, "LOD");
        lodNode.After(springArmNode);
        auto& streamerNode = m_tickGraph.AddTask([this]() { m_worldStreamer.Step(*this); }, "WorldStreamer");
        streamerNode.After(lodNode);
        auto& vehicleNode = m_tickGraph.AddTask([this]() { VehicleSystem::Update(*this, m_currentFixedDeltaSeconds); }, "VehicleSystem");
        vehicleNode.After(streamerNode);
        auto& footIKNode = m_tickGraph.AddTask([this]() { FootIKSystem::Update(*this, m_currentFixedDeltaSeconds); }, "FootIKSystem");
        footIKNode.After(vehicleNode);
        auto& snapshotNode = m_tickGraph.AddTask([this]() { BuildSnapshot(*m_currentOutSnapshot); }, "Snapshot");
        snapshotNode.After(streamerNode);
    }

    void SimulationWorld::FinalizeTickGraph()
    {
        m_tickGraph.Finalize();
    }

    void SimulationWorld::Tick(u64 tickIndex, f64 fixedDeltaSeconds, SimSnapshot& outSnapshot)
    {
        m_currentFixedDeltaSeconds = static_cast<f32>(fixedDeltaSeconds);
        m_currentOutSnapshot = &outSnapshot;

        m_tickGraph.Reset();
        lacrima::JobSystem::Get().RunGraph(m_tickGraph);
        m_clock.Advance(tickIndex);
    }

    SimTickFunc SimulationWorld::MakeTickFunc()
    {
        return [this](u64 tickIndex, f64 dt, SimSnapshot& snapshot)
        {
            Tick(tickIndex, dt, snapshot);
        };
    }

    void SimulationWorld::StepTime() { }
    void SimulationWorld::StepNavigation() { }

    void SimulationWorld::BuildSnapshot(SimSnapshot& outSnapshot)
    {
        DT_PROFILE_SCOPE("SimulationWorld::BuildSnapshot");

        outSnapshot.proxies.clear();
        outSnapshot.raycastColumns.clear();

        m_transforms.ForEach([this, &outSnapshot](Entity entity, TransformComponent& transform)
        {
            VisualComponent* visual = m_visuals.Get(entity);
            if (visual != nullptr)
            {
                RenderProxy proxy;
                proxy.positionX = transform.x;
                proxy.positionY = transform.y;
                proxy.positionZ = transform.z;
                proxy.scaleX = 1.0f;
                proxy.scaleY = 1.0f;
                proxy.visualId = visual->visualId;
                outSnapshot.proxies.push_back(proxy);
            }
            
            Sprite2DComponent* sprite = m_sprites.Get(entity);
            if (sprite != nullptr)
            {
                RenderProxy proxy;
                proxy.positionX = transform.x;
                proxy.positionY = transform.y;
                proxy.positionZ = transform.z;
                proxy.scaleX = sprite->width;
                proxy.scaleY = sprite->height;
                proxy.visualId = sprite->textureAsset;
                
                proxy.spriteU = sprite->uv.u;
                proxy.spriteV = sprite->uv.v;
                proxy.spriteWidth = sprite->uv.width;
                proxy.spriteHeight = sprite->uv.height;
                proxy.spriteTintR = sprite->tintR;
                proxy.spriteTintG = sprite->tintG;
                proxy.spriteTintB = sprite->tintB;
                proxy.spriteTintA = sprite->tintA;
                
                outSnapshot.proxies.push_back(proxy);
            }
        });
    }
    bool SimulationWorld::SaveToFile(const std::string& path) const
    {
        lacrima::BinaryWriter writer;
        SaveState(writer);
        
        std::ofstream out(path, std::ios::binary);
        if (!out) return false;
        const auto& data = writer.Data();
        out.write(reinterpret_cast<const char*>(data.data()), data.size());
        return true;
    }

    bool SimulationWorld::LoadFromFile(const std::string& path)
    {
        std::ifstream in(path, std::ios::binary | std::ios::ate);
        if (!in) return false;
        
        std::streamsize size = in.tellg();
        in.seekg(0, std::ios::beg);
        
        std::vector<u8> data(size);
        if (in.read(reinterpret_cast<char*>(data.data()), size))
        {
            lacrima::BinaryReader reader(data);
            return LoadState(reader);
        }
        return false;
    }
}







