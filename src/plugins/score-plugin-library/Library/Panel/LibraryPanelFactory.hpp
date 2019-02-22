#pragma once
#include <score/plugins/panel/PanelDelegateFactory.hpp>

namespace Library
{
class PanelDelegateFactory final : public score::PanelDelegateFactory
{
  SCORE_CONCRETE("ddbc5169-1ca3-4a64-a805-40b8fc0e1e02")

  std::unique_ptr<score::PanelDelegate>
  make(const score::GUIApplicationContext& ctx) override;
};
}
