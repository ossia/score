#pragma once
#include <Fx/Looper/Sync.hpp>
#include <Fx/Looper/Transport.hpp>

#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

/**
 * The actions of the loopers' Switches tab, made for footswitches, and the
 * settings that say what they and the transport do.
 *
 * Loop is the state: it shows what the looper does or is about to do,
 * whatever changed it. Play, Stop, Record and Overdub set it, as setting it
 * by hand does. The other actions say what no value can: "the next thing",
 * "a new layer", "from the start", "this layer". Everything that changes
 * what is heard waits for the next point of the grid, as a change of Loop
 * does; Undo, Redo and Clear act at once, and selecting a layer as well.
 *
 * The looper (the Engine below) provides, besides the transport's engine:
 *
 *   int looper_passes() const;                // passes heard
 *   bool looper_muted(int pass) const;
 *   void looper_set_muted(int pass, bool muted);
 *   void looper_mute(int pass);               // toggles
 *   void looper_erase(int pass);
 */
namespace Nodes::Looper
{
//! The control F of a layout, its choices on `Columns` buttons a row: Loop
//! keeps its modes on one line.
template <auto F, int Columns>
struct item_in_columns
{
  halp_meta(layout, halp::layouts::control)
  static constexpr int columns() noexcept { return Columns; }
  decltype(F) model = F;
};

//! What Rec / Play / Dub does while overdubbing until the setting is changed.
inline constexpr AfterOverdub after_overdub_default = AfterOverdub::Play;

//! The name Loop shows a mode by.
inline std::string_view mode_label(LoopMode m) noexcept
{
  static constexpr std::array<std::string_view, 4> names{
      "Play", "Record", "Overdub", "Stop"};
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

/**
 * What came in on Loop, and what it shows.
 *
 * A value that comes in on every tick, as an automation's or a cable's
 * does, asks for its mode once, when it changes: the looper changing Loop
 * by itself, at a recording's end or on a full buffer, does not make the
 * cable ask again and the mode flip back. A value that is what is asked for
 * already asks nothing, so that the looper's own change of Loop, coming back
 * from the UI, is not taken for a request, nor clears what is queued.
 */
struct loop_input
{
  //! A value came in this tick, whatever it was.
  bool arrived{};
  //! One came in on the tick before.
  bool streamed{};
  //! What the control held at the end of the last tick.
  LoopMode left{LoopMode::Stop};
  //! The last value that came in.
  LoopMode last_in{LoopMode::Stop};
  LoopMode shown{LoopMode::Stop};
};

template <typename Field>
void read_loop(Field& f, transport& t, loop_input& l, bool empty)
{
  const bool arrived = std::exchange(l.arrived, false) || f.value != l.left;
  if(arrived)
  {
    const LoopMode v = f.value;
    const bool repeated = l.streamed && v == l.last_in;
    if(!repeated && v != t.wanted)
      t.ask(v, empty);
    l.last_in = v;
  }
  l.streamed = arrived;
}

//! Loop shows what is asked for, once the tick has run.
template <typename Field>
void show_loop(Field& f, const transport& t, loop_input& l)
{
  if(t.wanted != l.shown)
  {
    l.shown = t.wanted;
    show_mode(f, l.shown);
  }
  l.left = f.value;
}

//! Every end of a recording: its bars, a full buffer, Rec / Play / Dub.
//! The Post-action of older scores, at the same place among the inputs.
struct after_record_selector
{
  static clang_buggy_consteval auto name() { return std::string_view{"After record"}; }
  static constexpr std::string_view former_name() noexcept { return "Post-action"; }
  enum widget
  {
    enumeration,
    list,
    combobox
  };
  struct range
  {
    std::string_view values[2] = {"Play", "Overdub"};
    AfterRecord init = AfterRecord::Play;
  };
  AfterRecord value = AfterRecord::Play;
  operator AfterRecord() const noexcept { return value; }
  auto& operator=(AfterRecord t) noexcept
  {
    value = t;
    return *this;
  }
  halp_meta(
      description,
      "What a recording gives way to when it ends: when its bars run out, "
      "when the buffer is full, or by Rec / Play / Dub. Play plays the loop; "
      "Overdub overdubs it from the very frame the recording ends on, so that "
      "what is held goes on without a gap. Setting Loop ends a recording into "
      "the mode it is set to.")
};

//! Where the loop is read from when the mode changes. The Restart of older
//! scores, at the same place among the inputs, its choices renamed.
struct start_from_selector
{
  static clang_buggy_consteval auto name() { return std::string_view{"Start from"}; }
  static constexpr std::string_view former_name() noexcept { return "Restart"; }
  static constexpr std::array<std::string_view, 3> former_values() noexcept
  {
    return {"Always", "Recording", "In phase"};
  }
  enum widget
  {
    enumeration,
    list,
    combobox
  };
  struct range
  {
    std::string_view values[3] = {"Loop start", "Where it was", "In phase"};
    StartFrom init = StartFrom::WhereItWas;
  };
  StartFrom value = StartFrom::WhereItWas;
  operator StartFrom() const noexcept { return value; }
  auto& operator=(StartFrom t) noexcept
  {
    value = t;
    return *this;
  }
  halp_meta(
      description,
      "Where the loop is read from when the mode changes. Loop start reads "
      "it from its beginning again; Where it was lets playing and overdubbing "
      "carry on from where the playhead is, so that Play, Overdub and Play "
      "again stay in time. In phase carries on as well, and a loop played "
      "again after a stop, or when the score starts or jumps, is where it "
      "would be had it kept playing with the timeline. A recording always "
      "starts at the loop's start.")
};

struct at_max_layers_selector
    : enum_combobox<"At max layers", AtMaxLayers, AtMaxLayers::MergeIntoNewest>
{
  using enum_combobox::operator=;
  struct range
  {
    std::string_view values[3]
        = {"Merge oldest", "Merge into newest", "Replace a muted layer"};
    AtMaxLayers init = AtMaxLayers::MergeIntoNewest;
  };
  halp_meta(
      description,
      "What a pass does when there are Max layers already. Merge oldest "
      "merges the two oldest layers into one, losing their undo. Merge into "
      "newest merges the pass into the newest layer once it ends: undo still "
      "takes it out, until the next pass starts. Replace a muted layer drops "
      "the oldest muted layer for the pass, else the newest layer.")
};

struct rec_play_dub_button : halp::impulse_button<"Rec / Play / Dub">
{
  halp_meta(
      description,
      "One button for the whole loop: records an empty loop, ends the recording "
      "into what After record says, overdubs a playing loop, and does what "
      "After overdub says while overdubbing. A stopped loop plays. Pressed "
      "again before the grid, each press is taken up on a grid point of its "
      "own.")
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
      "When a stop is taken up, from Stop, Play / Stop or Loop: on the next "
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
      "What Rec / Play / Dub does while overdubbing: go back to playing, or "
      "go on overdubbing in a new layer, as New layer does.")
};

struct play_stop_button : halp::impulse_button<"Play / Stop">
{
  halp_meta(description, "Plays a stopped loop; stops it in any other mode.")
};

struct record_button : halp::impulse_button<"Record">
{
  halp_meta(
      description,
      "Sets Loop to Record: records a new loop in place of the one there is. "
      "Retrigger starts a recording again.")
};

struct overdub_button : halp::impulse_button<"Overdub">
{
  halp_meta(
      description,
      "Sets Loop to Overdub. On an empty loop, records it, and the recording "
      "then gives way to overdubbing.")
};

struct play_button : halp::impulse_button<"Play">
{
  halp_meta(description, "Sets Loop to Play.")
};

struct stop_button : halp::impulse_button<"Stop">
{
  halp_meta(description, "Sets Loop to Stop.")
};

struct new_layer_button : halp::impulse_button<"New layer">
{
  halp_meta(
      description,
      "While overdubbing, ends the layer on the grid and goes on overdubbing "
      "in a new one, without the playhead moving. Does nothing otherwise.")
};

struct retrigger_button : halp::impulse_button<"Retrigger">
{
  halp_meta(
      description,
      "Goes back to the start of the loop on the grid, in the same mode: an "
      "overdub goes on from there in the same layer. While recording, starts "
      "the recording again, its bars and tempo counted from there.")
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

struct mute_layer_button : halp::impulse_button<"Mute layer">
{
  halp_meta(
      description,
      "Mutes the layer Layer says, or unmutes it, on the grid, as the m of "
      "its lane does. A muted layer is kept but not heard: it takes nothing "
      "from the layers under it, is not in the mix of a take, and still has "
      "its file in a take written as layers.")
};

struct erase_layer_button : halp::impulse_button<"Erase layer">
{
  halp_meta(
      description,
      "Removes the layer Layer says, on the grid, as the x of its lane does. "
      "The layer being overdubbed is dropped and the overdub goes on in a new "
      "one, as with Undo.")
};

struct solo_layer_button : halp::impulse_button<"Solo layer">
{
  halp_meta(
      description,
      "Mutes every layer but the one Layer says, on the grid. Pressed again "
      "on that layer, the layers get back the mutes they had, unless a mute "
      "or an erase changed them since; on another layer, solos that one.")
};

struct reverse_button : halp::impulse_button<"Toggle reverse">
{
  halp_meta(description, "Turns Reverse on, or off, on the grid.")
};

struct half_speed_button : halp::impulse_button<"Half speed">
{
  halp_meta(description, "Sets Speed to x1/2, or back to x1 when it is x1/2, on the grid.")
};

struct double_speed_button : halp::impulse_button<"Double speed">
{
  halp_meta(description, "Sets Speed to x2, or back to x1 when it is x2, on the grid.")
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
    if(count < 0 || e.looper_passes() != count)
      return false;
    for(int i = 0; i < count; i++)
      if(e.looper_muted(i) != (i != pass))
        return false;
    return true;
  }

