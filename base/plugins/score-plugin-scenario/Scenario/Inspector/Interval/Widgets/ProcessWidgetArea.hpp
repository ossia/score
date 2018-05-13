#pragma once
#include <Inspector/InspectorSectionWidget.hpp>
#include <wobjectdefs.h>
#include <Process/Process.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>

namespace Process
{
class ProcessModel;
}
namespace Scenario
{
class ProcessWidgetArea final : public Inspector::InspectorSectionWidget
{
  W_OBJECT(ProcessWidgetArea)
public:
  template <typename... Args>
  ProcessWidgetArea(
      const Process::ProcessModel& proc,
      CommandDispatcher<>& disp,
      Args&&... args)
      : InspectorSectionWidget{std::forward<Args>(args)...}
      , m_proc{proc}
      , m_disp{disp}
  {
    setAcceptDrops(true);

    connect(
        this, &ProcessWidgetArea::sig_performSwap, this,
        &ProcessWidgetArea::performSwap, Qt::QueuedConnection);

    connect(
        this, &ProcessWidgetArea::sig_putAtEnd, this,
        &ProcessWidgetArea::putAtEnd, Qt::QueuedConnection);

    connect(
        this, &ProcessWidgetArea::sig_handleSwap, this,
        &ProcessWidgetArea::handleSwap);
  }

private:
  void mousePressEvent(QMouseEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dropEvent(QDropEvent* event) override;

public:
  void sig_handleSwap(
      QPointer<const Process::ProcessModel> cst, double center, double y) W_SIGNAL(sig_handleSwap, cst, center, y);
  void sig_performSwap(
      QPointer<const Scenario::IntervalModel> cst,
      const Id<Process::ProcessModel>& id1,
      const Id<Process::ProcessModel>& id2) W_SIGNAL(sig_performSwap, cst, id1, id2);
  void sig_putAtEnd(
      QPointer<const Scenario::IntervalModel> cst,
      const Id<Process::ProcessModel>& id1) W_SIGNAL(sig_putAtEnd, cst, id1);

private:
  void performSwap(
      QPointer<const Scenario::IntervalModel> cst,
      const Id<Process::ProcessModel>& id1,
      const Id<Process::ProcessModel>& id2); W_SLOT(performSwap);
  void putAtEnd(
      QPointer<const Scenario::IntervalModel> cst,
      const Id<Process::ProcessModel>& id1); W_SLOT(putAtEnd);

  void handleSwap(
      QPointer<const Process::ProcessModel> cst, double center, double y); W_SLOT(handleSwap);

private:
  const Process::ProcessModel& m_proc;
  CommandDispatcher<>& m_disp;
};
}
