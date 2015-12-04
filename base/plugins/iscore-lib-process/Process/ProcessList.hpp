#pragma once
#include <Process/ProcessFactory.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

class ISCORE_LIB_PROCESS_EXPORT ProcessList final : public iscore::FactoryListInterface
{
        ISCORE_FACTORY_LIST_DECL(ProcessFactory)

        //public:
        //    virtual ~ProcessList();
};
/*
#include <Process/ProcessList.hpp>
ProcessList::~ProcessList()
{

}
*/

// MOVEME
#include <Process/StateProcessFactory.hpp>
class ISCORE_LIB_PROCESS_EXPORT StateProcessList final : public iscore::FactoryListInterface
{
        ISCORE_FACTORY_LIST_DECL(StateProcessFactory)
};
