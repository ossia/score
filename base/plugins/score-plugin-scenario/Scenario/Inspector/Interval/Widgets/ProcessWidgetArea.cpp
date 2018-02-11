// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ProcessWidgetArea.hpp"
#include <Process/Process.hpp>
#include <Scenario/Commands/Interval/SetProcessPosition.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/serialization/VisitorCommon.hpp>
#include <score/model/path/PathSerialization.hpp>

#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QLabel>
#include <QMimeData>
namespace Scenario
{
// MOVEME
template <typename T>
auto id(const Path<T>& path)
{
  SCORE_ASSERT(path.valid());
  SCORE_ASSERT(bool(path.unsafePath().vec().back().id()));

  return Id<T>(path.unsafePath().vec().back().id());
}

void ProcessWidgetArea::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton)
  {

    auto drag = new QDrag(this->parentWidget());
    auto mimeData = new QMimeData;

    mimeData->setData(
        "application/x-score-processdrag",
        score::marshall<DataStream>(make_path(m_proc)));
    drag->setMimeData(mimeData);
    QLabel label{m_proc.metadata().getName()};
    drag->setPixmap(label.grab());
    drag->setHotSpot(label.rect().center());

    drag->exec();
  }
}

void ProcessWidgetArea::dragEnterEvent(QDragEnterEvent* event)
{
  if (!event->mimeData()->hasFormat("application/x-score-processdrag"))
    return;

  auto path = score::unmarshall<Path<Process::ProcessModel>>(
      event->mimeData()->data("application/x-score-processdrag"));
  auto res = path.try_find(m_disp.stack().context());
  if (!res)
    return;

  if (res == &m_proc)
    return;

  if (res->parent() != m_proc.parent())
    return;

  event->acceptProposedAction();
}

void ProcessWidgetArea::dragMoveEvent(QDragMoveEvent* event)
{
  // TODO : show a bar to indicate
  // graphically where the drop will occur.
}

void ProcessWidgetArea::dropEvent(QDropEvent* event)
{
  // Accept
  event->acceptProposedAction();

  // Get the process
  auto path = score::unmarshall<Path<Process::ProcessModel>>(
      event->mimeData()->data("application/x-score-processdrag"));

  // Position
  auto center = rect().center().y();
  auto y = event->pos().y();

  handleSwap(path.try_find(m_disp.stack().context()), center, y);
}

void ProcessWidgetArea::performSwap(
    QPointer<const Scenario::IntervalModel> cst,
    const Id<Process::ProcessModel>& id1,
    const Id<Process::ProcessModel>& id2)
{
  if(cst)
  {
    // Create a command to swap both processes
    m_disp.submitCommand(
          new Command::PutProcessBefore{*cst, id1, id2});
  }
}

void ProcessWidgetArea::putAtEnd(
    QPointer<const Scenario::IntervalModel> cst, const Id<Process::ProcessModel>& id1)
{
  if(cst)
  {
    m_disp.submitCommand(new Command::PutProcessToEnd{*cst, id1});
  }
}

void ProcessWidgetArea::handleSwap(
    QPointer<const Process::ProcessModel> path, double center, double y)
{
  auto cst = safe_cast<Scenario::IntervalModel*>(m_proc.parent());
  if (y < center)
  {
    // Drop before this
    sig_performSwap(cst, m_proc.id(), path->id());
  }
  else
  {
    // Drop after this
    auto next_proc_it = cst->processes.find(m_proc.id());
    std::advance(next_proc_it, 1);
    if (next_proc_it != cst->processes.end())
    {
      // Drop before next process
      sig_performSwap(cst, next_proc_it->id(), path->id());
    }
    else
    {
      // Drop at end
      sig_putAtEnd(cst, path->id());
    }
  }
}
}
