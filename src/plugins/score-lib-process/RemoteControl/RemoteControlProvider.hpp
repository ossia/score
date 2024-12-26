#pragma once
#include <score/plugins/Interface.hpp>
#include <score/plugins/InterfaceList.hpp>

#include <ossia-qt/time_value.hpp>
#include <ossia-qt/value_metatypes.hpp>

#include <QObject>

#include <score_lib_process_export.h>

#include <verdigris>
namespace score
{
struct DocumentContext;
}

namespace Process
{

class SCORE_LIB_PROCESS_EXPORT RemoteControl : public QObject
{
  W_OBJECT(RemoteControl)
public:
  RemoteControl();
  ~RemoteControl();

  // Device setup
  enum ControllerHint
  {
    // clang-format off
    None            = 0b0000'0000,
    Knob            = 0b0000'0001,
    Fader           = 0b0000'0010,
    Wheel           = 0b0000'0011,
    Button_Knob     = 0b0000'0100,
    Button_Function = 0b0000'0101,
    Button_Rec      = 0b0000'0110,
    Button_Solo     = 0b0000'0111,
    Button_Mute     = 0b0000'1000,
    Button_Select   = 0b0000'1001,
    Generic         = 0b0001'1111,

    MapControls     = 0b0010'0000,
    // clang-format on
  };
  W_FLAG(
      ControllerHint, None, Knob, Fader, Wheel, Button_Knob, Button_Function, Button_Rec,
      Button_Solo, Button_Mute, Button_Select, Generic, MapControls);

  enum ControllerAction
  {
    Press,
    Release,
    Click
  };

  using ControllerHandle = std::intptr_t;

  virtual std::vector<ControllerHandle>
  registerControllerGroup(ControllerHint hint, int count) = 0;

  // Device -> score
  virtual void left(ControllerAction);
  virtual void right(ControllerAction);
  virtual void up(ControllerAction);
  virtual void down(ControllerAction);

  virtual void save(ControllerAction);
  virtual void ok(ControllerAction);
  virtual void cancel(ControllerAction);
  virtual void enter(ControllerAction);

  virtual void undo(ControllerAction);
  virtual void redo(ControllerAction);

  virtual void play(ControllerAction);
  virtual void pause(ControllerAction);
  virtual void resume(ControllerAction);
  virtual void stop(ControllerAction);
  virtual void record(ControllerAction);

  virtual void solo(ControllerAction);
  virtual void mute(ControllerAction);
  virtual void select(ControllerAction);

  virtual void zoom(double zoom_x, double zoom_y);
  virtual void scroll(double scroll_x, double scroll_y);
  virtual void scrub(double z);

  virtual void prevBank(ControllerAction);
  virtual void nextBank(ControllerAction);
  virtual void prevChannel(ControllerAction);
  virtual void nextChannel(ControllerAction);

  // Set the value
  virtual void setControl(ControllerHandle index, const ossia::value& val);

  // For numeric controls, applies val as a proportional offset.
  virtual void offsetControl(ControllerHandle index, double val);

  // score -> Device
  void controlNameChanged(ControllerHandle index, QString name)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, controlNameChanged, index, name);
  void controlValueChanged(ControllerHandle index, ossia::value value)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, controlValueChanged, index, value);

  void transportChanged(
      ossia::time_value flicks, double bar, double beat, double sub, double tick)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, transportChanged, flicks, bar, beat, sub, tick);
};

class SCORE_LIB_PROCESS_EXPORT RemoteControlProvider : public score::InterfaceBase
{
  SCORE_INTERFACE(RemoteControlProvider, "50c57dfc-c31a-11ef-9af3-5ce91ee31bcd")
public:
  ~RemoteControlProvider();
  virtual std::shared_ptr<RemoteControl> make(const score::DocumentContext& ctx) = 0;
  virtual void release(const score::DocumentContext& ctx, std::shared_ptr<RemoteControl>)
      = 0;
};

class RemoteControlProviderList final
    : public score::InterfaceList<RemoteControlProvider>
{
};
}
