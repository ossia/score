#pragma once
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/plugins/panel/PanelDelegateFactory.hpp>

namespace Scenario
{

class PanelDelegateFactory final : public score::PanelDelegateFactory
{
  SCORE_CONCRETE("c255f3db-3758-4d99-961d-76c1ffffc646")

  std::unique_ptr<score::PanelDelegate>
  make(const score::GUIApplicationContext& ctx) override;
};
}
