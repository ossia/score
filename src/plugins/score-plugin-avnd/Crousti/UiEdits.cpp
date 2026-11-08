#include "UiEdits.hpp"

#include <score/command/Dispatchers/SendStrategy.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <QCoreApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QTimer>

#include <algorithm>

namespace oscr
{
UiEdits::UiEdits(const score::DocumentContext& ctx, QObject* parent)
    : QObject{parent}
    , m_ctx{ctx}
    , m_stack{&ctx.document.commandStack()}
{
}

UiEdits::~UiEdits()
{
  // The ui goes mid-gesture, e.g. with its process: what it did stays done
  m_gestures = 0;
  commit();
}

void UiEdits::set(Process::ControlInlet& port, ossia::value v)
{
  edit<Process::SetControlValue>(port, std::move(v));
}

void UiEdits::set(Process::ControlOutlet& port, ossia::value v)
{
  edit<Process::SetControlOutletValue>(port, std::move(v));
}

template <typename Command, typename Port>
void UiEdits::edit(Port& port, ossia::value v)
{
  if(m_reacting > 0 || !m_stack || m_stack->isReplaying())
    return;

  auto it = std::ranges::find(m_cmds, &port, &Edit::port);
  if(it == m_cmds.end())
  {
    // A click that moves nothing leaves no undo step
    if(v == port.value())
      return;
    open();
    auto cmd = new Command{port, std::move(v)};
    // A port that goes, e.g. as the ports of its process are made again,
    // could leave its address to another one
    auto destroyed = connect(&port, &QObject::destroyed, this, [this] { commit(); });
    m_cmds.push_back({&port, cmd, destroyed});
    cmd->redo(m_ctx);
  }
  else
  {
    auto cmd = static_cast<Command*>(it->cmd);
    cmd->update(port, std::move(v));
    cmd->redo(m_ctx);
  }
  scheduleCommit();
}

void UiEdits::beginGesture()
{
  ++m_gestures;
}

void UiEdits::endGesture()
{
  if(m_gestures > 0 && --m_gestures == 0)
    commit();
}

void UiEdits::open()
{
  if(m_open || !m_stack)
    return;
  m_open = true;
  m_ctx.commandStack.disableActions();
  m_stack->beginPendingEdit();
  m_aboutToChange = connect(
      m_stack.data(), &score::CommandStack::aboutToChange, this, [this] { commit(); });
}

void UiEdits::close()
{
  stopWaitingForRelease();
  disconnect(m_aboutToChange);
  for(auto& e : m_cmds)
    disconnect(e.destroyed);
  if(!m_open)
    return;
  m_open = false;
  if(m_stack)
  {
    m_ctx.commandStack.enableActions();
    m_stack->endPendingEdit();
  }
}

void UiEdits::scheduleCommit()
{
  // A transaction ends itself; else the event, or the drag
  if(m_gestures > 0 || m_scheduled || m_waitingForRelease)
    return;
  m_scheduled = true;
  QTimer::singleShot(0, this, [this] {
    m_scheduled = false;
    if(m_gestures > 0)
      return;
    if(QGuiApplication::mouseButtons() == Qt::NoButton)
    {
      commit();
    }
    else if(!m_waitingForRelease)
    {
      // A drag: its edits are pushed once it is over
      m_waitingForRelease = true;
      qApp->installEventFilter(this);
    }
  });
}

void UiEdits::stopWaitingForRelease()
{
  if(m_waitingForRelease)
  {
    qApp->removeEventFilter(this);
    m_waitingForRelease = false;
  }
}

bool UiEdits::eventFilter(QObject* watched, QEvent* event)
{
  switch(event->type())
  {
    case QEvent::MouseButtonRelease:
    case QEvent::GraphicsSceneMouseRelease:
      // After the item has handled the release, which may edit too
      if(QGuiApplication::mouseButtons() == Qt::NoButton)
      {
        stopWaitingForRelease();
        scheduleCommit();
      }
      break;
    case QEvent::MouseButtonPress:
    case QEvent::KeyPress:
      // A release this filter missed, e.g. taken by a drag and drop: the
      // gesture is over once another one starts
      if(m_waitingForRelease)
        commit();
      break;
    default:
      break;
  }
  return QObject::eventFilter(watched, event);
}

void UiEdits::commit()
{
  auto cmds = std::exchange(m_cmds, {});
  // Before pushing, which emits aboutToChange too
  for(auto& e : cmds)
    disconnect(e.destroyed);
  close();
  if(cmds.empty())
    return;

  if(!m_stack)
  {
    for(auto& e : cmds)
      delete e.cmd;
    return;
  }

  if(cmds.size() == 1)
  {
    SendStrategy::Quiet::send(m_ctx.commandStack, cmds.front().cmd);
  }
  else
  {
    auto all = new Process::SetControlValues;
    for(auto& e : cmds)
      all->addCommand(e.cmd);
    SendStrategy::Quiet::send(m_ctx.commandStack, all);
  }
}

void UiEdits::rollback()
{
  m_gestures = 0;
  auto cmds = std::exchange(m_cmds, {});
  for(auto& e : cmds)
    disconnect(e.destroyed);
  close();
  for(auto it = cmds.rbegin(); it != cmds.rend(); ++it)
  {
    if(m_stack)
      it->cmd->undo(m_ctx);
    delete it->cmd;
  }
}
}
