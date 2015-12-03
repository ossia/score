#pragma once
#include <Process/ProcessFactory.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

class ProcessList final : public iscore::FactoryListInterface
{
        ISCORE_FACTORY_LIST_DECL(ProcessFactory)
};

// MOVEME
#include <Process/StateProcessFactory.hpp>
class StateProcessList final : public iscore::FactoryListInterface
{
        ISCORE_FACTORY_LIST_DECL(StateProcessFactory)
};
