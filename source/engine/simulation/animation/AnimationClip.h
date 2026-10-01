// Copyright Neofilisoft. All Rights Reserved.
#pragma once

// ---------------------------------------------------------------------------
// AnimationClip.h - Lacrima Engine Skeletal Animation (Step 3)
//
// A single named animation (e.g. "Idle", "Walk", "Run", "Jump").
// Stores per-bone keyframe tracks for position, rotation, and scale.
// Supports both looping and one-shot playback.
//
// Sampling:
//   AnimationClip::Sample(time, boneCount, outLocalTransforms)
//   Writes one TRS per bone into outLocalTransforms[boneIndex].
//   Missing tracks default to identity (bind pose).
// ---------------------------------------------------------------------------

#include "core/platform/Types.h"
#include "core/math/Math.h"
#include <vector>
#include <string>

namespace lacrima::sim
{
    // ---------------------------------------------------------------------------
    // Keyframe types
    // ---------------------------------------------------------------------------
    struct PositionKey { f32 time; Vec3 value; };
    struct RotationKey { f32 time; Quat value; };
    struct ScaleKey    { f32 time; Vec3 value; };

    // Per-bone animation track
    struct BoneTrack
    {
        i32                      boneIndex = -1;
        std::vector<PositionKey> positionKeys;
        std::vector<RotationKey> rotationKeys;
        std::vector<ScaleKey>    scaleKeys;

        // Sample position at time t using linear interpolation
        Vec3 SamplePosition(f32 t) const;
        // Sample rotation at time t using spherical linear interpolation
        Quat SampleRotation(f32 t) const;
        // Sample scale at time t using linear interpolation
        Vec3 SampleScale(f32 t) const;
    };

    // Represents one animation clip (Idle, Walk, Run, Attack, etc.)
    struct AnimationClip
    {
        std::string name;
        f32         duration    = 0.0f; // Total clip duration in seconds
        bool        isLooping   = true;

        std::vector<BoneTrack> tracks;

        // Adds or returns a reference to the track for a given boneIndex
        BoneTrack& GetOrCreateTrack(i32 boneIndex);

        // Samples all tracks at time t and writes TRS values into
        // outPositions, outRotations, outScales (indexed by bone index).
        // Arrays must be pre-sized to at least skeleton.BoneCount() elements.
        void Sample(
            f32 t,
            std::vector<Vec3>& outPositions,
            std::vector<Quat>& outRotations,
            std::vector<Vec3>& outScales) const;
    };
}
