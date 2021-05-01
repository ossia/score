#pragma once
#include <score/plugins/panel/PanelDelegateFactory.hpp>

namespace InspectorPanel
{

class PanelDelegateFactory final : public score::PanelDelegateFactory
{
  SCORE_CONCRETE("3c489368-c946-4f9f-8d6c-d051b724726c")

  std::unique_ptr<score::PanelDelegate>
  make(const score::GUIApplicationContext& ctx) override;
};
}
