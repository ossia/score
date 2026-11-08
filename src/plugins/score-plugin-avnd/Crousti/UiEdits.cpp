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
  if(m_stack)
    commit();
  else
    for(auto& e : m_cmds)
      delete e.cmd;
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
    if(v == port.value())
      return;
    begin();
    auto cmd = new Command{port, std::move(v)};
    m_cmds.push_back({&port, cmd});
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

void UiEdits::begin()
{
  if(!m_cmds.empty())
    return;
  m_ctx.commandStack.disableActions();
  m_aboutToChange = connect(
      m_stack.data(), &score::CommandStack::aboutToChange, this, [this] { commit(); });
}

void UiEdits::scheduleCommit()
{
  if(m_scheduled || m_waitingForRelease)
    return;
  m_scheduled = true;
  QTimer::singleShot(0, this, [this] {
    m_scheduled = false;
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

bool UiEdits::eventFilter(QObject* watched, QEvent* event)
{
  switch(event->type())
  {
    case QEvent::MouseButtonRelease:
    case QEvent::GraphicsSceneMouseRelease:
      // After the item has handled the release, which may edit too
      if(QGuiApplication::mouseButtons() == Qt::NoButton)
      {
        qApp->removeEventFilter(this);
        m_waitingForRelease = false;
        scheduleCommit();
      }
      break;
    case QEvent::MouseButtonPress:
    case QEvent::KeyPress:
      // A release this filter missed, e.g. taken by a drag and drop: the
      // gesture is over once another one starts
      if(m_waitingForRelease)
      {
        qApp->removeEventFilter(this);
        m_waitingForRelease = false;
        commit();
      }
      break;
    default:
      break;
  }
  return QObject::eventFilter(watched, event);
}

void UiEdits::commit()
{
  if(m_waitingForRelease)
  {
    qApp->removeEventFilter(this);
    m_waitingForRelease = false;
  }
  if(m_cmds.empty())
    return;

  // Pushing emits aboutToChange too
  disconnect(m_aboutToChange);
  auto cmds = std::exchange(m_cmds, {});
  if(!m_stack)
  {
    for(auto& e : cmds)
      delete e.cmd;
    return;
  }

  m_ctx.commandStack.enableActions();
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
}
