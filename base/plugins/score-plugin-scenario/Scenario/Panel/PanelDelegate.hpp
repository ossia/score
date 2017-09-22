#pragma once
#include <score/plugins/panel/PanelDelegate.hpp>

namespace Process
{
class ProcessModel;
class LayerPanelProxy;
}

namespace Scenario
{
class PanelDelegate final : public QObject, public score::PanelDelegate
{
public:
  PanelDelegate(const score::GUIApplicationContext& ctx);

private:
  QWidget* widget() override;

  const score::PanelStatus& defaultPanelStatus() const override;

  void on_modelChanged(
      score::MaybeDocument oldm, score::MaybeDocument newm) override;

  void on_focusedViewModelChanged(const Process::ProcessModel* theLM);
  void on_focusedViewModelRemoved(const Process::ProcessModel* theLM);

  void cleanup();

  QWidget* m_widget{};
  QPointer<const Process::ProcessModel> m_layerModel{};
  Process::LayerPanelProxy* m_proxy{};

  std::vector<QMetaObject::Connection> m_connections;
};
}
