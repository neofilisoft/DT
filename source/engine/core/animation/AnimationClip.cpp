// Copyright Neofilisoft. All Rights Reserved.
#include "core/animation/AnimationClip.h"
#include <algorithm>
#include <cmath>

namespace lacrima::anim
{
    void AnimationClip::AddTrack(const BoneTrack& track)
    {
        size_t index = m_tracks.size();
        m_tracks.push_back(track);
        if (track.boneIndex != kInvalidBoneIndex)
        {
            m_boneIndexToTrack[track.boneIndex] = index;
        }
        if (!track.boneName.empty())
        {
            m_boneNameToTrack[track.boneName] = index;
        }
    }

    const BoneTrack* AnimationClip::FindTrack(i32 boneIndex) const
    {
        auto it = m_boneIndexToTrack.find(boneIndex);
        if (it != m_boneIndexToTrack.end())
        {
            return &m_tracks[it->second];
        }
        return nullptr;
    }

    const BoneTrack* AnimationClip::FindTrackByName(const std::string& name) const
    {
        auto it = m_boneNameToTrack.find(name);
        if (it != m_boneNameToTrack.end())
        {
            return &m_tracks[it->second];
        }
        return nullptr;
    }

    Vec3 AnimationClip::InterpolateVectorKeys(const std::vector<VectorKey>& keys, f32 time)
    {
        if (keys.empty()) return Vec3(0.0f, 0.0f, 0.0f);
        if (keys.size() == 1 || time <= keys.front().time) return keys.front().value;
        if (time >= keys.back().time) return keys.back().value;

        // Binary search for keyframe bracket
        for (size_t i = 0; i < keys.size() - 1; ++i)
        {
            if (time >= keys[i].time && time <= keys[i + 1].time)
            {
                f32 dt = keys[i + 1].time - keys[i].time;
                f32 factor = (dt > 1e-6f) ? ((time - keys[i].time) / dt) : 0.0f;
                return Vec3::Lerp(keys[i].value, keys[i + 1].value, factor);
            }
        }
        return keys.back().value;
    }

    Quat AnimationClip::InterpolateQuatKeys(const std::vector<QuatKey>& keys, f32 time)
    {
        if (keys.empty()) return Quat(0.0f, 0.0f, 0.0f, 1.0f);
        if (keys.size() == 1 || time <= keys.front().time) return keys.front().value;
        if (time >= keys.back().time) return keys.back().value;

        for (size_t i = 0; i < keys.size() - 1; ++i)
        {
            if (time >= keys[i].time && time <= keys[i + 1].time)
            {
                f32 dt = keys[i + 1].time - keys[i].time;
                f32 factor = (dt > 1e-6f) ? ((time - keys[i].time) / dt) : 0.0f;
                return Quat::Slerp(keys[i].value, keys[i + 1].value, factor);
            }
        }
        return keys.back().value;
    }

    bool AnimationClip::SampleBone(i32 boneIndex, f32 time, Vec3& outPos, Quat& outRot, Vec3& outScale) const
    {
        const BoneTrack* track = FindTrack(boneIndex);
        if (!track) return false;

        f32 animTime = time;
        if (m_duration > 0.0f)
        {
            if (m_isLooping)
            {
                animTime = std::fmod(time, m_duration);
                if (animTime < 0.0f) animTime += m_duration;
            }
            else
            {
                animTime = std::clamp(time, 0.0f, m_duration);
            }
        }

        if (!track->positions.empty())
        {
            outPos = InterpolateVectorKeys(track->positions, animTime);
        }
        if (!track->rotations.empty())
        {
            outRot = InterpolateQuatKeys(track->rotations, animTime);
        }
        if (!track->scales.empty())
        {
            outScale = InterpolateVectorKeys(track->scales, animTime);
        }

        return true;
    }
}