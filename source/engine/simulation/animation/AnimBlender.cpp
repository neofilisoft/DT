// Copyright Neofilisoft. All Rights Reserved.
#include "simulation/animation/AnimBlender.h"
#include <algorithm>
#include <cmath>

namespace lacrima::sim
{
    // -----------------------------------------------------------------------
    // BlendSpace1D::Evaluate
    // -----------------------------------------------------------------------
    void BlendSpace1D::Evaluate(
        f32 param,
        f32 time,
        const Skeleton& skeleton,
        AnimationPose& outPose) const
    {
        if (m_points.empty())
        {
            outPose.ResetToBindPose(skeleton);
            return;
        }

        // Clamp param to range of defined points
        if (param <= m_points.front().paramValue || m_points.size() == 1)
        {
            if (m_points.front().clip)
                outPose.ApplyClipSample(time, *m_points.front().clip, skeleton);
            else
                outPose.ResetToBindPose(skeleton);
            return;
        }

        if (param >= m_points.back().paramValue)
        {
            if (m_points.back().clip)
                outPose.ApplyClipSample(time, *m_points.back().clip, skeleton);
            else
                outPose.ResetToBindPose(skeleton);
            return;
        }

        // Find surrounding pair
        for (usize i = 0; i + 1 < m_points.size(); ++i)
        {
            if (param >= m_points[i].paramValue && param <= m_points[i + 1].paramValue)
            {
                const f32 dt = m_points[i + 1].paramValue - m_points[i].paramValue;
                const f32 alpha = (dt > 1e-8f) ? (param - m_points[i].paramValue) / dt : 0.0f;

                // Sample both clips into separate poses then blend
                AnimationPose poseA, poseB;
                poseA.Initialize(skeleton.BoneCount());
                poseB.Initialize(skeleton.BoneCount());

                if (m_points[i].clip)
                    poseA.ApplyClipSample(time, *m_points[i].clip, skeleton);
                else
                    poseA.ResetToBindPose(skeleton);

                if (m_points[i + 1].clip)
                    poseB.ApplyClipSample(time, *m_points[i + 1].clip, skeleton);
                else
                    poseB.ResetToBindPose(skeleton);

                // Blend poseA toward poseB by alpha
                poseA.BlendWith(poseB, alpha);
                outPose = std::move(poseA);
                return;
            }
        }

        outPose.ResetToBindPose(skeleton);
    }

    // -----------------------------------------------------------------------
    // CrossFader
    // -----------------------------------------------------------------------
    void CrossFader::StartTransition(const AnimationClip* from, const AnimationClip* to, f32 blendDuration)
    {
        m_clipFrom      = from;
        m_clipTo        = to;
        m_blendDuration = blendDuration;
        m_blendElapsed  = 0.0f;
    }

    bool CrossFader::Tick(f32 deltaTime)
    {
        if (m_blendElapsed < m_blendDuration)
        {
            m_blendElapsed += deltaTime;
            return true; // still blending
        }
        return false;
    }

    f32 CrossFader::BlendAlpha() const
    {
        if (m_blendDuration < 1e-8f) return 1.0f;
        return std::min(m_blendElapsed / m_blendDuration, 1.0f);
    }

    void CrossFader::Evaluate(
        f32 fromTime,
        f32 toTime,
        const Skeleton& skeleton,
        AnimationPose& outPose) const
    {
        const f32 alpha = BlendAlpha();

        if (alpha >= 1.0f || m_clipFrom == nullptr)
        {
            // Transition complete: play only target clip
            if (m_clipTo)
                outPose.ApplyClipSample(toTime, *m_clipTo, skeleton);
            else
                outPose.ResetToBindPose(skeleton);
            return;
        }

        AnimationPose poseFrom, poseTo;
        poseFrom.Initialize(skeleton.BoneCount());
        poseTo.Initialize(skeleton.BoneCount());

        poseFrom.ApplyClipSample(fromTime, *m_clipFrom, skeleton);

        if (m_clipTo)
            poseTo.ApplyClipSample(toTime, *m_clipTo, skeleton);
        else
            poseTo.ResetToBindPose(skeleton);

        poseFrom.BlendWith(poseTo, alpha);
        outPose = std::move(poseFrom);
    }
}
