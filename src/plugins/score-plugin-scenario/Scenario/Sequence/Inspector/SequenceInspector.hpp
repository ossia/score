#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

#include <Scenario/Sequence/SequenceModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <QWidget>

class QListWidget;
class QVBoxLayout;

namespace Sequence
{

class InspectorWidget final
    : public Process::InspectorWidgetDelegate_T<SequenceModel>
{
public:
  explicit InspectorWidget(
      const SequenceModel& model, const score::DocumentContext& ctx,
      QWidget* parent);

protected:
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;

private:
  void rebuildList();

  const score::DocumentContext& m_ctx;
  QListWidget* m_listWidget{};
  CommandDispatcher<> m_dispatcher;
};

class InspectorFactory final
    : public Process::InspectorWidgetDelegateFactory_T<SequenceModel, InspectorWidget>
{
  SCORE_CONCRETE("b2a7f3c1-4e9d-4a2b-8c3f-1d5e7a9b0c2e")
};

}
