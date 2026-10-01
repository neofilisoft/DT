// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/platform/Types.h"
#include "core/string/StringID.h"
#include <vector>

namespace lacrima::action
{
    struct HitboxComponent
    {
        StringID boneName;
        f32 radius = 0.5f;
        f32 damage = 10.0f;
        u32 activeFrames = 0;
        bool isActive = false;
    };

    struct HurtboxComponent
    {
        StringID boneName;
        f32 radius = 0.5f;
    };

    struct HitboxInstance
    {
        u64 entityId = 0;
        HitboxComponent hitbox;
        f32 posX = 0.0f;
        f32 posY = 0.0f;
        f32 posZ = 0.0f;
    };

    struct HurtboxInstance
    {
        u64 entityId = 0;
        HurtboxComponent hurtbox;
        f32 posX = 0.0f;
        f32 posY = 0.0f;
        f32 posZ = 0.0f;
    };

    struct HitEvent
    {
        u64 attackerId = 0;
        u64 defenderId = 0;
        f32 damage = 0.0f;
        StringID hitBone;
        StringID hurtBone;
    };

    class HitboxManager
    {
    public:
        void RegisterHitbox(u64 entityId, const HitboxComponent& hitbox, f32 posX = 0.0f, f32 posY = 0.0f, f32 posZ = 0.0f);
        void RegisterHurtbox(u64 entityId, const HurtboxComponent& hurtbox, f32 posX = 0.0f, f32 posY = 0.0f, f32 posZ = 0.0f);

        void Clear();
        void CheckCollisions();

        const std::vector<HitEvent>& GetRecentHits() const { return m_recentHits; }

    private:
        std::vector<HitboxInstance> m_hitboxes;
        std::vector<HurtboxInstance> m_hurtboxes;
        std::vector<HitEvent> m_recentHits;
    };
}
