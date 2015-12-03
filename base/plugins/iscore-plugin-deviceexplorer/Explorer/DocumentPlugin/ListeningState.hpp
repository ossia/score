#pragma once
#include <vector>
#include <State/Address.hpp>

class ListeningState
{
    public:
        std::vector<std::vector<iscore::Address>> listened;
};
