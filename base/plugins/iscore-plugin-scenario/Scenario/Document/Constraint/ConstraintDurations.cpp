#include "ConstraintDurations.hpp"
#include "ConstraintModel.hpp"
#include <Process/TimeValue.hpp>
#include <Scenario/Document/ModelConsistency.hpp>

#define TIME_TOLERANCE_MSEC 0.5
namespace Scenario
{
ConstraintDurations &ConstraintDurations::operator=(const ConstraintDurations &other)
{
    m_defaultDuration = other.m_defaultDuration;
    m_minDuration = other.m_minDuration;
    m_maxDuration = other.m_maxDuration;

    m_playPercentage = other.m_playPercentage;
    m_executionSpeed = other.m_executionSpeed;
    m_rigidity = other.m_rigidity;

    m_isMinNull = other.m_isMinNull;
    m_isMaxInfinite = other.m_isMaxInfinite;

    return *this;
}

void ConstraintDurations::checkConsistency()
{
  m_model.consistency.setWarning(minDuration().msec() < 0 - TIME_TOLERANCE_MSEC ||
                                 (isRigid() && minDuration() != maxDuration()) ); // a voir

  m_model.consistency.setValid(minDuration() - TimeValue::fromMsecs(TIME_TOLERANCE_MSEC) <= m_defaultDuration &&
                               maxDuration() + TimeValue::fromMsecs(TIME_TOLERANCE_MSEC) >= m_defaultDuration &&
                               m_defaultDuration.msec() + TIME_TOLERANCE_MSEC > 0);

}

void ConstraintDurations::setDefaultDuration(const TimeValue& arg)
{
    if(m_defaultDuration != arg)
    {
        m_defaultDuration = arg;
        emit defaultDurationChanged(arg);
    }
    checkConsistency();
}

void ConstraintDurations::setMinDuration(const TimeValue& arg)
{
    if(m_minDuration != arg && !m_isMinNull)
    {
        m_minDuration = arg;
        emit minDurationChanged(arg);
        checkConsistency();
    }
}

void ConstraintDurations::setMaxDuration(const TimeValue& arg)
{
    if(m_maxDuration != arg)
    {
        m_maxDuration = arg;
        emit maxDurationChanged(arg);
        checkConsistency();
    }
}

void ConstraintDurations::setPlayPercentage(double arg)
{
    if (m_playPercentage == arg)
        return;

    m_playPercentage = arg;
    emit playPercentageChanged(arg);
}

void ConstraintDurations::setRigid(bool arg)
{
    if (m_rigidity == arg)
        return;

    m_rigidity = arg;
    emit rigidityChanged(arg);
}

void ConstraintDurations::setMinNull(bool isMinNull)
{
    if (m_isMinNull == isMinNull)
        return;

    m_isMinNull = isMinNull;
    emit minNullChanged(isMinNull);
    emit minDurationChanged(minDuration());
}

void ConstraintDurations::setMaxInfinite(bool isMaxInfinite)
{
    if (m_isMaxInfinite == isMaxInfinite)
        return;

    m_isMaxInfinite = isMaxInfinite;
    emit maxInfiniteChanged(isMaxInfinite);
    emit maxDurationChanged(maxDuration());
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
}
