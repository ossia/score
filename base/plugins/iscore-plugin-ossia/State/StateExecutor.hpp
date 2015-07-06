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

//        void execute(const iscore::State&);

    private:
        const DeviceList& m_deviceList;

};
