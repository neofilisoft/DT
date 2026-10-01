// Copyright Neofilisoft. All Rights Reserved.
#include "simulation/action/HitboxManager.h"
#include "core/logging/Logger.h"
#include <cmath>

namespace lacrima::action
{
    void HitboxManager::RegisterHitbox(u64 entityId, const HitboxComponent& hitbox, f32 posX, f32 posY, f32 posZ)
    {
        m_hitboxes.push_back({ entityId, hitbox, posX, posY, posZ });
    }

    void HitboxManager::RegisterHurtbox(u64 entityId, const HurtboxComponent& hurtbox, f32 posX, f32 posY, f32 posZ)
    {
        m_hurtboxes.push_back({ entityId, hurtbox, posX, posY, posZ });
    }

    void HitboxManager::Clear()
    {
        m_hitboxes.clear();
        m_hurtboxes.clear();
        m_recentHits.clear();
    }

    void HitboxManager::CheckCollisions()
    {
        m_recentHits.clear();

        for (auto& hit : m_hitboxes)
        {
            if (!hit.hitbox.isActive)
            {
                continue;
            }

            for (const auto& hurt : m_hurtboxes)
            {
                // Disallow self-damage
                if (hit.entityId == hurt.entityId)
                {
                    continue;
                }

                f32 dx = hit.posX - hurt.posX;
                f32 dy = hit.posY - hurt.posY;
                f32 dz = hit.posZ - hurt.posZ;
                f32 distSq = (dx * dx) + (dy * dy) + (dz * dz);

                f32 radSum = hit.hitbox.radius + hurt.hurtbox.radius;
                if (distSq <= (radSum * radSum))
                {
                    HitEvent evt;
                    evt.attackerId = hit.entityId;
                    evt.defenderId = hurt.entityId;
                    evt.damage = hit.hitbox.damage;
                    evt.hitBone = hit.hitbox.boneName;
                    evt.hurtBone = hurt.hurtbox.boneName;

                    m_recentHits.push_back(evt);

                    LACRIMA_LOG_INFO(LogCategory::Simulation,
                        "HitboxManager: Collision detected between Entity %llu (hitbox) and Entity %llu (hurtbox) for %.1f damage",
                        hit.entityId, hurt.entityId, hit.hitbox.damage);
                }
            }

            // Decrement active frame countdown
            if (hit.hitbox.activeFrames > 0)
            {
                --hit.hitbox.activeFrames;
                if (hit.hitbox.activeFrames == 0)
                {
                    hit.hitbox.isActive = false;
                }
            }
        }
    }
}
