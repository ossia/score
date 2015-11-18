#pragma once
#include <Process/ProcessFactory.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

class DynamicProcessList final : public iscore::FactoryListInterface
{
        ISCORE_FACTORY_LIST_DECL(ProcessFactory)
};
