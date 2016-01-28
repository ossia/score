#pragma once
#include <vector>
#include <State/Address.hpp>


namespace DeviceExplorer
{
class ListeningState
{
    public:
        std::vector<std::vector<State::Address>> listened;
};
}
