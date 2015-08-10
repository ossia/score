#pragma once

#include "src/Area/AreaFactoryList.hpp"
class SingletonAreaFactoryList
{
    public:
        SingletonAreaFactoryList() = delete;
        static AreaFactoryList& instance();
};
