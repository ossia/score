#pragma once

#include <Device/Protocol/ProtocolList.hpp>
class SingletonProtocolList
{
    public:
        SingletonProtocolList() = delete;
        static ProtocolList& instance();
};
