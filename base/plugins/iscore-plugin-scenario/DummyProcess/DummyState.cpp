#include "DummyState.hpp"

DummyState::DummyState(
        Process& model,
        QObject* parent):
    ProcessStateDataInterface{model, parent}
{

}

QString DummyState::stateName() const
{
    return "Dummy";
}

ProcessStateDataInterface* DummyState::clone(
        QObject* parent) const
{
    return new DummyState{process(), parent};
}

std::vector<iscore::Address> DummyState::matchingAddresses()
{
    return {};
}

iscore::MessageList DummyState::messages() const
{
    return {};
}

iscore::MessageList DummyState::setMessages(
        const iscore::MessageList&,
        const MessageNode&)
{
    return {};
}
