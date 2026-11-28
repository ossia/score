#pragma once
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/plugins/panel/PanelDelegateFactory.hpp>

#include <QPointer>

namespace Execution
{
class ProfilerWidget;
class ProfilerPanel final : public score::PanelDelegate
{
public:
  explicit ProfilerPanel(const score::GUIApplicationContext& ctx);

  QWidget* widget() override;

private:
  const score::PanelStatus& defaultPanelStatus() const override;
  void on_modelChanged(score::MaybeDocument oldm, score::MaybeDocument newm) override;

  QWidget* m_widget{};
  QPointer<ProfilerWidget> m_cur;
};

class ProfilerPanelFactory final : public score::PanelDelegateFactory
{
  SCORE_CONCRETE("4c1d2b8e-6f3a-4e57-9a0b-7d2c5e8f1a36")

  std::unique_ptr<score::PanelDelegate>
  make(const score::GUIApplicationContext& ctx) override;
};
}
