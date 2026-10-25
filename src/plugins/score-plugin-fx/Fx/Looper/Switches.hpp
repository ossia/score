#pragma once
#include <Fx/Looper/Sync.hpp>
#include <Fx/Looper/Transport.hpp>

#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>

/**
 * The actions of the loopers' Switches tab, made for footswitches.
 *
 * Loop is a value: setting it to the mode it already has does nothing, so
 * "overdub again", "a new layer" or "from the start" cannot be said with it.
 * An action is a press, and always acts. What it changes waits for the next
 * point of the grid, as a change of Loop does, and the controls it changes
 * (Loop, Reverse, Speed, Layer) show what it changed them to.
 *
 * The looper (the Engine below) provides, besides the transport's engine:
 *
 *   int looper_passes() const;                // passes heard
 *   bool looper_muted(int pass) const;
 *   void looper_set_muted(int pass, bool muted);
 *   void looper_mute(int pass);               // as Mute layer: toggles
 *   void looper_erase(int pass);              // as Erase layer
 */
namespace Nodes::Looper
{
//! What Rec / Play / Dub does while overdubbing until the setting is changed.
inline constexpr AfterOverdub after_overdub_default = AfterOverdub::Play;

//! The name Loop shows a mode by.
inline std::string_view mode_label(LoopMode m) noexcept
{
  static constexpr std::array<std::string_view, 5> names{
      "Play", "Record", "Overdub", "Stop", "Comp"};
  const auto i = std::size_t(m);
  return i < names.size() ? names[i] : names[3];
}

//! Sets a control from the audio thread, and shows it in the UI.
template <typename Field, typename V>
void show_control(Field& f, V v)
{
  f.value = v;
  if(f.update_controller)
    f.update_controller(v);
}

template <typename Field>
void show_mode(Field& f, LoopMode m)
{
  f.value = m;
  if(f.update_controller)
    f.update_controller(mode_label(m));
}

struct rec_play_dub_button : halp::impulse_button<"Rec / Play / Dub">
{
  halp_meta(
      description,
      "One button for the whole loop: records an empty loop, ends the recording "
      "into what After record says, overdubs a playing loop, and does what "
      "After overdub says while overdubbing. A stopped loop plays.")
};

struct after_record_selector : enum_combobox<"After record", AfterRecord, AfterRecord::Play>
{
  using enum_combobox::operator=;
  halp_meta(
      description,
      "What Rec / Play / Dub and Record go to when they end a recording: play the "
      "loop, or overdub it (or comp it) from the very frame the recording ends "
      "on, so that what is held goes on without a gap. A recording that ends "
      "after Bars goes to Post-action instead.")
};

struct stop_at_selector : enum_combobox<"Stop at", StopAt, StopAt::Grid>
{
  using enum_combobox::operator=;
  struct range
  {
    std::string_view values[3] = {"Grid", "Loop end", "Now"};
    StopAt init = StopAt::Grid;
  };
  halp_meta(
      description,
      "When Stop, Play / Stop and Loop set to Stop stop the loop: on the next "
      "point of the grid, once the loop has played to its end, or at once.")
};

struct after_overdub_selector
    : enum_combobox<"After overdub", AfterOverdub, after_overdub_default>
{
  using enum_combobox::operator=;
  struct range
  {
    std::string_view values[2] = {"Play", "New layer"};
    AfterOverdub init = after_overdub_default;
  };
  halp_meta(
      description,
      "What Rec / Play / Dub does while overdubbing or comping: go back to "
      "playing, or go on overdubbing in a new layer, as Comp next does.")
};

struct play_stop_button : halp::impulse_button<"Play / Stop">
{
  halp_meta(description, "Plays a stopped loop; stops it in any other mode.")
};

struct record_button : halp::impulse_button<"Record">
{
  halp_meta(
      description,
      "Records a new loop, in place of the one there is. While recording, ends "
      "the recording, which gives way to the post-action.")
};

struct overdub_button : halp::impulse_button<"Overdub">
{
  halp_meta(description, "Overdubs, from any mode. Does nothing while overdubbing.")
};

struct comp_next_button : halp::impulse_button<"Comp next">
{
  halp_meta(
      description,
      "Ends the layer being overdubbed and starts a new one, without stopping. "
      "From Play or Stop, starts comping; while recording, ends the recording "
      "and starts the first layer over it.")
};

struct play_button : halp::impulse_button<"Play">
{
  halp_meta(description, "Plays the loop, from any mode.")
};

struct stop_button : halp::impulse_button<"Stop">
{
  halp_meta(description, "Stops, from any mode.")
};

struct restart_button : halp::impulse_button<"Restart loop">
{
  halp_meta(
      description,
      "Goes back to the start of the loop, in the same mode: an overdub goes on "
      "from there in the same layer, a recording starts again.")
};

struct erase_last_button : halp::impulse_button<"Erase last">
{
  halp_meta(
      description,
      "Erases the newest layer. While overdubbing, that is the layer being "
      "overdubbed: as Undo, it is dropped and overdubbing goes on in a new one.")
};

struct mute_last_button : halp::impulse_button<"Mute last">
{
  halp_meta(description, "Mutes the newest layer, or unmutes it.")
};

struct previous_layer_button : halp::impulse_button<"Previous layer">
{
  halp_meta(
      description,
      "Selects the layer before the one Layer says, which Mute layer, Erase "
      "layer and Solo layer act on. The view outlines it.")
};

struct next_layer_button : halp::impulse_button<"Next layer">
{
  halp_meta(description, "Selects the layer after the one Layer says.")
};

struct solo_layer_button : halp::impulse_button<"Solo layer">
{
  halp_meta(
      description,
      "Mutes every layer but the one Layer says. Pressed again, the layers get "
      "back the mutes they had, unless a mute or an erase changed them since.")
};

struct reverse_button : halp::impulse_button<"Toggle reverse">
{
  halp_meta(description, "Turns Reverse on, or off.")
};

struct half_speed_button : halp::impulse_button<"Half speed">
{
  halp_meta(description, "Sets Speed to x1/2, or back to x1 when it is x1/2.")
};

struct double_speed_button : halp::impulse_button<"Double speed">
{
  halp_meta(description, "Sets Speed to x2, or back to x1 when it is x2.")
};

//! The mutes the layers had before a solo, to give them back.
struct solo_memory
{
  static constexpr int max_passes = 256;
  std::array<bool, max_passes> muted{};
  //! Passes when the solo was made, -1 when there is none to undo.
  int count{-1};
  int pass{-1};

