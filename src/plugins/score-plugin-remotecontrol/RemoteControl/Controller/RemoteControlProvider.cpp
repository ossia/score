#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>

#include <core/document/DocumentView.hpp>

#include <QMainWindow>
#include <QWheelEvent>

#include <RemoteControl/Controller/DocumentPlugin.hpp>
#include <RemoteControl/Controller/RemoteControlProvider.hpp>
namespace RemoteControl::Controller
{

RemoteControlImpl::RemoteControlImpl(DocumentPlugin& self)
    : doc{self}
{
}

std::vector<Process::RemoteControl::ControllerHandle>
RemoteControlImpl::registerControllerGroup(ControllerHint hint, int count)
{
  return doc.registerControllerGroup(*this, hint, count);
}

void RemoteControlImpl::sendKey(
    ControllerAction act, Qt::Key k, Qt::KeyboardModifiers mods)
{
  auto t = act == ControllerAction::Press ? QKeyEvent::Type::KeyPress
                                          : QKeyEvent::Type::KeyRelease;
  QKeyEvent e{t, k, {}};
  QCoreApplication::sendEvent(doc.context().app.mainWindow, &e);
}

void RemoteControlImpl::left(ControllerAction act)
{
  sendKey(act, Qt::Key_Left);
}

void RemoteControlImpl::right(ControllerAction act)
{
  sendKey(act, Qt::Key_Right);
}

void RemoteControlImpl::up(ControllerAction act)
{
  sendKey(act, Qt::Key_Up);
}

void RemoteControlImpl::down(ControllerAction act)
{
  sendKey(act, Qt::Key_Down);
}

void RemoteControlImpl::save(ControllerAction act)
{
  sendKey(act, Qt::Key_S, Qt::ControlModifier);
}

void RemoteControlImpl::ok(ControllerAction act) { }

void RemoteControlImpl::cancel(ControllerAction act) { }

void RemoteControlImpl::enter(ControllerAction act)
{
  sendKey(act, Qt::Key_Enter);
}

void RemoteControlImpl::undo(ControllerAction act)
{
  this->doc.editContext.undo();
}

void RemoteControlImpl::redo(ControllerAction act)
{
  this->doc.editContext.redo();
}

void RemoteControlImpl::play(ControllerAction act)
{
  this->doc.editContext.play();
}

void RemoteControlImpl::pause(ControllerAction act)
{
  this->doc.editContext.pause();
}

void RemoteControlImpl::resume(ControllerAction act)
{
  this->doc.editContext.resume();
}

void RemoteControlImpl::stop(ControllerAction act)
{
  this->doc.editContext.stop();
}

void RemoteControlImpl::record(ControllerAction act) { }

void RemoteControlImpl::solo(ControllerAction act) { }

void RemoteControlImpl::mute(ControllerAction act) { }

void RemoteControlImpl::select(ControllerAction act) { }

void RemoteControlImpl::zoom(double zx, double zy)
{
  auto main_view = qobject_cast<Scenario::ScenarioDocumentView*>(
      &this->doc.context().document.view()->viewDelegate());
  if(!main_view)
    return;

  main_view->zoom(zx, zy);
}

void RemoteControlImpl::scroll(double sx, double sy)
{
  auto main_view = qobject_cast<Scenario::ScenarioDocumentView*>(
      &this->doc.context().document.view()->viewDelegate());
  if(!main_view)
    return;

  main_view->scroll(sx, sy);
}

void RemoteControlImpl::scrub(double z)
{
  this->doc.editContext.scrub(z);
}

void RemoteControlImpl::prevBank(ControllerAction act)
{
  if(act == ControllerAction::Press)
    doc.prevBank(*this);
}

void RemoteControlImpl::nextBank(ControllerAction act)
{
  if(act == ControllerAction::Press)
    doc.nextBank(*this);
}

void RemoteControlImpl::prevChannel(ControllerAction act)
{
  if(act == ControllerAction::Press)
    doc.prevChannel(*this);
}

void RemoteControlImpl::nextChannel(ControllerAction act)
{
  if(act == ControllerAction::Press)
    doc.nextChannel(*this);
}

void RemoteControlImpl::setControl(ControllerHandle index, const ossia::value& val)
{
  doc.setControl(*this, index, val);
}

void RemoteControlImpl::offsetControl(ControllerHandle index, double val)
{
  doc.offsetControl(*this, index, val);
}

std::shared_ptr<Process::RemoteControl>
RemoteControlProvider::make(const score::DocumentContext& ctx)
{
  auto* doc = ctx.findPlugin<DocumentPlugin>();
  if(!doc)
    return {};
  return doc->acquireRemoteControlInterface();
}

void RemoteControlProvider::release(
    const score::DocumentContext& ctx, std::shared_ptr<Process::RemoteControl> impl)
{
  auto* doc = ctx.findPlugin<DocumentPlugin>();
  if(!doc)
    return;
  doc->releaseRemoteControlInterface(impl);
}
}