  //! Solos `selected`, or unsolos it when it is the layer soloed.
  template <typename Engine>
  void toggle(Engine& e, int selected)
  {
    const bool soloed = still_soloed(e);
    if(soloed && selected == pass)
    {
      for(int i = 0; i < count; i++)
        e.looper_set_muted(i, muted[i]);
      forget();
      return;
    }
    const int n = std::min(e.looper_passes(), max_passes);
    if(selected < 0 || selected >= n)
      return;
    // From one solo to another, the mutes to give back are the ones from
    // before the first.
    if(!soloed)
      for(int i = 0; i < n; i++)
        muted[i] = e.looper_muted(i);
    for(int i = 0; i < n; i++)
      e.looper_set_muted(i, i != selected);
    count = n;
    pass = selected;
  }
};

//! A layer action waiting for the grid: on the layer Layer says then, or on
//! the pass a lane of the view showed, when the loop still has `layers`.
struct layer_action
{
  enum kind_t : uint8_t
  {
    mute,
    erase,
    solo
  } kind;
  int pass;
  int layers;
};

/**
 * The actions of the Switches tab, read each tick. What changes what is
 * heard waits for the grid point, the transport calling looper_point();
 * selecting a layer is done at once.
 */
struct switches
{
  static constexpr int max_actions = 16;
  std::array<layer_action, max_actions> actions{};
  int action_count{};
  bool reverse{};
  std::optional<float> speed;
  solo_memory soloed;
  //! Passes when Layer was last made to follow them.
  int seen_passes{};

