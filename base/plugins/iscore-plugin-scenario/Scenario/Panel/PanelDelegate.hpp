#pragma once
#include <iscore/plugins/panel/PanelDelegate.hpp>

namespace Process
{
class ProcessModel;
class LayerPanelProxy;
}

namespace Scenario
{
class PanelDelegate final : public QObject, public iscore::PanelDelegate
{
public:
  PanelDelegate(const iscore::GUIApplicationContext& ctx);

private:
  QWidget* widget() override;

  const iscore::PanelStatus& defaultPanelStatus() const override;

  void on_modelChanged(
      iscore::MaybeDocument oldm, iscore::MaybeDocument newm) override;

  void on_focusedViewModelChanged(const Process::ProcessModel* theLM);
  void on_focusedViewModelRemoved(const Process::ProcessModel* theLM);

  void cleanup();

  QWidget* m_widget{};
  QPointer<const Process::ProcessModel> m_layerModel{};
  Process::LayerPanelProxy* m_proxy{};

  std::vector<QMetaObject::Connection> m_connections;
};
}
