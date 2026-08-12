#pragma once

#include "core/handle/Handle.h"
#include "runtime/Entity.h"

// ---------------------------------------------------------------------------
// EntityAllocator.h
//
// Real entity identity allocation. Entity is Handle<EntityTag> (see
// Entity.h); this is the SlotMap<EntityTag> that actually produces and
// validates those handles. Every simulation module's
// ComponentArray<Entity, T> keys against Entity values that came from
// exactly one EntityAllocator instance (owned by SimulationWorld) - this
// replaces the M1 harness's shortcut of using SlotMap<ToySim>::Create's own
// handle reinterpreted as an Entity (see Game.cpp's original comment on
// this being an M1-only expedient), which worked for a single throwaway
// component type but does not generalize: once multiple real modules
// (Needs, InteractionQueue, future Relationship/Navigation/...) each need
// to key a ComponentArray<Entity, T> against the *same* entity identity,
// that identity must be allocated independently of any one module's own
// storage, not borrowed from whichever module happened to create the
// SlotMap first.
//
// EntityTag itself carries no data (see Entity.h) - SlotMap<EntityTag>'s
// "slots" are just generation-counted placeholders; the real per-entity
// data lives entirely in per-module ComponentArray<Entity, T> instances
// that use the Entity this allocator produces as their key. This mirrors
// how a real ECS separates entity *identity* (just an ID) from entity
// *data* (components) - identity here is deliberately as small as
// possible (Handle<EntityTag> is 8 bytes) precisely because it exists only
// to be a stable cross-module key, not a data container.
// ---------------------------------------------------------------------------

namespace dt
{
    class EntityAllocator
    {
    public:
        Entity Create()
        {
            return m_slots.Create();
        }

        bool Destroy(Entity entity)
        {
            return m_slots.Destroy(entity);
        }

        bool IsValid(Entity entity) const
        {
            return m_slots.IsValid(entity);
        }

        usize LiveCount() const { return m_slots.LiveCount(); }

        template <typename Func>
        void ForEachValid(Func&& func) const
        {
            m_slots.ForEachValid([&](Entity entity, EntityTag&) { func(entity); });
        }

    private:
        SlotMap<EntityTag> m_slots;
    };
}
