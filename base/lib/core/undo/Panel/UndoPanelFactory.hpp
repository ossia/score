#pragma once
#include <iscore/plugins/panel/PanelDelegateFactory.hpp>

namespace iscore
{

class UndoPanelDelegateFactory final
    : public PanelDelegateFactory
{
  ISCORE_CONCRETE_FACTORY("293c0f8b-fcb4-4554-8425-4bc03d803c75")

  std::unique_ptr<PanelDelegate>
  make(const iscore::ApplicationContext& ctx) override;
};
}
