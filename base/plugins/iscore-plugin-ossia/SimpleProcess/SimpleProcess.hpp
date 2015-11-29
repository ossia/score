#pragma once
#include <ProcessModel/TimeProcessWithConstraint.hpp>
#include <memory>

#include "Editor/State.h"
#include "Editor/TimeValue.h"

namespace OSSIA {
class StateElement;
class TimeConstraint;
}  // namespace OSSIA

class SimpleProcess : public TimeProcessWithConstraint
{
    public:
        SimpleProcess():
            m_start{OSSIA::State::create()},
            m_end{OSSIA::State::create()}
        {

        }

        std::shared_ptr<OSSIA::StateElement> state(
                const OSSIA::TimeValue&,
                const OSSIA::TimeValue&) override;

        const std::shared_ptr<OSSIA::State>& getStartState() const override
        {
            return m_start;
        }

        const std::shared_ptr<OSSIA::State>& getEndState() const override
        {
            return m_end;
        }


    private:
        std::shared_ptr<OSSIA::TimeConstraint> m_constraint;
        std::shared_ptr<OSSIA::State> m_start;
        std::shared_ptr<OSSIA::State> m_end;
};
