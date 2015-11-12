#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <Process/ProcessFactory.hpp>

using ProcessList = GenericFactoryList_T<ProcessFactory, ProcessFactoryKey>;

class SingletonProcessList
{
    public:
        SingletonProcessList() = delete;
        static ProcessList& instance();
};
