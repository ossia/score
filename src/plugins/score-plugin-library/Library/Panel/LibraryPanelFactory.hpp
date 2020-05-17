#pragma once
#include <score/plugins/panel/PanelDelegateFactory.hpp>

namespace Library
{
class UserPanelFactory final : public score::PanelDelegateFactory
{
  SCORE_CONCRETE("ddbc5169-1ca3-4a64-a805-40b8fc0e1e02")

  std::unique_ptr<score::PanelDelegate> make(const score::GUIApplicationContext& ctx) override;
};
class ProjectPanelFactory final : public score::PanelDelegateFactory
{
  SCORE_CONCRETE("82eed25c-32df-4950-860b-128ee17779d8")

  std::unique_ptr<score::PanelDelegate> make(const score::GUIApplicationContext& ctx) override;
};
class ProcessPanelFactory final : public score::PanelDelegateFactory
{
  SCORE_CONCRETE("9331ff3d-1130-4859-87f4-112cab25eb11")

  std::unique_ptr<score::PanelDelegate> make(const score::GUIApplicationContext& ctx) override;
};
}
