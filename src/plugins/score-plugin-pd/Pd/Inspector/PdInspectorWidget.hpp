#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <Pd/Commands/EditPd.hpp>
#include <Pd/PdProcess.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/widgets/MarginLess.hpp>

#include <QCheckBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>
namespace Pd
{

class PdWidget final : public Process::InspectorWidgetDelegate_T<Pd::ProcessModel>
{
  W_OBJECT(PdWidget)
public:
  explicit PdWidget(
      const Pd::ProcessModel& object, const score::DocumentContext& context,
      QWidget* parent);

  void pressed() W_SIGNAL(pressed);
  void contextMenuRequested(QPoint p) W_SIGNAL(contextMenuRequested, p);

private:
  void reinit();
  void on_patchChange(const QString& newText);

  CommandDispatcher<> m_disp;
  const Pd::ProcessModel& m_proc;
  Explorer::DeviceExplorerModel& m_explorer;

  QLineEdit m_ledit;
  score::MarginLess<QVBoxLayout> m_lay;
  QWidget m_portwidg;

  score::MarginLess<QFormLayout> m_sublay;
  QSpinBox m_audioIn, m_audioOut;
  QCheckBox m_midiIn, m_midiOut;
};

class InspectorFactory final
    : public Process::InspectorWidgetDelegateFactory_T<ProcessModel, PdWidget>
{
  SCORE_CONCRETE("ac3f1317-1381-4a19-a10f-2e7ae711bf58")
};
}
