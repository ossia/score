#pragma once
#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>
#include <list>

class ConstraintInspectorWidget;
class ConstraintModel;
class OngoingCommandDispatcher;
class QWidget;

class ConstraintInspectorDelegate
{
    protected:
        const ConstraintModel& m_model;
    public:
        ConstraintInspectorDelegate(const ConstraintModel& cst):
            m_model{cst}
        {

        }

        virtual ~ConstraintInspectorDelegate();

        virtual void addWidgets_pre(std::list<QWidget*>&, ConstraintInspectorWidget* parent) = 0;
        virtual void addWidgets_post(std::list<QWidget*>&, ConstraintInspectorWidget* parent) = 0;

        virtual void updateElements() = 0;

        virtual void on_defaultDurationChanged(
                OngoingCommandDispatcher& dispatcher,
                const TimeValue& val,
                ExpandMode e) const = 0;
};
