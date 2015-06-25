#include "StateExecutor.hpp"
#include "Protocols/OSSIADevice.hpp"

#include <API/Headers/Misc/Container.h>
#include <API/Headers/Editor/State.h>
#include <API/Headers/Editor/Message.h>

#include "iscore2OSSIA.hpp"

StateExecutor::StateExecutor(
        const DeviceList& lst):
    m_deviceList{lst}
{

}
/*
std::shared_ptr<OSSIA::Message> StateExecutor::messageFromIscore(
        const iscore::Message& mess)
{
    const auto& dev = m_deviceList.device(mess.address.device);

    if(auto casted_dev = dynamic_cast<const OSSIADevice*>(&dev))
    {
        auto ossia_node = iscore::convert::getNodeFromPath(
                              mess.address.path,
                              &casted_dev->impl());

        return OSSIA::Message::create(
                              ossia_node->getAddress(),
                              iscore::convert::toValue(mess.value));
    }

    return {};
}

std::shared_ptr<OSSIA::State> StateExecutor::stateFromIscore(
        const iscore::State& state)
{
    auto ossia_state = OSSIA::State::create();

    auto& elts = ossia_state->stateElements();

    if(state.data().canConvert<iscore::State>())
    {
        elts.push_back(stateFromIscore(state.data().value<iscore::State>()));
    }
    else if(state.data().canConvert<iscore::StateList>())
    {
        for(const auto& st : state.data().value<iscore::StateList>())
        {
            elts.push_back(stateFromIscore(st));
        }
    }
    else if(state.data().canConvert<iscore::Message>())
    {
        elts.push_back(messageFromIscore(state.data().value<iscore::Message>()));
    }
    else if(state.data().canConvert<iscore::MessageList>())
    {
        for(const auto& mess : state.data().value<iscore::MessageList>())
        {
            elts.push_back(messageFromIscore(mess));
        }
    }
    else
    {
        qDebug() << Q_FUNC_INFO << "todo";
    }

    return ossia_state;
}

void StateExecutor::execute(const iscore::State& state)
{
    auto ossia_state = stateFromIscore(state);
    ossia_state->launch();
}
*/
