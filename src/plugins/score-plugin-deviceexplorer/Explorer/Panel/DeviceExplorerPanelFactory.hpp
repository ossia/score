#pragma once
#include <score/plugins/panel/PanelDelegateFactory.hpp>
namespace Explorer
{

class PanelDelegateFactory final : public score::PanelDelegateFactory
{
  SCORE_CONCRETE("de755995-398d-4b16-9030-574533b17a9f")

  std::unique_ptr<score::PanelDelegate> make(const score::GUIApplicationContext& ctx) override;
};
}
