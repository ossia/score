#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/panel/PanelDelegate.hpp>

namespace iscore
{
class ISCORE_LIB_BASE_EXPORT PanelDelegateFactory :
        public iscore::AbstractFactory<PanelDelegateFactory>
{
        ISCORE_ABSTRACT_FACTORY_DECL(
                PanelDelegateFactory,
                "8d6211f7-5244-44f9-94dd-f3e32255c43e")
        public:
            virtual ~PanelDelegateFactory();

        virtual std::unique_ptr<PanelDelegate> make(
                const iscore::ApplicationContext& ctx) = 0;
};

class ISCORE_LIB_BASE_EXPORT PanelDelegateFactoryList final :
        public ConcreteFactoryList<iscore::PanelDelegateFactory>
{
    public:
        using object_type = PanelDelegate;
};
}
