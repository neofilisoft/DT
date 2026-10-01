// Copyright Neofilisoft. All Rights Reserved.
#include "core/animation/AnimBlender.h"
#include <algorithm>

namespace lacrima::anim
{
    void AnimBlender::SampleClip(const AnimationClip& clip, f32 time, AnimationPose& outPose)
    {
        u32 boneCount = outPose.GetBoneCount();
        for (u32 i = 0; i < boneCount; ++i)
        {
            Vec3 pos(0.0f, 0.0f, 0.0f);
            Quat rot(0.0f, 0.0f, 0.0f, 1.0f);
            Vec3 scale(1.0f, 1.0f, 1.0f);

            if (clip.SampleBone(static_cast<i32>(i), time, pos, rot, scale))
            {
                outPose.SetLocalTransform(static_cast<i32>(i), pos, rot, scale);
            }
        }
    }

    void AnimBlender::BlendPoses(const AnimationPose& poseA, const AnimationPose& poseB, f32 weight, AnimationPose& outPose)
    {
        f32 w = std::clamp(weight, 0.0f, 1.0f);
        u32 count = std::min(poseA.GetBoneCount(), poseB.GetBoneCount());
        if (outPose.GetBoneCount() != count)
        {
            outPose.Resize(count);
        }

        for (u32 i = 0; i < count; ++i)
        {
            const auto& tA = poseA.GetLocalTransform(static_cast<i32>(i));
            const auto& tB = poseB.GetLocalTransform(static_cast<i32>(i));

            BoneTransform blended;
            blended.translation = Vec3::Lerp(tA.translation, tB.translation, w);
            blended.rotation = Quat::Slerp(tA.rotation, tB.rotation, w);
            blended.scale = Vec3::Lerp(tA.scale, tB.scale, w);

            outPose.SetLocalTransform(static_cast<i32>(i), blended);
        }
    }

    void BlendSpace1D::AddSample(f32 parameterValue, const AnimationClip* clip)
    {
        if (!clip) return;
        m_samples.push_back(BlendPoint1D{parameterValue, clip});
        std::sort(m_samples.begin(), m_samples.end(), [](const BlendPoint1D& a, const BlendPoint1D& b) {
            return a.value < b.value;
        });
    }

    void BlendSpace1D::Evaluate(f32 parameterValue, f32 time, AnimationPose& outPose) const
    {
        if (m_samples.empty()) return;

        if (m_samples.size() == 1 || parameterValue <= m_samples.front().value)
        {
            AnimBlender::SampleClip(*m_samples.front().clip, time, outPose);
            return;
        }

        if (parameterValue >= m_samples.back().value)
        {
            AnimBlender::SampleClip(*m_samples.back().clip, time, outPose);
            return;
        }

        // Bracket search
        for (size_t i = 0; i < m_samples.size() - 1; ++i)
        {
            if (parameterValue >= m_samples[i].value && parameterValue <= m_samples[i + 1].value)
            {
                f32 delta = m_samples[i + 1].value - m_samples[i].value;
                f32 factor = (delta > 1e-5f) ? ((parameterValue - m_samples[i].value) / delta) : 0.0f;

                AnimationPose poseA(outPose.GetBoneCount());
                AnimationPose poseB(outPose.GetBoneCount());

                AnimBlender::SampleClip(*m_samples[i].clip, time, poseA);
                AnimBlender::SampleClip(*m_samples[i + 1].clip, time, poseB);

                AnimBlender::BlendPoses(poseA, poseB, factor, outPose);
                return;
            }
        }
    }

    void CrossFader::StartTransition(f32 transitionDurationSeconds)
    {
        m_duration = (transitionDurationSeconds > 1e-4f) ? transitionDurationSeconds : 1e-4f;
        m_elapsed = 0.0f;
        m_blendWeight = 0.0f;
        m_isTransitioning = true;
    }

    void CrossFader::Update(f32 deltaTime)
    {
        if (!m_isTransitioning) return;

        m_elapsed += deltaTime;
        m_blendWeight = std::clamp(m_elapsed / m_duration, 0.0f, 1.0f);
        if (m_elapsed >= m_duration)
        {
            m_isTransitioning = false;
            m_blendWeight = 1.0f;
        }
    }
}