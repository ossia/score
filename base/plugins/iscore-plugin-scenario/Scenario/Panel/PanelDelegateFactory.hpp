#pragma once
#include <iscore/plugins/panel/PanelDelegate.hpp>
#include <iscore/plugins/panel/PanelDelegateFactory.hpp>

namespace Scenario
{

class PanelDelegateFactory final : public iscore::PanelDelegateFactory
{
  ISCORE_CONCRETE_FACTORY("c255f3db-3758-4d99-961d-76c1ffffc646")

  std::unique_ptr<iscore::PanelDelegate>
  make(const iscore::ApplicationContext& ctx) override;
};
}
