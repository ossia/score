#include "ConstraintDurations.hpp"
#include "ConstraintModel.hpp"


ConstraintDurations &ConstraintDurations::operator=(const ConstraintDurations &other)
{
    m_minDuration = other.m_minDuration;
    m_maxDuration = other.m_maxDuration;
    m_defaultDuration = other.m_defaultDuration;
    m_playDuration = other.m_playDuration;
    m_rigidity = other.m_rigidity;
}

void ConstraintDurations::setDefaultDuration(const TimeValue& arg)
{
    if(m_defaultDuration != arg)
    {
        m_defaultDuration = arg;
        emit defaultDurationChanged(arg);
        m_model.consistency.setValid(true);
        m_model.consistency.setWarning(m_defaultDuration < m_minDuration || m_defaultDuration > m_maxDuration);
    }
    if(m_defaultDuration.msec() < 0)
    {
        m_model.consistency.setValid(false);
    }
}

void ConstraintDurations::setMinDuration(const TimeValue& arg)
{
    if(m_minDuration != arg)
    {
        m_minDuration = arg;
        emit minDurationChanged(arg);
        m_model.consistency.setWarning(m_defaultDuration < m_minDuration);
    }
}

void ConstraintDurations::setMaxDuration(const TimeValue& arg)
{
    if(m_maxDuration != arg)
    {
        m_maxDuration = arg;
        emit maxDurationChanged(arg);
        m_model.consistency.setWarning(m_defaultDuration > m_maxDuration && !m_maxDuration.isInfinite());
    }
}

void ConstraintDurations::setPlayDuration(const TimeValue &arg)
{
    if (m_playDuration == arg)
        return;

    m_playDuration = arg;
    emit playDurationChanged(arg);
}

void ConstraintDurations::setRigid(bool arg)
{
    if (m_rigidity == arg)
        return;

    m_rigidity = arg;
    emit rigidityChanged(arg);
}


void ConstraintDurations::Algorithms::setDurationInBounds(ConstraintModel &cstr, const TimeValue &time)
{
    if(cstr.duration.defaultDuration() != time)
    {
        // Rigid
        if(cstr.duration.isRigid())
        {
            cstr.duration.setDefaultDuration(time);

            cstr.duration.setMinDuration(time);
            cstr.duration.setMaxDuration(time);
        }
        else // TODO The checking must be done elsewhere if(arg >= m_minDuration && arg <= m_maxDuration)
            // --> it should be in a command to be undoable
        {
            cstr.duration.setDefaultDuration(time);
        }
    }
}


void ConstraintDurations::Algorithms::changeAllDurations(ConstraintModel &cstr, const TimeValue &time)
{
    if(cstr.duration.defaultDuration() != time)
    {
        // Note: the OSSIA implementation requires min <= dur <= max at all time
        // and will throw if not the case. Hence this order.
        auto delta = time - cstr.duration.defaultDuration();
        cstr.duration.setDefaultDuration(time);

        cstr.duration.setMinDuration(cstr.duration.minDuration() + delta);
        cstr.duration.setMaxDuration(cstr.duration.maxDuration() + delta);
    }
}
