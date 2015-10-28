#pragma once
#include <ProcessInterface/State/ProcessStateDataInterface.hpp>

class DummyState : public ProcessStateDataInterface
{
    public:
        QString stateName() const;
        ProcessStateDataInterface*clone(QObject* parent) const;
        std::vector<iscore::Address> matchingAddresses();
        iscore::MessageList messages() const;
        iscore::MessageList setMessages(const iscore::MessageList& newMessages, const MessageNode& currentState);
};
