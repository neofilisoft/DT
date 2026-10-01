// Copyright Neofilisoft. All Rights Reserved.
#include "simulation/animation/AnimationClip.h"
#include <cmath>
#include <algorithm>

namespace lacrima::sim
{
    // ---------------------------------------------------------------------------
    // Helpers - find lower key index for a given time t
    // ---------------------------------------------------------------------------
    template<typename K>
    static usize FindKeyIndex(const std::vector<K>& keys, f32 t)
    {
        if (keys.empty()) return 0;
        // Binary search for last key with key.time <= t
        usize lo = 0, hi = keys.size() - 1;
        while (lo < hi)
        {
            usize mid = (lo + hi + 1) / 2;
            if (keys[mid].time <= t) lo = mid;
            else hi = mid - 1;
        }
        return lo;
    }

    static f32 CalcAlpha(f32 t0, f32 t1, f32 t)
    {
        const f32 dt = t1 - t0;
        if (dt < 1e-8f) return 0.0f;
        return (t - t0) / dt;
    }

    // ---------------------------------------------------------------------------
    // BoneTrack::Sample methods
    // ---------------------------------------------------------------------------
    Vec3 BoneTrack::SamplePosition(f32 t) const
    {
        if (positionKeys.empty()) return Vec3(0.0f, 0.0f, 0.0f);
        if (positionKeys.size() == 1) return positionKeys[0].value;

        const usize i = FindKeyIndex(positionKeys, t);
        if (i + 1 >= positionKeys.size()) return positionKeys.back().value;

        const f32 alpha = CalcAlpha(positionKeys[i].time, positionKeys[i + 1].time, t);
        return Vec3::Lerp(positionKeys[i].value, positionKeys[i + 1].value, alpha);
    }

    Quat BoneTrack::SampleRotation(f32 t) const
    {
        if (rotationKeys.empty()) return Quat(0.0f, 0.0f, 0.0f, 1.0f);
        if (rotationKeys.size() == 1) return rotationKeys[0].value;

        const usize i = FindKeyIndex(rotationKeys, t);
        if (i + 1 >= rotationKeys.size()) return rotationKeys.back().value;

        const f32 alpha = CalcAlpha(rotationKeys[i].time, rotationKeys[i + 1].time, t);
        return Quat::Slerp(rotationKeys[i].value, rotationKeys[i + 1].value, alpha);
    }

    Vec3 BoneTrack::SampleScale(f32 t) const
    {
        if (scaleKeys.empty()) return Vec3(1.0f, 1.0f, 1.0f);
        if (scaleKeys.size() == 1) return scaleKeys[0].value;

        const usize i = FindKeyIndex(scaleKeys, t);
        if (i + 1 >= scaleKeys.size()) return scaleKeys.back().value;

        const f32 alpha = CalcAlpha(scaleKeys[i].time, scaleKeys[i + 1].time, t);
        return Vec3::Lerp(scaleKeys[i].value, scaleKeys[i + 1].value, alpha);
    }

    // ---------------------------------------------------------------------------
    // AnimationClip
    // ---------------------------------------------------------------------------
    BoneTrack& AnimationClip::GetOrCreateTrack(i32 boneIndex)
    {
        for (auto& track : tracks)
        {
            if (track.boneIndex == boneIndex)
                return track;
        }
        BoneTrack& newTrack = tracks.emplace_back();
        newTrack.boneIndex = boneIndex;
        return newTrack;
    }

    void AnimationClip::Sample(
        f32 t,
        std::vector<Vec3>& outPositions,
        std::vector<Quat>& outRotations,
        std::vector<Vec3>& outScales) const
    {
        // Wrap time if looping
        f32 sampleTime = t;
        if (isLooping && duration > 1e-8f)
        {
            sampleTime = std::fmod(t, duration);
            if (sampleTime < 0.0f) sampleTime += duration;
        }
        else
        {
            sampleTime = std::min(t, duration);
        }

        // Sample each track and write into output arrays
        for (const BoneTrack& track : tracks)
        {
            if (track.boneIndex < 0) continue;
            const auto bi = static_cast<usize>(track.boneIndex);
            if (bi >= outPositions.size()) continue;

            outPositions[bi] = track.SamplePosition(sampleTime);
            outRotations[bi] = track.SampleRotation(sampleTime);
            outScales[bi]    = track.SampleScale(sampleTime);
        }
    }
}
