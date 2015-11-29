#pragma once

#include <Process/State/ProcessStateDataInterface.hpp>
#include <State/Message.hpp>
#include <qstring.h>
#include <vector>

#include "Process/State/MessageNode.hpp"

class AutomationModel;
class QObject;
namespace iscore {
struct Address;
}  // namespace iscore

class AutomationState final : public ProcessStateDataInterface
{
    public:
        // watchedPoint : something between 0 and 1
        AutomationState(
                AutomationModel& process,
                double watchedPoint,
                QObject* parent);

        QString stateName() const override;
        AutomationModel& process() const;

        iscore::Message message() const;
        double point() const;

        AutomationState* clone(QObject* parent) const override;

        std::vector<iscore::Address> matchingAddresses() override;
        iscore::MessageList messages() const override;
        iscore::MessageList setMessages(const iscore::MessageList&, const MessageNode&) override;

    private:
        double m_point{};
};
