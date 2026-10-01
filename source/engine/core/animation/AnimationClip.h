// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/platform/Types.h"
#include "core/math/Math.h"
#include "core/animation/Bone.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace lacrima::anim
{
    struct VectorKey
    {
        f32 time = 0.0f;
        Vec3 value{0.0f, 0.0f, 0.0f};
    };

    struct QuatKey
    {
        f32 time = 0.0f;
        Quat value{0.0f, 0.0f, 0.0f, 1.0f};
    };

    struct BoneTrack
    {
        i32 boneIndex = kInvalidBoneIndex;
        std::string boneName;

        std::vector<VectorKey> positions;
        std::vector<QuatKey> rotations;
        std::vector<VectorKey> scales;
    };

    class AnimationClip
    {
    public:
        AnimationClip() = default;
        ~AnimationClip() = default;

        void SetName(const std::string& name) { m_name = name; }
        const std::string& GetName() const { return m_name; }

        void SetDuration(f32 durationSeconds) { m_duration = durationSeconds; }
        f32 GetDuration() const { return m_duration; }

        void SetTicksPerSecond(f32 tps) { m_ticksPerSecond = tps; }
        f32 GetTicksPerSecond() const { return m_ticksPerSecond; }

        void SetLooping(bool looping) { m_isLooping = looping; }
        bool IsLooping() const { return m_isLooping; }

        void AddTrack(const BoneTrack& track);
        const BoneTrack* FindTrack(i32 boneIndex) const;
        const BoneTrack* FindTrackByName(const std::string& name) const;

        // Samples bone transform at specified time (handles loop wrapping and keyframe interpolation)
        bool SampleBone(i32 boneIndex, f32 time, Vec3& outPos, Quat& outRot, Vec3& outScale) const;

        const std::vector<BoneTrack>& GetTracks() const { return m_tracks; }

    private:
        static Vec3 InterpolateVectorKeys(const std::vector<VectorKey>& keys, f32 time);
        static Quat InterpolateQuatKeys(const std::vector<QuatKey>& keys, f32 time);

        std::string m_name;
        f32 m_duration = 0.0f;
        f32 m_ticksPerSecond = 30.0f;
        bool m_isLooping = true;

        std::vector<BoneTrack> m_tracks;
        std::unordered_map<i32, size_t> m_boneIndexToTrack;
        std::unordered_map<std::string, size_t> m_boneNameToTrack;
    };
}