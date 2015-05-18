#pragma once

#include <DeviceExplorer/Protocol/ProtocolList.hpp>
class SingletonProtocolList
{
    public:
        SingletonProtocolList() = delete;
        static ProtocolList& instance();
};
