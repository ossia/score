#pragma once
#include <score/plugins/panel/PanelDelegate.hpp>

#include <QLabel>
#include <QVBoxLayout>

namespace score
{

class ScriptPanelDelegate : public PanelDelegate
{
public:
  ScriptPanelDelegate(const score::GUIApplicationContext& ctx)
      : PanelDelegate{ctx}
  {
    widg = new QWidget;
  }

  QWidget* widget() override { return widg; }

  const PanelStatus& defaultPanelStatus() const override
  {
    static const PanelStatus stat{
        false,    true,     Qt::RightDockWidgetArea,     -100000,
        "Script", "script", QKeySequence("Ctrl+Shift+S")};
    return stat;
  }
  QWidget* widg{};
};
}
