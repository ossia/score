#include <RemoteControl/RemoteControlProvider.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Process::RemoteControl)

namespace Process
{
RemoteControl::RemoteControl() = default;
RemoteControl::~RemoteControl() = default;
RemoteControlProvider::~RemoteControlProvider() = default;

void RemoteControl::left(ControllerAction) { }

void RemoteControl::right(ControllerAction) { }

void RemoteControl::up(ControllerAction) { }

void RemoteControl::down(ControllerAction) { }

void RemoteControl::save(ControllerAction) { }

void RemoteControl::ok(ControllerAction) { }

void RemoteControl::cancel(ControllerAction) { }

void RemoteControl::enter(ControllerAction) { }

void RemoteControl::undo(ControllerAction) { }

void RemoteControl::redo(ControllerAction) { }

void RemoteControl::play(ControllerAction) { }

void RemoteControl::pause(ControllerAction) { }

void RemoteControl::resume(ControllerAction) { }

void RemoteControl::stop(ControllerAction) { }

void RemoteControl::record(ControllerAction) { }

void RemoteControl::solo(ControllerAction) { }

void RemoteControl::mute(ControllerAction) { }

void RemoteControl::select(ControllerAction) { }

void RemoteControl::zoom(double, double) { }
void RemoteControl::scroll(double, double) { }
void RemoteControl::scrub(double) { }

void RemoteControl::prevBank(ControllerAction) { }
void RemoteControl::nextBank(ControllerAction) { }
void RemoteControl::prevChannel(ControllerAction) { }
void RemoteControl::nextChannel(ControllerAction) { }

void RemoteControl::setControl(ControllerHandle index, const ossia::value& val) { }
void RemoteControl::offsetControl(ControllerHandle index, double val) { }
}
