#pragma once
#include <OSSIA/ProcessModel/TimeProcessWithConstraint.hpp>
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
        SimpleProcess()
        {}

        std::shared_ptr<OSSIA::StateElement> state() override;

    private:
        std::shared_ptr<OSSIA::TimeConstraint> m_constraint;
};