  void reset() noexcept
  {
    action_count = 0;
    reverse = false;
    speed.reset();
  }

  void add(layer_action a, transport& t) noexcept
  {
    if(action_count < max_actions)
      actions[action_count++] = a;
    t.request_point();
  }

  //! Reads the actions pressed this tick. `empty`: there is no loop and none
  //! is being recorded.
  template <typename Inputs, typename Engine>
  void read(Inputs& in, transport& t, Engine& e, bool empty)
  {
    const auto press = [&](TransportAction a) {
      t.press(a, empty, in.after_record.value, in.after_overdub.value);
    };
    const auto set_loop = [&](LoopMode m) {
      if(t.wanted != m || t.queued())
        t.ask(m, empty);
    };
    if(in.rec_play_dub.value)
      press(TransportAction::RecPlayDub);
    if(in.play_stop.value)
      press(TransportAction::PlayStop);
    if(in.play.value)
      set_loop(LoopMode::Play);
    if(in.stop.value)
      set_loop(LoopMode::Stop);
    if(in.record.value)
      set_loop(LoopMode::Record);
    if(in.overdub.value)
      set_loop(LoopMode::Overdub);
    if(in.new_layer.value)
      press(TransportAction::NewLayer);
    if(in.retrigger.value)
      press(TransportAction::Retrigger);

    if(const int n = e.looper_passes();
       n > 0 && (in.next_layer.value || in.previous_layer.value))
    {
      const int step = (in.next_layer.value ? 1 : 0) - (in.previous_layer.value ? 1 : 0);
      show_control(in.layer, std::clamp(std::clamp(in.layer.value, 1, n) + step, 1, n));
    }

    if(in.mute_layer.value)
      add({layer_action::mute, -1, -1}, t);
    if(in.erase_layer.value)
      add({layer_action::erase, -1, -1}, t);
    if(in.solo_layer.value)
      add({layer_action::solo, -1, -1}, t);

    bool asked = false;
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
    for(int i = 0; i < action_count; i++)
    {
      const auto& a = actions[i];
      const int n = e.looper_passes();
      if(a.layers >= 0 && a.layers != n)
        continue;
      // A Layer past the layers there are names none.
      const int pass = a.pass >= 0 ? a.pass : in.layer.value - 1;
      switch(a.kind)
      {
        case layer_action::mute:
          e.looper_mute(pass);
          break;
        case layer_action::erase:
          e.looper_erase(pass);
          break;
        case layer_action::solo:
          soloed.toggle(e, pass);
          break;
      }
      follow(in, e.looper_passes());
    }
    action_count = 0;
    if(std::exchange(reverse, false))
      show_control(in.reverse, !in.reverse.value);
    if(speed)
      show_control(in.speed, *std::exchange(speed, std::nullopt));
  }

