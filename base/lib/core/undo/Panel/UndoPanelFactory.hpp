#pragma once
#include <iscore/plugins/panel/PanelDelegateFactory.hpp>

namespace iscore
{

class ISCORE_LIB_BASE_EXPORT UndoPanelDelegateFactory final :
        public PanelDelegateFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("293c0f8b-fcb4-4554-8425-4bc03d803c75")

        std::unique_ptr<PanelDelegate> make(
                const iscore::ApplicationContext& ctx) override;
};

}

