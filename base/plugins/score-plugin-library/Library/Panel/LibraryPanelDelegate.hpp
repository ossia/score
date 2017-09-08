#pragma once
#include <score/plugins/panel/PanelDelegate.hpp>
class QTabWidget;
namespace Library
{
class PanelDelegate final : public score::PanelDelegate
{
public:
  PanelDelegate(const score::GUIApplicationContext& ctx);

private:
  QWidget* widget() override;

  const score::PanelStatus& defaultPanelStatus() const override;

  QTabWidget* m_widget{};
};
}
