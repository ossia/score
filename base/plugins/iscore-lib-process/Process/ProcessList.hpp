#pragma once
#include <Process/ProcessFactory.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

using ProcessList = GenericFactoryMap_T<ProcessFactory, ProcessFactoryKey>;

class DynamicProcessList final : public iscore::FactoryListInterface
{
        ISCORE_FACTORY_LIST_DECL(ProcessFactory)
};

class SingletonProcessList
{
    public:
        SingletonProcessList() = delete;
        static ProcessList& instance();
};
