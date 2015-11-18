#pragma once
#include <list>
#include <iscore/command/Dispatchers/OngoingCommandDispatcher.hpp>
#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>

class QWidget;
class ConstraintInspectorWidget;
class ConstraintModel;

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
