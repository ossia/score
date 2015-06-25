#pragma once
#include <State/State.hpp>
#include <DeviceExplorer/Protocol/DeviceList.hpp>
namespace OSSIA
{
class State;
class Message;
}
class StateExecutor
{
    public:
        StateExecutor(const DeviceList&);

        void execute(const iscore::State&);

    private:
        std::shared_ptr<OSSIA::State> stateFromIscore(const iscore::State& state);
        std::shared_ptr<OSSIA::Message> messageFromIscore(const iscore::Message& mess);

        const DeviceList& m_deviceList;

};
