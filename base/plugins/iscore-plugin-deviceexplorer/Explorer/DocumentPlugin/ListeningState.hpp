#pragma once
#include <vector>
#include <State/Address.hpp>


namespace Explorer
{
class ListeningState
{
    public:
        std::vector<std::vector<State::Address>> listened;
};
}
