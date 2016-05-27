#pragma once
#include <Process/ProcessFactory.hpp>

#include <iscore/plugins/customfactory/FactoryFamily.hpp>

namespace Process
{
class ISCORE_LIB_PROCESS_EXPORT ProcessList final :
        public iscore::ConcreteFactoryList<ProcessFactory>
{
    public:
        using object_type = Process::ProcessModel;
};
}