  void forget() noexcept { count = -1; }

  //! The layers are muted as the solo left them.
  template <typename Engine>
  bool still_soloed(const Engine& e) const noexcept
  {
    if(count < 0 || e.looper_passes() < count)
      return false;
    for(int i = 0; i < count; i++)
      if(e.looper_muted(i) != (i != pass))
        return false;
    return true;
  }

  template <typename Engine>
  void toggle(Engine& e, int selected)
  {
    if(still_soloed(e))
    {
      for(int i = 0; i < count; i++)
        e.looper_set_muted(i, muted[i]);
      forget();
      return;
    }
    const int n = std::min(e.looper_passes(), max_passes);
    if(selected < 0 || selected >= n)
      return;
    for(int i = 0; i < n; i++)
      muted[i] = e.looper_muted(i);
    for(int i = 0; i < n; i++)
      e.looper_set_muted(i, i != selected);
    count = n;
    pass = selected;
  }
};

/**
 * The actions of the Switches tab that are not the transport's. What changes
 * what is heard waits for the grid point, the transport calling
 * looper_point(); selecting a layer is done at once.
 */
struct switches
{
  int erase_last{};
  bool mute_last{};
  bool solo{};
  bool reverse{};
  std::optional<float> speed;
  solo_memory soloed;

  void reset() noexcept
  {
    erase_last = 0;
    mute_last = false;
    solo = false;
    reverse = false;
    speed.reset();
  }

  //! Reads the actions pressed this tick. `empty`: there is no loop.
  template <typename Inputs, typename Engine>
  void read(Inputs& in, transport& t, Engine& e, bool empty)
  {
    const auto press = [&](TransportAction a) {
      t.press(a, empty, in.after_record.value, in.after_overdub.value);
    };
    if(in.rec_play_dub.value)
      press(TransportAction::RecPlayDub);
    if(in.play_stop.value)
      press(TransportAction::PlayStop);
    if(in.record.value)
      press(TransportAction::Record);
    if(in.overdub.value)
      press(TransportAction::Overdub);
    if(in.comp_next.value)
      press(TransportAction::CompNext);
    if(in.play.value)
      press(TransportAction::Play);
    if(in.stop.value)
      press(TransportAction::Stop);
    if(in.restart_loop.value)
      press(TransportAction::Restart);

    if(const int n = e.looper_passes();
       n > 0 && (in.next_layer.value || in.previous_layer.value))
    {
      const int step = (in.next_layer.value ? 1 : 0) - (in.previous_layer.value ? 1 : 0);
      show_control(in.layer, std::clamp(std::clamp(in.layer.value, 1, n) + step, 1, n));
    }

    bool asked = false;
    if(in.erase_last.value)
      erase_last++, asked = true;
    if(in.mute_last.value)
      mute_last = !mute_last, asked = true;
    if(in.solo_layer.value)
      solo = !solo, asked = true;
    if(in.reverse_toggle.value)
      reverse = !reverse, asked = true;
    // From the speed a change still waiting for the grid leaves.
    const auto toggle_speed = [&](float to) {
      const float from = speed ? *speed : in.speed.value;
      speed = from == to ? 1.f : to;
      asked = true;
    };
    if(in.half_speed.value)
      toggle_speed(0.5f);
    if(in.double_speed.value)
      toggle_speed(2.f);
    if(asked)
      t.request_point();
  }

  //! On the grid point.
  template <typename Inputs, typename Engine>
  void apply(Inputs& in, Engine& e)
  {
    for(; erase_last > 0; erase_last--)
      if(const int n = e.looper_passes(); n > 0)
        e.looper_erase(n - 1);
    if(std::exchange(mute_last, false))
      if(const int n = e.looper_passes(); n > 0)
        e.looper_mute(n - 1);
    if(std::exchange(solo, false))
      soloed.toggle(e, in.layer.value - 1);
    if(std::exchange(reverse, false))
      show_control(in.reverse, !in.reverse.value);
    if(speed)
      show_control(in.speed, *std::exchange(speed, std::nullopt));
  }
};
}
