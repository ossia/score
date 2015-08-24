#pragma once

#include <ProcessInterface/State/ProcessStateDataInterface.hpp>
#include <State/Message.hpp>
class AutomationModel;
class AutomationState : public ProcessStateDataInterface
{
    public:
        // watchedPoint : something between 0 and 1
        AutomationState(const AutomationModel* model, double watchedPoint);
        QString stateName() const override
        { return "AutomationState"; }

        iscore::Message message() const;
        double point() const
        { return m_point; }

        AutomationState* clone() const override;
    protected:
        const AutomationModel* model() const;

    private:
        double m_point{};
};
