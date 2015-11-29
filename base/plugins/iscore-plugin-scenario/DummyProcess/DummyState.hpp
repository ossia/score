#pragma once
#include <Process/State/ProcessStateDataInterface.hpp>
#include <QString>
#include <vector>

#include <Process/State/MessageNode.hpp>
#include <State/Message.hpp>

class Process;
class QObject;
namespace iscore {
struct Address;
}  // namespace iscore

class DummyState final : public ProcessStateDataInterface
{
    public:
        DummyState(Process& model, QObject* parent);
        QString stateName() const override;
        ProcessStateDataInterface* clone(QObject* parent) const override;

        std::vector<iscore::Address> matchingAddresses() override;
        iscore::MessageList messages() const override;
        iscore::MessageList setMessages(
                const iscore::MessageList& newMessages,
                const MessageNode& currentState) override;
};
