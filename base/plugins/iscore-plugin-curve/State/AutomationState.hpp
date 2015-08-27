#pragma once

#include <ProcessInterface/State/ProcessStateDataInterface.hpp>
#include <State/Message.hpp>
class AutomationModel;
class AutomationState : public ProcessStateDataInterface
{
    public:
        // watchedPoint : something between 0 and 1
        AutomationState(
                const AutomationModel& model,
                double watchedPoint,
                QObject* parent);

        QString stateName() const override
        { return "AutomationState"; }

        iscore::Message message() const;
        double point() const
        { return m_point; }

        AutomationState* clone(QObject* parent) const override;

        const AutomationModel& model() const;

    private:
        double m_point{};
};