  //! Layer moves to each new layer as it is added, and stays among the
  //! layers there are when some go.
  template <typename Inputs>
  void follow(Inputs& in, int passes)
  {
    const int was = std::exchange(seen_passes, passes);
    if(passes > was)
      show_control(in.layer, passes);
    else if(passes > 0 && in.layer.value > passes)
      show_control(in.layer, passes);
  }

  //! Pass `pass` went from the layers: the one Layer says keeps its place.
  template <typename Inputs>
  void removed(Inputs& in, int pass)
  {
    seen_passes = std::max(0, seen_passes - 1);
    if(pass < in.layer.value - 1 && in.layer.value > 1)
      show_control(in.layer, in.layer.value - 1);
  }
};

//! The Switches tab of a looper whose inputs are `Ins`: the actions, by what
//! they act on, two to a row, so that the tab is no wider than the others.
template <typename Ins>
struct switches_tab
{
  halp_meta(name, "Switches")
  halp_meta(layout, halp::layouts::hbox)
  halp_meta(alignment, halp::alignments::left)
  halp_meta(spacing, 6)
  struct
  {
    halp_meta(layout, halp::layouts::vbox)
    struct
    {
      halp_meta(name, "One button")
      halp_meta(layout, halp::layouts::section)
      halp_meta(alignment, halp::alignments::left)
      halp_meta(spacing, 6)
      struct
      {
        halp_meta(layout, halp::layouts::grid)
        halp_meta(alignment, halp::alignments::left)
        halp_meta(spacing, 6)
        halp_meta(columns, 2)
        halp::item<&Ins::rec_play_dub> rec_play_dub;
        halp::item<&Ins::play_stop> play_stop;
      } buttons;
      struct
      {
        halp_meta(layout, halp::layouts::grid)
        halp_meta(alignment, halp::alignments::left)
        halp_meta(spacing, 6)
        halp_meta(columns, 2)
        halp::item<&Ins::after_record> after_record;
        halp::item<&Ins::after_overdub> after_overdub;
        halp::item<&Ins::stop_at> stop_at;
      } settings;
    } one_button;
    struct
    {
      halp_meta(name, "Transport")
      halp_meta(layout, halp::layouts::section)
      halp_meta(alignment, halp::alignments::left)
      halp_meta(spacing, 6)
      struct
      {
        halp_meta(layout, halp::layouts::grid)
        halp_meta(alignment, halp::alignments::left)
        halp_meta(spacing, 6)
        halp_meta(columns, 2)
        halp::item<&Ins::play> play;
        halp::item<&Ins::stop> stop;
        halp::item<&Ins::record> record;
        halp::item<&Ins::overdub> overdub;
        halp::item<&Ins::new_layer> new_layer;
        halp::item<&Ins::retrigger> retrigger;
      } buttons;
    } transport;
  } left;
  struct
  {
    halp_meta(layout, halp::layouts::vbox)
    struct
    {
      halp_meta(name, "Layers")
      halp_meta(layout, halp::layouts::section)
      halp_meta(alignment, halp::alignments::left)
      halp_meta(spacing, 6)
      struct
      {
        halp_meta(layout, halp::layouts::grid)
        halp_meta(alignment, halp::alignments::left)
        halp_meta(spacing, 6)
        halp_meta(columns, 2)
        halp::item<&Ins::undo> undo;
        halp::item<&Ins::redo> redo;
        halp::item<&Ins::clear> clear;
        halp::item<&Ins::layer> layer;
        halp::item<&Ins::previous_layer> previous_layer;
        halp::item<&Ins::next_layer> next_layer;
        halp::item<&Ins::mute_layer> mute_layer;
        halp::item<&Ins::erase_layer> erase_layer;
        halp::item<&Ins::solo_layer> solo_layer;
      } buttons;
    } layers;
    struct
    {
      halp_meta(name, "Play")
      halp_meta(layout, halp::layouts::section)
      halp_meta(alignment, halp::alignments::left)
      halp_meta(spacing, 6)
      struct
      {
        halp_meta(layout, halp::layouts::grid)
        halp_meta(alignment, halp::alignments::left)
        halp_meta(spacing, 6)
        halp_meta(columns, 2)
        halp::item<&Ins::reverse_toggle> reverse;
        halp::item<&Ins::half_speed> half_speed;
        halp::item<&Ins::double_speed> double_speed;
        halp::item<&Ins::capture> capture;
      } buttons;
    } play;
  } right;
};
}
