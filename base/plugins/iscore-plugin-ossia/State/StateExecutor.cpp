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

void StateExecutor::execute(const iscore::State& state)
{
    auto ossia_state = stateFromIscore(state);
    ossia_state->launch();
}
*/
