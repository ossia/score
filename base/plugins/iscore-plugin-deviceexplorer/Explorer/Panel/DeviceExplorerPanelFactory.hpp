#pragma once
#include <iscore/plugins/panel/PanelDelegateFactory.hpp>
namespace Explorer
{

class PanelDelegateFactory final :
        public iscore::PanelDelegateFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("de755995-398d-4b16-9030-574533b17a9f")

        std::unique_ptr<iscore::PanelDelegate> make(
                const iscore::ApplicationContext& ctx) override;
};

}

