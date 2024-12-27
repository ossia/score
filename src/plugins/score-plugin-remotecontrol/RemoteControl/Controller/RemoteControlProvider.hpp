#pragma once

#include <RemoteControl/RemoteControlProvider.hpp>
namespace RemoteControl::Controller
{
class DocumentPlugin;
class RemoteControlImpl : public Process::RemoteControlInterface
{
public:
  using ControllerHandle = Process::RemoteControlInterface::ControllerHandle;
  using ControllerAction = Process::RemoteControlInterface::ControllerAction;

  ::RemoteControl::Controller::DocumentPlugin& doc;
  explicit RemoteControlImpl(Controller::DocumentPlugin& self);

  std::vector<Process::RemoteControlInterface::ControllerHandle>
  registerControllerGroup(ControllerHint hint, int count) override;

  void sendKey(ControllerAction act, Qt::Key k, Qt::KeyboardModifiers mods = {});
  void left(ControllerAction) override;
  void right(ControllerAction) override;
  void up(ControllerAction) override;
  void down(ControllerAction) override;

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
  void zoom(double zoom_x, double zoom_y) override;
  void scroll(double scroll_x, double scroll_y) override;
  void scrub(double z) override;

  void prevBank(ControllerAction) override;
  void nextBank(ControllerAction) override;
  void prevChannel(ControllerAction) override;
  void nextChannel(ControllerAction) override;

  void setControl(ControllerHandle index, const ossia::value& val) override;
  void offsetControl(ControllerHandle index, double val) override;
};

class RemoteControlProvider final : public Process::RemoteControlProvider
{
  SCORE_CONCRETE("ac63880f-ce30-489d-a93a-869578ade25a")
  std::shared_ptr<Process::RemoteControlInterface>
  make(const score::DocumentContext& ctx) override;
  void release(
      const score::DocumentContext& ctx,
      std::shared_ptr<Process::RemoteControlInterface>) override;
};
}
