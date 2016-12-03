#pragma once
#include <iscore/plugins/panel/PanelDelegateFactory.hpp>

namespace InspectorPanel
{

class PanelDelegateFactory final : public iscore::PanelDelegateFactory
{
  ISCORE_CONCRETE_FACTORY("3c489368-c946-4f9f-8d6c-d051b724726c")

  std::unique_ptr<iscore::PanelDelegate>
  make(const iscore::ApplicationContext& ctx) override;
};
}
