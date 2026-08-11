#include "simulation/interaction/InteractionQueue.h"
#include "scripting/ScriptCoroutine.h"
#include "scripting/ScriptEngine.h"

namespace dt::sim
{
    void InteractionTable::Register(InteractionDef def)
    {
        m_interactions.push_back(std::move(def));
    }

    const InteractionDef* InteractionTable::Find(const std::string& name) const
    {
        for (const InteractionDef& def : m_interactions)
        {
            if (def.name == name)
            {
                return &def;
            }
        }
        return nullptr;
    }

    void InteractionQueue::Push(const InteractionDef& def, Entity target)
    {
        QueuedInteraction q;
        q.def = &def;
        q.target = target;
        q.isAutonomous = false;
        m_queue.push_back(q);
    }

    void InteractionQueue::PushAutonomous(const InteractionDef& def, Entity target)
    {
        QueuedInteraction q;
        q.def = &def;
        q.target = target;
        q.isAutonomous = true;
        m_queue.push_back(q);
    }

    QueuedInteraction* InteractionQueue::Front()
    {
        return m_queue.empty() ? nullptr : &m_queue.front();
    }

    void InteractionQueue::PopFront()
    {
        if (!m_queue.empty())
        {
            m_queue.pop_front();
        }
        // Front entry changed (or queue is now empty) - any coroutine
        // state belonged to the entry that just left the front and must
        // not be reused for whatever is now at front (if anything).
        m_frontCoroutine.reset();
        m_frontCoroutineOwner = nullptr;
    }

    InteractionStepResult InteractionQueue::StepFront(script::ScriptEngine& engine, Entity actor, f32 fixedDeltaSeconds)
    {
        if (m_queue.empty())
        {
            return InteractionStepResult::Failed;
        }

        QueuedInteraction& front = m_queue.front();
        if (front.def == nullptr)
        {
            PopFront();
            return InteractionStepResult::Failed;
        }

        // (Re)create the coroutine if this is the first Step for this
        // front entry, or if the front entry changed since the last call
        // without going through PopFront (defensive - normal usage always
        // routes through PopFront, but this guards against the coroutine
        // silently going stale if that invariant is ever violated).
        if (!m_frontCoroutine.has_value() || m_frontCoroutineOwner != front.def)
        {
            auto created = script::ScriptCoroutine::Create(engine, front.def->luaRunFunction);
            if (!created.has_value())
            {
                // Missing/invalid Lua function - content error, not an
                // engine bug. Fail this interaction immediately rather
                // than leaving it stuck at the front of the queue forever
                // (which would permanently block the entity from ever
                // autonomy-selecting anything else again).
                PopFront();
                return InteractionStepResult::Failed;
            }
            m_frontCoroutine = std::move(created);
            m_frontCoroutineOwner = front.def;
        }

        const InteractionStepResult result = m_frontCoroutine->Step(actor, front.target, fixedDeltaSeconds);

        if (result == InteractionStepResult::Complete || result == InteractionStepResult::Failed)
        {
            PopFront(); // Also resets m_frontCoroutine/m_frontCoroutineOwner.
        }

        return result;
    }
}
