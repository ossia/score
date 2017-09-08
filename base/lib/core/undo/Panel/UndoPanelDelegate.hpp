#pragma once
#include <score/plugins/panel/PanelDelegate.hpp>

namespace score
{
class UndoListWidget;
class UndoPanelDelegate final : public score::PanelDelegate
{
public:
  UndoPanelDelegate(const score::GUIApplicationContext& ctx);

private:
  QWidget* widget() override;

  const PanelStatus& defaultPanelStatus() const override;

  void on_modelChanged(MaybeDocument oldm, MaybeDocument newm) override;

  score::UndoListWidget* m_list{};
  QWidget* m_widget{};
};
}
