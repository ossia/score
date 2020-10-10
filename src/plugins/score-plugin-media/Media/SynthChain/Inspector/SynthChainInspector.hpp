#pragma once
#include <Media/SynthChain/SynthChainModel.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <nano_observer.hpp>
#include <QMenu>
class QListWidget;
class QPushButton;

namespace Media::SynthChain
{
class InspectorWidget final
    : public Process::InspectorWidgetDelegate_T<ProcessModel>
    , public Nano::Observer
{
public:
  explicit InspectorWidget(
      const ProcessModel& object,
      const score::DocumentContext& doc,
      QWidget* parent);

private:
  void recreate();
  QListWidget* m_list{};
  CommandDispatcher<> m_dispatcher;
  const score::DocumentContext& m_ctx;
  void addRequested(int pos);
  int cur_pos();
  void on_effectAdded(const Process::ProcessModel& p);
  void on_effectRemoved(const Process::ProcessModel& p);
  void on_orderChanged();

  QMenu m_contextMenu;
};

class InspectorFactory final
    : public Process::InspectorWidgetDelegateFactory_T<ProcessModel, InspectorWidget>
{
  SCORE_CONCRETE("44da7af5-420f-4471-8f09-1ce545c46005")
};
}
