#pragma once

#include <ProcessInterface/State/ProcessStateDataInterface.hpp>
#include <State/Message.hpp>
class AutomationModel;
// TODO move in Automation plug-in
class AutomationState : public ProcessStateDataInterface
{
    public:
        // watchedPoint : something between 0 and 1
        AutomationState(
                AutomationModel& model,
                double watchedPoint,
                QObject* parent);

        QString stateName() const override;
        AutomationModel& model() const;

        iscore::Message message() const;
        double point() const;

        AutomationState* clone(QObject* parent) const override;

        std::vector<iscore::Address> matchingAddresses() override;
        iscore::MessageList messages() const override;
        void setMessages(const iscore::MessageList&) override;

    private:
        double m_point{};

        // DynamicStateDataInterface interface
};
