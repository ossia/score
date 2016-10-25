#include "DummyState.hpp"
#include <Process/State/ProcessStateDataInterface.hpp>
#include <State/Address.hpp>

namespace Process { class ProcessModel; }
class QObject;

namespace Dummy
{
DummyState::DummyState(
        Process::ProcessModel& model,
        QObject* parent):
    ProcessStateDataInterface{model, parent}
{

}

ProcessStateDataInterface* DummyState::clone(
        QObject* parent) const
{
    return new DummyState{process(), parent};
}

std::vector<State::AddressAccessor> DummyState::matchingAddresses()
{
    return {};
}

State::MessageList DummyState::messages() const
{
    return {};
}

State::MessageList DummyState::setMessages(
        const State::MessageList&,
        const Process::MessageNode&)
{
    return {};
}
}
