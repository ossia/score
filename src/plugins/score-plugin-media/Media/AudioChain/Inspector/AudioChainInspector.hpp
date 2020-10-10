#pragma once
#include <Media/AudioChain/AudioChainModel.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <nano_observer.hpp>
#include <QMenu>
class QListWidget;
class QPushButton;

namespace Media::AudioChain
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
  void add_score(std::size_t pos);
  void add_lv2(std::size_t pos);
  void add_vst2(std::size_t pos);
  void add_faust(std::size_t pos);
  void recreate();
  void on_effectAdded(const Process::ProcessModel& p);
  void on_effectRemoved(const Process::ProcessModel& p);
  void on_orderChanged();
  QListWidget* m_list{};
  CommandDispatcher<> m_dispatcher;
  const score::DocumentContext& m_ctx;
  void addRequested(int pos);
  int cur_pos();

  QMenu m_contextMenu;
};

class InspectorFactory final
    : public Process::InspectorWidgetDelegateFactory_T<ProcessModel, InspectorWidget>
{
  SCORE_CONCRETE("cc8ceff3-ef93-4b73-865a-a9f870d6e898")
};
}
