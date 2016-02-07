#pragma once
#include <Process/ProcessFactory.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

#include <Process/StateProcessFactory.hpp>
namespace Process
{
class ISCORE_LIB_PROCESS_EXPORT ProcessList final :
        public iscore::ConcreteFactoryList<ProcessFactory>
{
    public:
        using object_type = Process::ProcessModel;
};

// MOVEME
class ISCORE_LIB_PROCESS_EXPORT StateProcessList final :
        public iscore::ConcreteFactoryList<StateProcessFactory>
{
    public:
        using object_type = Process::StateProcess;
};

}
