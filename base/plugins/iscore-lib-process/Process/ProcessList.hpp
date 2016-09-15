#pragma once
#include <Process/ProcessFactory.hpp>

#include <iscore/plugins/customfactory/FactoryFamily.hpp>

namespace Process
{
class ISCORE_LIB_PROCESS_EXPORT ProcessFactoryList final :
        public iscore::ConcreteFactoryList<ProcessModelFactory>
{
    public:
        using object_type = Process::ProcessModel;
        ~ProcessFactoryList();
};

class ISCORE_LIB_PROCESS_EXPORT LayerFactoryList final :
        public iscore::ConcreteFactoryList<LayerFactory>
{
    public:
        using object_type = Process::LayerModel;
        ~LayerFactoryList();

        LayerFactory* findDefaultFactory(const Process::ProcessModel& proc) const
        {
            for(auto& fac : *this)
            {
                if(fac.matches(proc))
                    return &fac;
            }
            return nullptr;
        }
};
}
