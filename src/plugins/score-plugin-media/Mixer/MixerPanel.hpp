#pragma once
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/plugins/panel/PanelDelegateFactory.hpp>

namespace Dataflow
{
class PortItem;
}
namespace Mixer
{
class MixerPanel;
class PanelDelegate final : public score::PanelDelegate
{
public:
  PanelDelegate(const score::GUIApplicationContext& ctx);

  QWidget* widget() override;

private:
  const score::PanelStatus& defaultPanelStatus() const override;

  void on_modelChanged(score::MaybeDocument oldm, score::MaybeDocument newm) override;

  QWidget* m_widget{};
  MixerPanel* m_cur{};
};

class PanelDelegateFactory final : public score::PanelDelegateFactory
{
  SCORE_CONCRETE("b7f7bf9f-3742-4c52-a6aa-41511a09e4a5")

  std::unique_ptr<score::PanelDelegate> make(const score::GUIApplicationContext& ctx) override;
};
}
