#pragma once

#include <RemoteControl/RemoteControlProvider.hpp>
namespace RemoteControl::Controller
{
class DocumentPlugin;
class RemoteControlImpl : public Process::RemoteControl
{
public:
  using ControllerHandle = Process::RemoteControl::ControllerHandle;
  using ControllerAction = Process::RemoteControl::ControllerAction;

  DocumentPlugin& doc;
  explicit RemoteControlImpl(Controller::DocumentPlugin& self);

  std::vector<Process::RemoteControl::ControllerHandle>
  registerControllerGroup(ControllerHint hint, int count) override;
  void left(ControllerAction) override;
  void right(ControllerAction) override;
  void up(ControllerAction) override;
  void down(ControllerAction) override;

  void shift(ControllerAction) override;
  void alt(ControllerAction) override;
  void option(ControllerAction) override;
  void control(ControllerAction) override;

  void save(ControllerAction) override;
  void ok(ControllerAction) override;
  void cancel(ControllerAction) override;
  void enter(ControllerAction) override;

  void undo(ControllerAction) override;
  void redo(ControllerAction) override;

  void play(ControllerAction) override;
  void pause(ControllerAction) override;
  void resume(ControllerAction) override;
  void stop(ControllerAction) override;
  void record(ControllerAction) override;

  void solo(ControllerAction) override;
  void mute(ControllerAction) override;
  void select(ControllerAction) override;

  void setControl(ControllerHandle index, const ossia::value& val) override;
  void offsetControl(ControllerHandle index, double val) override;
};

class RemoteControlProvider final : public Process::RemoteControlProvider
{
  SCORE_CONCRETE("ac63880f-ce30-489d-a93a-869578ade25a")
  std::shared_ptr<Process::RemoteControl>
  make(const score::DocumentContext& ctx) override;
  void release(
      const score::DocumentContext& ctx,
      std::shared_ptr<Process::RemoteControl>) override;
};
}
