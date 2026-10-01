// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/containers/ComponentArray.h"
#include "core/math/Math.h"
#include "simulation/spatial/LODComponent.h"
#include "simulation/spatial/TransformComponent.h"
#include "simulation/animation/VisualComponent.h"

namespace lacrima::sim
{
    class SimulationWorld;

    class LODSystem
    {
    public:
        // Executes the LOD step: calculates distance to active camera and updates VisualComponent
        void Step(SimulationWorld& world, const Vec3& cameraPos = Vec3(0.0f, 0.0f, 0.0f));
    };
}
