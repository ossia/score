#pragma once
#include <Process/ProcessFactory.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

#include <Process/StateProcessFactory.hpp>
namespace Process
{
class ISCORE_LIB_PROCESS_EXPORT ProcessList final : public iscore::FactoryListInterface
{
        ISCORE_FACTORY_LIST_DECL(Process::ProcessFactory)

        //public:
        //    virtual ~ProcessList();
};

// MOVEME
class ISCORE_LIB_PROCESS_EXPORT StateProcessList final : public iscore::FactoryListInterface
{
        ISCORE_FACTORY_LIST_DECL(StateProcessFactory)
};

}
