#include <RemoteControl/RemoteControlProvider.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Process::RemoteControlInterface)

namespace Process
{
RemoteControlInterface::RemoteControlInterface() = default;
RemoteControlInterface::~RemoteControlInterface() = default;
RemoteControlProvider::~RemoteControlProvider() = default;

void RemoteControlInterface::left(ControllerAction) { }

void RemoteControlInterface::right(ControllerAction) { }

void RemoteControlInterface::up(ControllerAction) { }

void RemoteControlInterface::down(ControllerAction) { }

void RemoteControlInterface::save(ControllerAction) { }

void RemoteControlInterface::ok(ControllerAction) { }

void RemoteControlInterface::cancel(ControllerAction) { }

void RemoteControlInterface::enter(ControllerAction) { }

void RemoteControlInterface::undo(ControllerAction) { }

void RemoteControlInterface::redo(ControllerAction) { }

void RemoteControlInterface::play(ControllerAction) { }

void RemoteControlInterface::pause(ControllerAction) { }

void RemoteControlInterface::resume(ControllerAction) { }

void RemoteControlInterface::stop(ControllerAction) { }

void RemoteControlInterface::record(ControllerAction) { }

void RemoteControlInterface::solo(ControllerAction) { }

void RemoteControlInterface::mute(ControllerAction) { }

void RemoteControlInterface::select(ControllerAction) { }

void RemoteControlInterface::zoom(double, double) { }
void RemoteControlInterface::scroll(double, double) { }
void RemoteControlInterface::scrub(double) { }

void RemoteControlInterface::prevBank(ControllerAction) { }
void RemoteControlInterface::nextBank(ControllerAction) { }
void RemoteControlInterface::prevChannel(ControllerAction) { }
void RemoteControlInterface::nextChannel(ControllerAction) { }

void RemoteControlInterface::setControl(ControllerHandle index, const ossia::value& val) { }
void RemoteControlInterface::offsetControl(ControllerHandle index, double val) { }
}
