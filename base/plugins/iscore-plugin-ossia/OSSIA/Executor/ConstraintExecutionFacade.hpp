#pragma once
#include <Scenario/Document/Constraint/ConstraintModel.hpp>

namespace RecreateOnPlay
{

class ConstraintExecutionFacade
{
   public:
    ConstraintExecutionFacade(Scenario::ConstraintModel& cst):
        m_cst{cst}
    {

    }

    const auto& id() const { return m_cst.id(); }

    double executionSpeed() const
    { return m_cst.duration.executionSpeed(); }
    double playPercentage() const
    { return m_cst.duration.playPercentage(); }
    void setPlayPercentage(double d) const
    { m_cst.duration.setPlayPercentage(d); }

    auto minDuration() const
    { return m_cst.duration.minDuration(); }
    auto maxDuration() const
    { return m_cst.duration.maxDuration(); }
    auto defaultDuration() const
    { return m_cst.duration.maxDuration(); }

    void reset() const
    { m_cst.reset(); }
    void executionStarted() const
    { m_cst.executionStarted(); }
    void executionStopped() const
    { m_cst.executionStopped(); }
    void executionStateChanged(Scenario::ConstraintExecutionState s) const
    { m_cst.executionStateChanged(s); }

  private:
    Scenario::ConstraintModel& m_cst;
};

}
