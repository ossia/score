#pragma once
#include <Process/ProcessFactory.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>

using ProcessList = GenericFactoryMap_T<ProcessFactory, ProcessFactoryKey>;

class SingletonProcessList
{
    public:
        SingletonProcessList() = delete;
        static ProcessList& instance();
};
