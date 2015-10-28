#include "DummyState.hpp"

QString DummyState::stateName() const
{
}

ProcessStateDataInterface*DummyState::clone(QObject* parent) const
{
}

std::vector<iscore::Address> DummyState::matchingAddresses()
{
}

iscore::MessageList DummyState::messages() const
{
}

iscore::MessageList DummyState::setMessages(const iscore::MessageList& newMessages, const MessageNode& currentState)
{
}
