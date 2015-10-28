#pragma once
#include <Editor/TimeProcess.h>
#include <Editor/State.h>

class TimeProcessWithConstraint : public OSSIA::TimeProcess
{
    public:
        void setParentConstraint(std::shared_ptr<OSSIA::TimeConstraint> c)
        {
            m_constraint = c;
        }

        const std::shared_ptr<OSSIA::TimeConstraint>& getParentTimeConstraint() const override
        {
            return m_constraint;
        }

    private:
        std::shared_ptr<OSSIA::TimeConstraint> m_constraint;
};
