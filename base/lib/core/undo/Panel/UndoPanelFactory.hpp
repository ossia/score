#pragma once
#include <score/plugins/panel/PanelDelegateFactory.hpp>

namespace score
{

class UndoPanelDelegateFactory final : public PanelDelegateFactory
{
  SCORE_CONCRETE("293c0f8b-fcb4-4554-8425-4bc03d803c75")

  std::unique_ptr<PanelDelegate>
  make(const score::GUIApplicationContext& ctx) override;
};
}
