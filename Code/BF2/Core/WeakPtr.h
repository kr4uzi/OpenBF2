// Weak handle to a separately allocated control block. The target clears its pointer on destruction;
// the block lives until the final weak handle releases it. [INFERRED] Header placement.

#ifndef CORE_WEAKPTR_H     // [INFERRED] guard style
#define CORE_WEAKPTR_H

#include <new>

namespace dice {
namespace hfe {

template <typename T>
class WeakPtr
{
public:
    struct Ctrl
    {
        int refCount;
        T*  target;
    };

    WeakPtr() : m_ctrl(0) {}

    WeakPtr(const WeakPtr& other) : m_ctrl(other.m_ctrl)
    {
        if (m_ctrl)
            ++m_ctrl->refCount;
    }

    ~WeakPtr()
    {
        releaseControl(m_ctrl);
    }

    // PE 0x101326F0: the IPlayer specialization; the other retail copies are identical.
    WeakPtr& operator=(const WeakPtr& other)
    {
        // Both recovered assignment specializations retain the incoming block first. This also
        // makes self-assignment safe without a separate branch.
        Ctrl* incoming = other.m_ctrl;
        if (incoming)
            ++incoming->refCount;
        releaseControl(m_ctrl);
        m_ctrl = incoming;
        return *this;
    }

    T* get() const
    {
        return m_ctrl ? m_ctrl->target : 0;
    }

    // [CHOSEN, not recovered] spelling. Ordered WeakPtr containers are attested; the stable
    // per-target control-block address is the ordering used by the recovered call sites.
    bool operator<(const WeakPtr& other) const
    {
        return m_ctrl < other.m_ctrl;
    }

    // Public because IObjectTemplate::getWeakPtr() is the recovered control-block producer. The
    // original access arrangement (public member vs friend/private constructor) is unresolved.
    Ctrl* m_ctrl;

private:
    static void releaseControl(Ctrl* control)
    {
        if (control && control->refCount-- == 1)
            ::operator delete(control);
    }
};

} // namespace hfe
} // namespace dice

#endif // CORE_WEAKPTR_H
