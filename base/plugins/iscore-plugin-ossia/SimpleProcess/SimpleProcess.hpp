#pragma once
#include <Editor/TimeProcess.h>
#include <memory>

#include "Editor/State.h"
#include "Editor/TimeValue.h"

namespace OSSIA {
class StateElement;
class TimeConstraint;
}  // namespace OSSIA

class SimpleProcess : public OSSIA::TimeProcess
{
    public:
        SimpleProcess()
        {}

        std::shared_ptr<OSSIA::StateElement> state() override;
        std::shared_ptr<OSSIA::StateElement> offset(const OSSIA::TimeValue &) override;

    private:
        std::shared_ptr<OSSIA::TimeConstraint> m_constraint;
};
