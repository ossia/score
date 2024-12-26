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
void RemoteControlImpl::left(ControllerAction act) { }

void RemoteControlImpl::right(ControllerAction act) { }

void RemoteControlImpl::up(ControllerAction act) { }

void RemoteControlImpl::down(ControllerAction act) { }

void RemoteControlImpl::shift(ControllerAction act) { }

void RemoteControlImpl::alt(ControllerAction act) { }

void RemoteControlImpl::option(ControllerAction act) { }

void RemoteControlImpl::control(ControllerAction act) { }

void RemoteControlImpl::save(ControllerAction act) { }

void RemoteControlImpl::ok(ControllerAction act) { }

void RemoteControlImpl::cancel(ControllerAction act) { }

void RemoteControlImpl::enter(ControllerAction act) { }

void RemoteControlImpl::undo(ControllerAction act) { }

void RemoteControlImpl::redo(ControllerAction act) { }

void RemoteControlImpl::play(ControllerAction act) { }

void RemoteControlImpl::pause(ControllerAction act) { }

void RemoteControlImpl::resume(ControllerAction act) { }

void RemoteControlImpl::stop(ControllerAction act) { }

void RemoteControlImpl::record(ControllerAction act) { }

void RemoteControlImpl::solo(ControllerAction act) { }

void RemoteControlImpl::mute(ControllerAction act) { }

void RemoteControlImpl::select(ControllerAction act) { }

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
