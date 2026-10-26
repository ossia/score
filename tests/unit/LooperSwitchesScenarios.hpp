#pragma once
// The control model of the loopers, the same for the audio and the MIDI
// looper: Loop is the state, the actions say what no value can, and every
// setting applies to every way of reaching what it decides. Each test file
// drives its looper through a harness H:
//
//   H::beat                      frames of a quarter note, the grid
//   H::frames                    frames of a tick; a beat is whole ticks
//   h.node                       the looper
//   h.tick_once()                runs one tick, the input playing all along
//   h.passes(), h.muted(k), h.length(), h.position()
//   h.signature(k)               what layer k holds, additive: the audio
//                                summed, or the MIDI events counted
//   h.set_max_layers(n)          Max layers, applied as a start would
//   h.enable_capture()           input kept for Capture
//   h.shown                      the last mode Loop was shown with
//   h.shown_count                how many times it was
//   h.stop_score()               stops the looper as the score stopping
//                                does; the next tick starts the score again
//   h.tempo                      the tempo of the ticks to come
//
// The loopers whose overdub passes are layers only from their first event
// on, the MIDI and the value looper, also have:
//
//   h.playing                    whether the input plays
//   h.enable_takes(), h.takes()  takes written, and how many there are
//
// The grid is a quarter note: a mode asked for between two beats is taken up
// on the tick that starts on the next one.

#include <Fx/Looper/Switches.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <string_view>

namespace looper_switches
{
using Nodes::Looper::AfterOverdub;
using Nodes::Looper::AfterRecord;
using Nodes::Looper::AtMaxLayers;
using Nodes::Looper::LoopMode;
using Nodes::Looper::Passes;
using Nodes::Looper::PastEnd;
using Nodes::Looper::StartFrom;
using Nodes::Looper::StopAt;

//! Two signatures of the same audio or events, whatever the order it was
//! summed in.
inline bool same(double a, double b)
{
  return std::abs(a - b) <= 1e-6 * std::max(1., std::abs(b));
}

template <typename H>
void press(H& h, auto& button)
{
  button.value.emplace();
  h.tick_once();
  button.value.reset();
}

template <typename H>
LoopMode mode(const H& h)
{
  return h.node.transport.actualMode;
}

//! Runs up to the next beat, and the tick that starts on it.
template <typename H>
void next_beat(H& h)
{
  while(h.now % H::beat != 0)
    h.tick_once();
  h.tick_once();
}

template <typename H>
void beats(H& h, int n)
{
  for(int i = 0; i < n * int(H::beat / H::frames); i++)
    h.tick_once();
}

//! Presses between two beats: what it asks for waits for the next one.
template <typename H>
void press_off_beat(H& h, auto& button)
{
  if(h.now % H::beat == 0)
    h.tick_once();
  press(h, button);
}

template <typename H>
void at_beat(H& h, auto& button)
{
  press_off_beat(h, button);
  next_beat(h);
}

//! Records a loop of four beats with Rec / Play / Dub, and plays it.
template <typename H>
void record_loop(H& h)
{
  auto& in = h.node.inputs;
  press_off_beat(h, in.rec_play_dub);
  next_beat(h);
  REQUIRE(mode(h) == LoopMode::Record);
  beats(h, 3);
  press(h, in.rec_play_dub);
  next_beat(h);
  REQUIRE(mode(h) == LoopMode::Play);
  REQUIRE(h.length() == 4 * H::beat);
}

//! Four layers: the recording, and three overdubbed over it with New layer,
//! then plays.
template <typename H>
void four_layers(H& h)
{
  auto& in = h.node.inputs;
  record_loop(h);
  at_beat(h, in.overdub);
  for(int i = 0; i < 2; i++)
    at_beat(h, in.new_layer);
  at_beat(h, in.play);
  REQUIRE(h.passes() == 4);
  REQUIRE(mode(h) == LoopMode::Play);
}

//! Recording a loop, with Rec / Play / Dub, with Record, and with Loop: Loop
//! shows the recording as soon as it is asked for, and the mode it gives way
//! to.
template <typename H>
void record_a_loop(H& h)
{
  auto& in = h.node.inputs;
  SECTION("Rec / Play / Dub")
  {
    press_off_beat(h, in.rec_play_dub);
    CHECK(mode(h) == LoopMode::Stop);
    CHECK(in.mode.value == LoopMode::Record);
    CHECK(h.shown == "Record");
    next_beat(h);
    CHECK(mode(h) == LoopMode::Record);
    beats(h, 3);
    press(h, in.rec_play_dub);
    CHECK(mode(h) == LoopMode::Record);
    CHECK(h.shown == "Play");
    next_beat(h);
    CHECK(mode(h) == LoopMode::Play);
  }
  SECTION("Record, then Play")
  {
    at_beat(h, in.record);
    CHECK(h.shown == "Record");
    beats(h, 3);
    // Record while recording sets Loop to what it is: nothing.
    const int shown = h.shown_count;
    press_off_beat(h, in.record);
    CHECK(h.shown_count == shown);
    next_beat(h);
    CHECK(mode(h) == LoopMode::Record);
    press_off_beat(h, in.play);
    next_beat(h);
    CHECK(mode(h) == LoopMode::Play);
    CHECK(h.length() == 5 * H::beat);
    return;
  }
  SECTION("Loop")
  {
    in.mode = LoopMode::Record;
    next_beat(h);
    CHECK(mode(h) == LoopMode::Record);
    beats(h, 3);
    h.tick_once();
    in.mode = LoopMode::Play;
    next_beat(h);
    CHECK(mode(h) == LoopMode::Play);
    CHECK(h.shown == "Play");
  }
  CHECK(h.length() == 4 * H::beat);
  CHECK(h.passes() == 1);
}

//! After record = Overdub, and Overdub on an empty loop, are the same
//! recording: it ends into overdubbing by Rec / Play / Dub, by its bars, and
//! by Loop.
template <typename H>
void record_then_overdub(H& h)
{
  auto& in = h.node.inputs;
  enum how_t
  {
    after_record,
    overdub_on_empty
  };
  auto start = [&](how_t how) {
    if(how == after_record)
    {
      in.after_record = AfterRecord::Overdub;
      press_off_beat(h, in.rec_play_dub);
    }
    else
    {
      press_off_beat(h, in.overdub);
    }
    // Overdub on an empty loop records it: Loop says so.
    CHECK(in.mode.value == LoopMode::Record);
    CHECK(h.shown == "Record");
    next_beat(h);
    REQUIRE(mode(h) == LoopMode::Record);
  };
  for(const how_t how : {after_record, overdub_on_empty})
  {
    CAPTURE(int(how));
    SECTION("ended by Rec / Play / Dub")
    {
      start(how);
      beats(h, 3);
      press(h, in.rec_play_dub);
      CHECK(h.shown == "Overdub");
      next_beat(h);
      CHECK(mode(h) == LoopMode::Overdub);
      CHECK(h.length() == 4 * H::beat);
      CHECK(h.passes() == 2);
      CHECK(h.node.transport.is_taking());
    }
    SECTION("ended by its bars")
    {
      in.postaction_bars.value = 1;
      start(how);
      for(int i = 0; i < 100 && mode(h) == LoopMode::Record; i++)
        h.tick_once();
      CHECK(mode(h) == LoopMode::Overdub);
      CHECK(h.shown == "Overdub");
      CHECK(h.length() == 4 * H::beat);
      CHECK(h.passes() == 2);
    }
    SECTION("Loop set to Record during it changes nothing")
    {
      start(how);
      beats(h, 1);
      in.mode = LoopMode::Record;
      beats(h, 2);
      CHECK(mode(h) == LoopMode::Record);
      press(h, in.rec_play_dub);
      next_beat(h);
      CHECK(mode(h) == LoopMode::Overdub);
      CHECK(h.length() == 4 * H::beat);
    }
    SECTION("Loop set to Play ends it into Play")
    {
      start(how);
      beats(h, 3);
      h.tick_once();
      in.mode = LoopMode::Play;
      next_beat(h);
      CHECK(mode(h) == LoopMode::Play);
      CHECK(h.passes() == 1);
    }
  }
}

//! Play and stop through Loop, Play and Stop, Play / Stop.
template <typename H>
void play_and_stop(H& h)
{
  auto& in = h.node.inputs;
  // Nothing to play: Play / Stop does nothing.
  press_off_beat(h, in.play_stop);
  CHECK(in.mode.value == LoopMode::Stop);
  CHECK(h.shown_count == 0);

  record_loop(h);
  in.mode = LoopMode::Stop;
  next_beat(h);
  CHECK(mode(h) == LoopMode::Stop);
  in.mode = LoopMode::Play;
  next_beat(h);
  CHECK(mode(h) == LoopMode::Play);

  at_beat(h, in.stop);
  CHECK(mode(h) == LoopMode::Stop);
  CHECK(h.shown == "Stop");
  at_beat(h, in.play);
  CHECK(mode(h) == LoopMode::Play);
  CHECK(h.shown == "Play");

  at_beat(h, in.play_stop);
  CHECK(mode(h) == LoopMode::Stop);
  at_beat(h, in.play_stop);
  CHECK(mode(h) == LoopMode::Play);
  at_beat(h, in.overdub);
  at_beat(h, in.play_stop);
  CHECK(mode(h) == LoopMode::Stop);
  CHECK(h.passes() == 2);
}

//! Overdub from Play, Overdub again does nothing, Play ends it.
template <typename H>
void overdub(H& h)
{
  auto& in = h.node.inputs;
  record_loop(h);
  at_beat(h, in.overdub);
  CHECK(mode(h) == LoopMode::Overdub);
  CHECK(h.passes() == 2);
  beats(h, 1);
  const int shown = h.shown_count;
  press_off_beat(h, in.overdub);
  CHECK(h.shown_count == shown);
  next_beat(h);
  beats(h, 2);
  CHECK(mode(h) == LoopMode::Overdub);
  CHECK(h.passes() == 2);
  CHECK(!h.node.transport.new_pass_pending());
  at_beat(h, in.play);
  CHECK(mode(h) == LoopMode::Play);
  CHECK(h.passes() == 2);
  CHECK(h.signature(1) > 0.);
}

//! A new layer while overdubbing, by New layer, by Per cycle and by
//! Rec / Play / Dub with After overdub on New layer: the playhead goes on.
template <typename H>
void new_layer(H& h)
{
  auto& in = h.node.inputs;
  record_loop(h);
  // Elsewhere than overdubbing, New layer does nothing.
  at_beat(h, in.new_layer);
  CHECK(h.passes() == 1);
  CHECK(mode(h) == LoopMode::Play);

  at_beat(h, in.overdub);
  REQUIRE(h.passes() == 2);
  beats(h, 1);
  const auto expect_no_jump = [&](auto&& act) {
    const int64_t before = h.position();
    act();
    const int64_t len = h.length();
    CHECK(h.position() == (before + H::frames) % len);
  };

  SECTION("New layer")
  {
    for(int i = 0; i < 3; i++)
    {
      press_off_beat(h, in.new_layer);
      CHECK(h.node.transport.new_pass_pending());
      while((h.now + H::frames) % H::beat != H::frames)
        h.tick_once();
      expect_no_jump([&] { h.tick_once(); });
      CHECK(h.passes() == 3 + i);
      CHECK(mode(h) == LoopMode::Overdub);
      CHECK(h.node.transport.is_taking());
    }
  }
  SECTION("Rec / Play / Dub with After overdub on New layer")
  {
    in.after_overdub.value = AfterOverdub::NewLayer;
    press_off_beat(h, in.rec_play_dub);
    while((h.now + H::frames) % H::beat != H::frames)
      h.tick_once();
    expect_no_jump([&] { h.tick_once(); });
    CHECK(h.passes() == 3);
    CHECK(mode(h) == LoopMode::Overdub);
    CHECK(h.shown == "Overdub");
  }
  SECTION("Per cycle")
  {
    in.passes.value = Passes::PerCycle;
    // Up to the loop's end, then over it.
    while(h.position() + H::frames < h.length())
      h.tick_once();
    const int before = h.passes();
    expect_no_jump([&] { h.tick_once(); });
    h.tick_once();
    CHECK(h.passes() == before + 1);
    beats(h, 4);
    CHECK(h.passes() == before + 2);
  }
  // Each layer is closed as any: playing, nothing is taken in, and each one
  // kept what it took.
  at_beat(h, in.play);
  CHECK(!h.node.transport.is_taking());
  for(int k = 1; k < h.passes(); k++)
    CHECK(h.signature(k) > 0.);
}

//! Retrigger: back to the loop's start on the grid, in the same mode; a
//! recording starts again.
template <typename H>
void retrigger(H& h)
{
  auto& in = h.node.inputs;
  // Nothing to go back to.
  press_off_beat(h, in.retrigger);
  CHECK(!h.node.transport.restart_pending());

  record_loop(h);
  beats(h, 1);
  press_off_beat(h, in.retrigger);
  CHECK(h.node.transport.restart_pending());
  next_beat(h);
  // On the beat, back to the start: a tick into the loop since.
  CHECK(h.position() == H::frames);
  CHECK(mode(h) == LoopMode::Play);

  SECTION("an overdub goes on from the start, in the same layer")
  {
    at_beat(h, in.overdub);
    beats(h, 1);
    REQUIRE(h.passes() == 2);
    at_beat(h, in.retrigger);
    CHECK(h.position() == H::frames);
    CHECK(mode(h) == LoopMode::Overdub);
    CHECK(h.passes() == 2);
    CHECK(h.node.transport.is_taking());
  }
  SECTION("a recording starts again")
  {
    press(h, in.clear);
    at_beat(h, in.record);
    beats(h, 2);
    at_beat(h, in.retrigger);
    CHECK(mode(h) == LoopMode::Record);
    CHECK(h.length() == H::frames);
    beats(h, 1);
    at_beat(h, in.play);
    CHECK(h.length() == 2 * H::beat);
  }
}

//! Start from: Where it was by default, Play, Overdub and Play again stay in
//! time; Loop start reads the loop from its start on each change.
template <typename H>
void start_from(H& h)
{
  auto& in = h.node.inputs;
  CHECK(in.start_from.value == StartFrom::WhereItWas);
  record_loop(h);
  beats(h, 1);
  const auto in_time = [&](auto& button) {
    press_off_beat(h, button);
    while((h.now + H::frames) % H::beat != H::frames)
      h.tick_once();
    const int64_t before = h.position();
    h.tick_once();
    return h.position() == (before + H::frames) % h.length();
  };
  SECTION("where it was")
  {
    CHECK(in_time(in.overdub));
    CHECK(mode(h) == LoopMode::Overdub);
    beats(h, 1);
    CHECK(in_time(in.play));
    CHECK(mode(h) == LoopMode::Play);
    // Stopped, then played again: from where it stopped.
    at_beat(h, in.stop);
    const int64_t stopped = h.position();
    at_beat(h, in.play);
    CHECK(h.position() == (stopped + H::frames) % h.length());
  }
  SECTION("loop start")
  {
    in.start_from = StartFrom::LoopStart;
    at_beat(h, in.overdub);
    CHECK(h.position() == H::frames);
    beats(h, 1);
    at_beat(h, in.play);
    CHECK(h.position() == H::frames);
  }
}

//! Undo takes out the last layer, the one being overdubbed included; Redo
//! brings it back.
template <typename H>
void undo_redo(H& h)
{
  auto& in = h.node.inputs;
  four_layers(h);
  press(h, in.undo);
  CHECK(h.passes() == 3);
  press(h, in.undo);
  CHECK(h.passes() == 2);
  press(h, in.redo);
  CHECK(h.passes() == 3);

  at_beat(h, in.overdub);
  REQUIRE(h.passes() == 4);
  beats(h, 1);
  const double dropped = h.signature(3);
  press(h, in.undo);
  // Dropped, and overdubbing goes on at once in a new one.
  CHECK(h.passes() == 4);
  CHECK(h.signature(3) < dropped);
  CHECK(mode(h) == LoopMode::Overdub);
  at_beat(h, in.play);
  // Redo has nothing to bring back over a new layer.
  press(h, in.redo);
  CHECK(h.passes() == 4);
  press(h, in.clear);
  CHECK(h.passes() == 0);
}

//! Mute layer and Erase layer act on the grid, on the newest layer unless
//! one was stepped back to.
template <typename H>
void mute_and_erase(H& h)
{
  auto& in = h.node.inputs;
  four_layers(h);
  // Layer moved to each new layer.
  CHECK(in.layer.value == 4);

  press_off_beat(h, in.mute_layer);
  CHECK(!h.muted(3));
  next_beat(h);
  CHECK(h.muted(3));
  at_beat(h, in.mute_layer);
  CHECK(!h.muted(3));

  press_off_beat(h, in.erase_layer);
  CHECK(h.passes() == 4);
  next_beat(h);
  CHECK(h.passes() == 3);
  CHECK(in.layer.value == 3);

  // Stepped back to the first overdub.
  press(h, in.previous_layer);
  CHECK(in.layer.value == 2);
  at_beat(h, in.mute_layer);
  CHECK(h.muted(1));
  CHECK(!h.muted(2));
  at_beat(h, in.erase_layer);
  CHECK(h.passes() == 2);
  CHECK(!h.muted(1));

  // A new layer: Layer goes to it.
  at_beat(h, in.overdub);
  CHECK(in.layer.value == 3);

  SECTION("the layer being overdubbed is dropped, and the overdub goes on")
  {
    beats(h, 1);
    at_beat(h, in.erase_layer);
    CHECK(h.passes() == 3);
    CHECK(mode(h) == LoopMode::Overdub);
    CHECK(h.node.transport.is_taking());
  }
}

//! Solo layer mutes every layer but the selected one; again on that layer,
//! the mutes from before come back; on another, that one is soloed.
template <typename H>
void solo(H& h)
{
  auto& in = h.node.inputs;
  four_layers(h);
  int layer_shown = 0;
  in.layer.update_controller = [&](int v) { layer_shown = v; };
  in.layer.value = 1;

  // Selecting is done at once, and clamped to the layers there are.
  press(h, in.next_layer);
  CHECK(in.layer.value == 2);
  CHECK(layer_shown == 2);
  for(int i = 0; i < 5; i++)
    press(h, in.next_layer);
  CHECK(in.layer.value == 4);
  for(int i = 0; i < 5; i++)
    press(h, in.previous_layer);
  CHECK(in.layer.value == 1);

  // The first layer muted, then the third soloed.
  at_beat(h, in.mute_layer);
  REQUIRE(h.muted(0));
  press(h, in.next_layer);
  press(h, in.next_layer);
  REQUIRE(in.layer.value == 3);
  press_off_beat(h, in.solo_layer);
  CHECK(!h.muted(1));
  next_beat(h);
  CHECK(h.muted(0));
  CHECK(h.muted(1));
  CHECK(!h.muted(2));
  CHECK(h.muted(3));

  SECTION("soloed again, the mutes from before come back")
  {
    at_beat(h, in.solo_layer);
    CHECK(h.muted(0));
    CHECK(!h.muted(1));
    CHECK(!h.muted(2));
    CHECK(!h.muted(3));
  }
  SECTION("on another layer, that one is soloed, and unsoloing it gives the mutes "
          "from before the first solo")
  {
    press(h, in.next_layer);
    at_beat(h, in.solo_layer);
    CHECK(h.muted(0));
    CHECK(h.muted(1));
    CHECK(h.muted(2));
    CHECK(!h.muted(3));
    at_beat(h, in.solo_layer);
    CHECK(h.muted(0));
    CHECK(!h.muted(1));
    CHECK(!h.muted(2));
    CHECK(!h.muted(3));
  }
  SECTION("a mute in between drops the mutes from before")
  {
    press(h, in.next_layer);
    at_beat(h, in.mute_layer);
    REQUIRE(!h.muted(3));
    press(h, in.previous_layer);
    at_beat(h, in.solo_layer);
    // A solo of its own.
    CHECK(h.muted(0));
    CHECK(h.muted(1));
    CHECK(!h.muted(2));
    CHECK(h.muted(3));
  }
}

template <typename H>
void reverse_and_speed(H& h)
{
  auto& in = h.node.inputs;
  record_loop(h);
  bool reverse_shown = false;
  float speed_shown = 0.f;
  in.reverse.update_controller = [&](bool v) { reverse_shown = v; };
  in.speed.update_controller = [&](float v) { speed_shown = v; };

  press_off_beat(h, in.reverse_toggle);
  CHECK(!in.reverse.value);
  next_beat(h);
  CHECK(in.reverse.value);
  CHECK(reverse_shown);
  at_beat(h, in.reverse_toggle);
  CHECK(!in.reverse.value);
  CHECK(!reverse_shown);

  press_off_beat(h, in.half_speed);
  CHECK(in.speed.value == 1.f);
  next_beat(h);
  CHECK(in.speed.value == 0.5f);
  CHECK(speed_shown == 0.5f);
  at_beat(h, in.half_speed);
  CHECK(in.speed.value == 1.f);
  at_beat(h, in.double_speed);
  CHECK(in.speed.value == 2.f);
  CHECK(speed_shown == 2.f);
  at_beat(h, in.half_speed);
  CHECK(in.speed.value == 0.5f);
  at_beat(h, in.double_speed);
  CHECK(in.speed.value == 2.f);
  at_beat(h, in.double_speed);
  CHECK(in.speed.value == 1.f);
  // The loop played on all along.
  CHECK(mode(h) == LoopMode::Play);
}

//! Three layers at most: a fourth pass is merged into the newest, merges the
//! two oldest, or replaces a muted layer, else the newest.
template <typename H>
void at_max_layers(H& h)
{
  auto& in = h.node.inputs;
  CHECK(in.at_max_layers.value == AtMaxLayers::MergeIntoNewest);
  h.set_max_layers(3);
  record_loop(h);
  at_beat(h, in.overdub);
  at_beat(h, in.new_layer);
  at_beat(h, in.play);
  REQUIRE(h.passes() == 3);
  const double s0 = h.signature(0), s1 = h.signature(1), s2 = h.signature(2);
  REQUIRE(s2 > 0.);

  const auto fourth_pass = [&] {
    at_beat(h, in.overdub);
    beats(h, 1);
    at_beat(h, in.play);
    beats(h, 1);
  };

  SECTION("merge into newest, and its undo")
  {
    fourth_pass();
    CHECK(h.passes() == 3);
    CHECK(same(h.signature(0), s0));
    CHECK(same(h.signature(1), s1));
    const double merged = h.signature(2);
    CHECK(merged > s2);

    // Undo takes the pass out of the newest layer.
    press(h, in.undo);
    CHECK(h.passes() == 3);
    CHECK(same(h.signature(2), s2));
    // Redo puts it back, and merges it again.
    press(h, in.redo);
    beats(h, 1);
    CHECK(h.passes() == 3);
    CHECK(same(h.signature(2), merged));
    // Undo takes it out again, then the newest layer as it was.
    press(h, in.undo);
    CHECK(same(h.signature(2), s2));
    press(h, in.undo);
    CHECK(h.passes() == 2);
  }
  SECTION("merge oldest")
  {
    in.at_max_layers = AtMaxLayers::MergeOldest;
    fourth_pass();
    CHECK(h.passes() == 3);
    CHECK(same(h.signature(0), s0 + s1));
    CHECK(same(h.signature(1), s2));
  }
  SECTION("replace a muted layer")
  {
    in.at_max_layers = AtMaxLayers::ReplaceMuted;
    in.layer.value = 2;
    at_beat(h, in.mute_layer);
    REQUIRE(h.muted(1));
    fourth_pass();
    CHECK(h.passes() == 3);
    CHECK(!h.muted(0));
    CHECK(!h.muted(1));
    CHECK(same(h.signature(1), s2));
  }
  SECTION("replace the newest when none is muted")
  {
    in.at_max_layers = AtMaxLayers::ReplaceMuted;
    fourth_pass();
    CHECK(h.passes() == 3);
    CHECK(same(h.signature(0), s0));
    CHECK(same(h.signature(1), s1));
  }
}

//! Three layers of three lengths, Max layers being three: the recording, a
//! pass of one beat and a pass of two, then plays.
template <typename H>
void three_distinct_layers(H& h)
{
  auto& in = h.node.inputs;
  h.set_max_layers(3);
  record_loop(h);
  at_beat(h, in.overdub);
  at_beat(h, in.new_layer);
  beats(h, 1);
  at_beat(h, in.play);
  REQUIRE(h.passes() == 3);
  REQUIRE(h.signature(2) > h.signature(1));
  REQUIRE(h.signature(1) > 0.);
}

//! Merge oldest: Layer goes to the pass that made the two oldest layers
//! merge, and a layer selected while they merge stays the one selected.
template <typename H>
void layer_follows_merges(H& h)
{
  auto& in = h.node.inputs;
  in.at_max_layers = AtMaxLayers::MergeOldest;
  h.slow_merges();
  three_distinct_layers(h);
  const double s2 = h.signature(2);
  in.layer.value = 2;
  at_beat(h, in.overdub);
  CHECK(in.layer.value == h.passes());
  // The layer under the new one.
  press(h, in.previous_layer);
  REQUIRE(same(h.signature(in.layer.value - 1), s2));
  beats(h, 2);
  REQUIRE(h.passes() == 3);
  CHECK(in.layer.value == 2);
  CHECK(same(h.signature(in.layer.value - 1), s2));
  at_beat(h, in.play);
  CHECK(in.layer.value == 2);
}

//! A pass per cycle with Extend, stopped just after the loop grew: the stop
//! is late, the loop keeps its length, and the layer the cut on the old end
//! started goes with what it took in. Undo and redo see the layers there are.
template <typename H>
void late_stop_after_cycle_cut(H& h)
{
  auto& in = h.node.inputs;
  record_loop(h);
  in.passes.value = Passes::PerCycle;
  in.past_end.value = PastEnd::Extend;
  // The looper's own default: more than a tick.
  in.late.value = 120;
  while(h.position() != 0)
    h.tick_once();
  in.mode = LoopMode::Overdub;
  h.tick_once();
  REQUIRE(mode(h) == LoopMode::Overdub);
  // A tick past the loop's end: the loop grew, and a layer was cut there.
  beats(h, 4);
  REQUIRE(h.length() == 8 * H::beat);
  REQUIRE(h.passes() == 3);
  in.mode = LoopMode::Play;
  h.tick_once();
  REQUIRE(mode(h) == LoopMode::Play);
  CHECK(h.length() == 4 * H::beat);
  CHECK(h.passes() == 2);
  for(int k = 0; k < h.passes(); k++)
    CHECK(h.signature(k) > 0.);
  press(h, in.undo);
  CHECK(h.passes() == 1);
  CHECK(h.length() == 4 * H::beat);
  press(h, in.redo);
  CHECK(h.passes() == 2);
  CHECK(h.length() == 4 * H::beat);
  press(h, in.redo);
  CHECK(h.passes() == 2);
}

//! Replace a muted layer: undoing the pass that replaced a layer puts that
//! layer back, and redo replaces it again; undone while it is taken in, the
//! pass goes and the layer comes back.
template <typename H>
void replace_muted_undo(H& h)
{
  auto& in = h.node.inputs;
  in.at_max_layers = AtMaxLayers::ReplaceMuted;
  three_distinct_layers(h);
  const double s0 = h.signature(0), s1 = h.signature(1), s2 = h.signature(2);
  in.layer.value = 2;
  at_beat(h, in.mute_layer);
  REQUIRE(h.muted(1));

  SECTION("undo and redo of the pass")
  {
    at_beat(h, in.overdub);
    beats(h, 2);
    at_beat(h, in.play);
    REQUIRE(h.passes() == 3);
    CHECK(same(h.signature(1), s2));
    const double s3 = h.signature(2);
    CHECK(s3 > s2);

    press(h, in.undo);
    CHECK(h.passes() == 3);
    CHECK(same(h.signature(0), s0));
    CHECK(same(h.signature(1), s1));
    CHECK(same(h.signature(2), s2));
    CHECK(h.muted(1));
    CHECK(in.layer.value == 3);

    press(h, in.redo);
    CHECK(h.passes() == 3);
    CHECK(same(h.signature(1), s2));
    CHECK(same(h.signature(2), s3));
    CHECK(!h.muted(0));
    CHECK(!h.muted(1));

    // And again, the loop playing on in between.
    beats(h, 1);
    press(h, in.undo);
    CHECK(same(h.signature(1), s1));
    CHECK(h.muted(1));
    press(h, in.undo);
    CHECK(h.passes() == 2);
    press(h, in.redo);
    press(h, in.redo);
    CHECK(h.passes() == 3);
    CHECK(same(h.signature(2), s3));
  }
  SECTION("undo while the pass is taken in")
  {
    at_beat(h, in.overdub);
    beats(h, 1);
    REQUIRE(h.node.transport.is_taking());
    // The pass goes, the muted layer comes back, and the overdub goes on in
    // a new pass, which replaces it again.
    press(h, in.undo);
    CHECK(h.node.transport.is_taking());
    CHECK(h.passes() == 3);
    CHECK(same(h.signature(1), s2));
    at_beat(h, in.play);
    press(h, in.undo);
    CHECK(h.passes() == 3);
    CHECK(same(h.signature(1), s1));
    CHECK(same(h.signature(2), s2));
    CHECK(h.muted(1));
  }
  SECTION("the next pass makes the replacement final")
  {
    // The second pass replaces the first, none being muted then.
    at_beat(h, in.overdub);
    beats(h, 1);
    at_beat(h, in.new_layer);
    beats(h, 1);
    at_beat(h, in.play);
    REQUIRE(h.passes() == 3);
    press(h, in.undo);
    CHECK(h.passes() == 3);
    CHECK(same(h.signature(1), s2));
    // The muted layer the first pass replaced stays gone.
    press(h, in.undo);
    CHECK(h.passes() == 2);
    CHECK(same(h.signature(0), s0));
    CHECK(same(h.signature(1), s2));
  }
}

//! Capture plays the loop it makes, and Loop says so: Stop stops it.
template <typename H>
void capture_shows_play(H& h)
{
  auto& in = h.node.inputs;
  h.enable_capture();
  in.capture_bars.value = 1;
  beats(h, 6);
  press(h, in.capture);
  CHECK(mode(h) == LoopMode::Play);
  CHECK(in.mode.value == LoopMode::Play);
  CHECK(h.shown == "Play");
  at_beat(h, in.stop);
  CHECK(mode(h) == LoopMode::Stop);
}

//! Undo or Clear while recording starts the recording again, its bars counted
//! from there.
template <typename H>
void undo_while_recording(H& h, bool clear)
{
  auto& in = h.node.inputs;
  in.postaction_bars.value = 1;
  // From a bar line.
  while(h.now % (4 * H::beat) != 4 * H::beat - H::frames)
    h.tick_once();
  in.mode = LoopMode::Record;
  next_beat(h);
  REQUIRE(mode(h) == LoopMode::Record);
  beats(h, 2);
  if(clear)
    press(h, in.clear);
  else
    press(h, in.undo);
  CHECK(mode(h) == LoopMode::Record);
  CHECK(h.length() <= H::frames);
  // Past the end of the first recording's bar, it goes on.
  beats(h, 2);
  CHECK(mode(h) == LoopMode::Record);
  beats(h, 4);
  CHECK(mode(h) == LoopMode::Play);
  CHECK(h.length() == 4 * H::beat);
}

//! A press acts on the state the one before it leaves: two presses of
//! Rec / Play / Dub before the grid record one grid step, then play.
template <typename H>
void presses_queue(H& h)
{
  auto& in = h.node.inputs;
  SECTION("Rec / Play / Dub twice")
  {
    press_off_beat(h, in.rec_play_dub);
    press(h, in.rec_play_dub);
    CHECK(h.shown == "Record");
    next_beat(h);
    CHECK(mode(h) == LoopMode::Record);
    CHECK(h.shown == "Play");
    next_beat(h);
    CHECK(mode(h) == LoopMode::Play);
    CHECK(h.length() == H::beat);
  }
  SECTION("Record twice is Record")
  {
    press_off_beat(h, in.record);
    press(h, in.record);
    next_beat(h);
    CHECK(mode(h) == LoopMode::Record);
    beats(h, 2);
    CHECK(mode(h) == LoopMode::Record);
  }
  SECTION("Rec / Play / Dub three times on a loop: overdub, play, overdub")
  {
    record_loop(h);
    press_off_beat(h, in.rec_play_dub);
    press(h, in.rec_play_dub);
    press(h, in.rec_play_dub);
    next_beat(h);
    CHECK(mode(h) == LoopMode::Overdub);
    next_beat(h);
    CHECK(mode(h) == LoopMode::Play);
    next_beat(h);
    CHECK(mode(h) == LoopMode::Overdub);
    CHECK(h.passes() == 3);
  }
  SECTION("Loop set in between drops what is queued")
  {
    record_loop(h);
    press_off_beat(h, in.rec_play_dub);
    press(h, in.rec_play_dub);
    in.mode = LoopMode::Stop;
    next_beat(h);
    CHECK(mode(h) == LoopMode::Stop);
    next_beat(h);
    CHECK(mode(h) == LoopMode::Stop);
  }
}

//! A cable sending Record on every tick asks for one recording: the looper
//! ending it after its bars does not make the cable ask again.
template <typename H>
void cable_on_loop(H& h)
{
  auto& in = h.node.inputs;
  in.postaction_bars.value = 1;
  const auto send = [&](LoopMode m, int ticks) {
    for(int i = 0; i < ticks; i++)
    {
      in.mode.value = m;
      h.node.loop_arrived();
      h.tick_once();
    }
  };
  send(LoopMode::Record, int(12 * H::beat / H::frames));
  CHECK(mode(h) == LoopMode::Play);
  CHECK(h.shown == "Play");
  CHECK(h.length() == 4 * H::beat);
  // A change of what it sends is a request.
  send(LoopMode::Stop, int(2 * H::beat / H::frames));
  CHECK(mode(h) == LoopMode::Stop);
  // The same value as the UI shows, coming back from it, asks nothing.
  in.mode.value = LoopMode::Stop;
  h.node.loop_arrived();
  h.tick_once();
  CHECK(mode(h) == LoopMode::Stop);
}

//! The m and x of a lane in the view act on the grid, as the actions do.
template <typename H>
void lanes_on_the_grid(H& h)
{
  four_layers(h);
  if(h.now % H::beat == 0)
    h.tick_once();
  h.node.process_message({.pass = 1, .layers = 4, .mute = true});
  h.tick_once();
  CHECK(!h.muted(1));
  next_beat(h);
  CHECK(h.muted(1));
  h.node.process_message({.pass = 2, .layers = 4, .mute = false});
  h.tick_once();
  CHECK(h.passes() == 4);
  next_beat(h);
  CHECK(h.passes() == 3);
  // Asked for a loop that changed since: nothing.
  h.node.process_message({.pass = 0, .layers = 4, .mute = false});
  next_beat(h);
  CHECK(h.passes() == 3);
}

//! Per cycle with Past the end on Extend: a layer each time the loop grows,
//! each closed as any.
template <typename H>
void per_cycle_extend(H& h)
{
  auto& in = h.node.inputs;
  record_loop(h);
  in.passes.value = Passes::PerCycle;
  in.past_end.value = PastEnd::Extend;
  while(h.position() != 0)
    h.tick_once();
  in.mode = LoopMode::Overdub;
  h.tick_once();
  REQUIRE(mode(h) == LoopMode::Overdub);
  beats(h, 9);
  // Two cuts: the loop grew twice.
  CHECK(h.length() == 12 * H::beat);
  CHECK(h.passes() == 4);
  in.mode = LoopMode::Play;
  next_beat(h);
  CHECK(!h.node.transport.is_taking());
  for(int k = 1; k < h.passes(); k++)
    CHECK(h.signature(k) > 0.);
}

//! The score stopping while overdubbing: the layer is closed as any, and
//! undo takes it out.
template <typename H>
void stop_closes_layer(H& h)
{
  auto& in = h.node.inputs;
  record_loop(h);
  at_beat(h, in.overdub);
  beats(h, 1);
  REQUIRE(h.node.transport.is_taking());
  h.node.stop();
  in.mode = LoopMode::Play;
  h.tick_once();
  CHECK(!h.node.transport.is_taking());
  CHECK(h.passes() == 2);
  beats(h, 1);
  CHECK(h.passes() == 2);
  CHECK(h.signature(1) > 0.);
  press(h, in.undo);
  CHECK(h.passes() == 1);
}

//! Record on a loop overdubs it, as on a looper pedal: by Record, and by
//! Mode set to Record. A new loop is recorded once the loop is cleared.
template <typename H>
void record_overdubs_a_loop(H& h)
{
  auto& in = h.node.inputs;
  record_loop(h);
  SECTION("Record")
  {
    press_off_beat(h, in.record);
    CHECK(in.mode.value == LoopMode::Overdub);
    CHECK(h.shown == "Overdub");
  }
  SECTION("Mode set to Record")
  {
    if(h.now % H::beat == 0)
      h.tick_once();
    in.mode = LoopMode::Record;
    h.tick_once();
    CHECK(in.mode.value == LoopMode::Overdub);
    CHECK(h.shown == "Overdub");
  }
  next_beat(h);
  CHECK(mode(h) == LoopMode::Overdub);
  beats(h, 1);
  at_beat(h, in.play);
  CHECK(mode(h) == LoopMode::Play);
  CHECK(h.passes() == 2);
  CHECK(h.length() == 4 * H::beat);
  CHECK(h.signature(0) > 0.);

  press(h, in.clear);
  CHECK(h.passes() == 0);
  press_off_beat(h, in.record);
  CHECK(in.mode.value == LoopMode::Record);
  next_beat(h);
  CHECK(mode(h) == LoopMode::Record);
  beats(h, 1);
  at_beat(h, in.play);
  CHECK(mode(h) == LoopMode::Play);
  CHECK(h.passes() == 1);
  CHECK(h.length() == 2 * H::beat);
}

//! The score stopped while recording or overdubbing, then started again with
//! Mode left as it is: the loop plays what the take left, and Mode says Play.
template <typename H>
void restart_after_stop(H& h)
{
  auto& in = h.node.inputs;
  SECTION("while overdubbing")
  {
    record_loop(h);
    at_beat(h, in.overdub);
    beats(h, 1);
    REQUIRE(mode(h) == LoopMode::Overdub);
  }
  SECTION("while recording")
  {
    at_beat(h, in.record);
    beats(h, 2);
    REQUIRE(mode(h) == LoopMode::Record);
  }
  SECTION("with a recording asked for on an empty loop")
  {
    press_off_beat(h, in.record);
    REQUIRE(mode(h) == LoopMode::Stop);
    REQUIRE(in.mode.value == LoopMode::Record);
  }
  REQUIRE(in.mode.value != LoopMode::Play);
  h.stop_score();
  const int passes = h.passes();
  const int64_t length = h.length();
  const auto after_stop = passes > 0 ? LoopMode::Play : LoopMode::Stop;
  CHECK(in.mode.value == after_stop);
  CHECK(h.shown == (passes > 0 ? "Play" : "Stop"));

  beats(h, 6);
  CHECK(mode(h) == after_stop);
  CHECK(!h.node.transport.is_taking());
  CHECK(h.passes() == passes);
  CHECK(h.length() == length);
  for(int k = 0; k < passes; k++)
    CHECK(h.signature(k) > 0.);
  CHECK(in.mode.value == after_stop);
}

//! Every action, from the modes it acts in, with no check in between: for
//! counting what they allocate.
template <typename H>
void exercise(H& h)
{
  auto& in = h.node.inputs;
  at_beat(h, in.rec_play_dub);
  beats(h, 3);
  at_beat(h, in.rec_play_dub);
  at_beat(h, in.rec_play_dub);
  at_beat(h, in.overdub);
  at_beat(h, in.new_layer);
  at_beat(h, in.new_layer);
  in.after_overdub.value = AfterOverdub::NewLayer;
  at_beat(h, in.rec_play_dub);
  at_beat(h, in.retrigger);
  at_beat(h, in.erase_layer);
  at_beat(h, in.play);
  at_beat(h, in.mute_layer);
  press(h, in.previous_layer);
  press(h, in.next_layer);
  at_beat(h, in.solo_layer);
  at_beat(h, in.solo_layer);
  at_beat(h, in.reverse_toggle);
  at_beat(h, in.half_speed);
  at_beat(h, in.double_speed);
  at_beat(h, in.reverse_toggle);
  at_beat(h, in.double_speed);
  at_beat(h, in.play_stop);
  at_beat(h, in.play_stop);
  press_off_beat(h, in.rec_play_dub);
  press(h, in.rec_play_dub);
  next_beat(h);
  next_beat(h);
  in.stop_at.value = StopAt::LoopEnd;
  at_beat(h, in.stop);
  beats(h, 4);
  in.stop_at.value = StopAt::Now;
  at_beat(h, in.play);
  press(h, in.stop);
  in.after_record.value = AfterRecord::Overdub;
  press(h, in.clear);
  at_beat(h, in.record);
  beats(h, 1);
  press(h, in.undo);
  beats(h, 1);
  at_beat(h, in.rec_play_dub);
  beats(h, 1);
  at_beat(h, in.play);
}
}

namespace looper_switches
{
//! Runs up to `frame`, the change asked for still waiting, and the tick that
//! frame falls in: the change is taken up there.
template <typename H>
void waits_until(H& h, int64_t frame, LoopMode before, LoopMode after)
{
  while(h.now + H::frames <= frame)
  {
    h.tick_once();
    REQUIRE(mode(h) == before);
  }
  REQUIRE(h.now <= frame);
  h.tick_once();
  CHECK(mode(h) == after);
}

//! Quantif on Loop: a change waits for the loop to come round to its start,
//! wherever that falls, and with 1/2 loop and 2x loop for its middle or every
//! other round. The loop is nineteen sixteenths long: its start falls inside a
//! tick, off the beats, and moves from one round to the next.
template <typename H>
void quantif_loop(H& h)
{
  auto& in = h.node.inputs;
  in.late.value = 0;
  constexpr int64_t sixteenth = H::beat / 4;
  constexpr int64_t len = 19 * sixteenth;

  in.quantif.value = halp::grid_value::note(1. / 16.);
  press_off_beat(h, in.record);
  // Taken up on the first sixteenth at or after the start of that tick
  const int64_t rec = (h.now - H::frames + sixteenth - 1) / sixteenth * sixteenth;
  const int64_t end = rec + len;
  while(h.now <= end - sixteenth)
    h.tick_once();
  press(h, in.play);
  while(h.now <= end)
    h.tick_once();
  REQUIRE(mode(h) == LoopMode::Play);
  REQUIRE(h.length() == len);
  // The loop goes round from `end`.

  SECTION("Loop: an overdub waits for the loop's start, past the beats")
  {
    in.quantif.value = halp::grid_value::period(1.);
    beats(h, 1);
    press(h, in.overdub);
    const int64_t restart = end + len;
    REQUIRE(restart % H::beat != 0);
    REQUIRE(restart % H::frames != 0);
    REQUIRE(restart - h.now > 2 * H::beat);
    waits_until(h, restart, LoopMode::Play, LoopMode::Overdub);
    CHECK(h.position() == h.now - restart);
  }
  SECTION("Loop: so does a stop")
  {
    in.quantif.value = halp::grid_value::period(1.);
    beats(h, 2);
    press(h, in.stop);
    waits_until(h, end + len, LoopMode::Play, LoopMode::Stop);
  }
  SECTION("1/2 loop: its start and its middle")
  {
    in.quantif.value = halp::grid_value::period(0.5);
    // A round in, past the start of the second round
    while(h.now < end + len)
      h.tick_once();
    h.tick_once();
    press(h, in.overdub);
    waits_until(h, end + len + len / 2, LoopMode::Play, LoopMode::Overdub);
    CHECK(h.position() == h.now - (end + len));
  }
  SECTION("2x loop: every other round, counted from the first")
  {
    in.quantif.value = halp::grid_value::period(2.);
    beats(h, 1);
    press(h, in.overdub);
    // The second round starts without it
    waits_until(h, end + 2 * len, LoopMode::Play, LoopMode::Overdub);
  }
}

//! Quantif on Loop before there is a loop: the length the recording will
//! have, Bars bars, or one bar when Bars is 0.
template <typename H>
void quantif_loop_empty(H& h)
{
  auto& in = h.node.inputs;
  in.late.value = 0;
  constexpr int64_t bar = 4 * H::beat;
  in.quantif.value = halp::grid_value::period(1.);

  SECTION("Bars bars")
  {
    in.postaction_bars.value = 2;
    while(h.now < bar + H::beat)
      h.tick_once();
    press(h, in.record);
    // Not on the next bar: on the next two bars
    waits_until(h, 2 * bar, LoopMode::Stop, LoopMode::Record);
    while(h.now < 4 * bar)
      h.tick_once();
    h.tick_once();
    CHECK(mode(h) == LoopMode::Play);
    CHECK(h.length() == 2 * bar);
  }
  SECTION("One bar when Bars is 0, while recording as well")
  {
    in.postaction_bars.value = 0;
    press_off_beat(h, in.record);
    waits_until(h, bar, LoopMode::Stop, LoopMode::Record);
    beats(h, 5);
    press(h, in.play);
    waits_until(h, 3 * bar, LoopMode::Record, LoopMode::Play);
    CHECK(h.length() == 2 * bar);
  }
  SECTION("2x loop: twice Bars")
  {
    in.postaction_bars.value = 1;
    in.quantif.value = halp::grid_value::period(2.);
    press_off_beat(h, in.record);
    waits_until(h, 2 * bar, LoopMode::Stop, LoopMode::Record);
  }
}
}

namespace looper_switches
{
//! Merge into newest, then a pass undone while it is taken in: the pass
//! goes, the newest layer stays as the merge left it, and the merge can still
//! be undone once the overdub that went on is merged in turn.
template <typename H>
void undo_while_taking_over_a_merge(H& h)
{
  auto& in = h.node.inputs;
  h.set_max_layers(3);
  record_loop(h);
  at_beat(h, in.overdub);
  at_beat(h, in.new_layer);
  at_beat(h, in.play);
  REQUIRE(h.passes() == 3);
  const double s2 = h.signature(2);
  at_beat(h, in.overdub);
  beats(h, 1);
  at_beat(h, in.play);
  beats(h, 1);
  REQUIRE(h.passes() == 3);
  const double merged = h.signature(2);
  REQUIRE(merged > s2);

  at_beat(h, in.overdub);
  beats(h, 1);
  REQUIRE(h.node.transport.is_taking());
  press(h, in.undo);
  CHECK(h.node.transport.is_taking());
  CHECK(h.passes() == 4);
  CHECK(same(h.signature(2), merged));
  at_beat(h, in.play);
  beats(h, 1);
  CHECK(h.passes() == 3);
  CHECK(h.signature(2) > merged);
  // The overdub that went on comes out of the newest layer, then the newest
  // layer goes as a whole: the first merge is not undone a second time.
  press(h, in.undo);
  CHECK(h.passes() == 3);
  CHECK(same(h.signature(2), merged));
  press(h, in.undo);
  CHECK(h.passes() == 2);
}

//! Merge oldest, the two oldest layers muted: a solo made while they merge is
//! undone by soloing again, whether the merge is done at once or over ticks.
template <typename H>
void solo_while_merging(H& h)
{
  auto& in = h.node.inputs;
  in.at_max_layers = AtMaxLayers::MergeOldest;
  h.set_max_layers(3);
  // A merge of a loop of eight beats takes more than two beats of ticks.
  h.slow_merges();
  press_off_beat(h, in.rec_play_dub);
  next_beat(h);
  beats(h, 7);
  press(h, in.rec_play_dub);
  next_beat(h);
  REQUIRE(h.length() == 8 * H::beat);
  at_beat(h, in.overdub);
  at_beat(h, in.new_layer);
  at_beat(h, in.play);
  REQUIRE(h.passes() == 3);
  for(int layer : {1, 2})
  {
    in.layer.value = layer;
    at_beat(h, in.mute_layer);
  }
  REQUIRE(h.muted(0));
  REQUIRE(h.muted(1));

  at_beat(h, in.overdub);
  // The layer under the one being overdubbed.
  in.layer.value = h.passes() - 1;
  at_beat(h, in.solo_layer);
  CHECK(h.muted(h.passes() - 1));
  CHECK(!h.muted(h.passes() - 2));
  at_beat(h, in.solo_layer);
  CHECK(!h.muted(h.passes() - 1));
  CHECK(!h.muted(h.passes() - 2));
}

//! A recording ended by its bars, the tempo having dropped while it went on:
//! it is cut back to the bars at the tempo it ends at, and what it recorded
//! past them goes into the overdub it gives way to, as a late stop's does.
template <typename H>
void bars_cut_carried_into_the_overdub(H& h)
{
  auto& in = h.node.inputs;
  in.postaction_bars.value = 1;
  in.after_record.value = AfterRecord::Overdub;
  // From a bar line.
  while(h.now % (4 * H::beat) != 4 * H::beat - H::frames)
    h.tick_once();
  in.mode = LoopMode::Record;
  next_beat(h);
  REQUIRE(mode(h) == LoopMode::Record);
  beats(h, 3);
  // A bar at 100 is longer than one at 120, and the bar the recording counts
  // ends later than it would have at 120: more is recorded than the bar.
  h.tempo = 100.;
  for(int i = 0; i < 100 && mode(h) == LoopMode::Record; i++)
    h.tick_once();
  REQUIRE(mode(h) == LoopMode::Overdub);
  CHECK(h.length() == 4 * H::beat * 120 / 100);
  REQUIRE(h.passes() == 2);
  const double first = h.signature(1);
  h.tick_once();
  const double per_tick = h.signature(1) - first;
  REQUIRE(per_tick > 0.);
  // The overdub's first tick, and the frames cut from the recording before it.
  CHECK(first > 2. * per_tick);
}

//! A layer merged with a muted one: the muted one is lost from what is
//! heard, and undoing the merge brings it back muted. Two muted layers merged
//! are one muted layer that has both.
template <typename H>
void merge_with_muted_layers(H& h)
{
  auto& in = h.node.inputs;
  h.set_max_layers(3);
  record_loop(h);
  at_beat(h, in.overdub);
  at_beat(h, in.new_layer);
  at_beat(h, in.play);
  REQUIRE(h.passes() == 3);
  const double s0 = h.signature(0), s1 = h.signature(1), s2 = h.signature(2);
  REQUIRE(s2 > 0.);
  SECTION("merged into a muted newest layer, and undone")
  {
    in.layer.value = 3;
    at_beat(h, in.mute_layer);
    REQUIRE(h.muted(2));
    at_beat(h, in.overdub);
    beats(h, 1);
    at_beat(h, in.play);
    beats(h, 1);
    REQUIRE(h.passes() == 3);
    CHECK(!h.muted(2));
    const double pass = h.signature(2);
    press(h, in.undo);
    CHECK(h.passes() == 3);
    CHECK(h.muted(2));
    CHECK(same(h.signature(2), s2));
    press(h, in.redo);
    beats(h, 1);
    CHECK(h.passes() == 3);
    CHECK(!h.muted(2));
    CHECK(same(h.signature(2), pass));
  }
  SECTION("the two oldest muted, merged")
  {
    in.at_max_layers = AtMaxLayers::MergeOldest;
    for(int layer : {1, 2})
    {
      in.layer.value = layer;
      at_beat(h, in.mute_layer);
    }
    at_beat(h, in.overdub);
    beats(h, 1);
    at_beat(h, in.play);
    beats(h, 1);
    REQUIRE(h.passes() == 3);
    CHECK(h.muted(0));
    CHECK(same(h.signature(0), s0 + s1));
    in.layer.value = 1;
    at_beat(h, in.mute_layer);
    CHECK(!h.muted(0));
    CHECK(same(h.signature(0), s0 + s1));
  }
}

//! Waits for the tick on the next beat; where the loop is then.
template <typename H>
int64_t up_to_beat(H& h)
{
  while(h.now % H::beat != 0)
    h.tick_once();
  return h.position();
}

//! An overdub pass is a layer from its first event on: one that gets none
//! is no layer, no take and no merge, and undo is as it was.
template <typename H>
void lazy_layers(H& h)
{
  auto& in = h.node.inputs;
  h.enable_takes();
  record_loop(h);
  // The recording, and a layer over it.
  at_beat(h, in.overdub);
  beats(h, 1);
  at_beat(h, in.play);
  REQUIRE(h.passes() == 2);
  REQUIRE(h.takes() == 2);
  const double s0 = h.signature(0), s1 = h.signature(1);

  SECTION("an overdub that gets nothing")
  {
    h.playing = false;
    at_beat(h, in.overdub);
    REQUIRE(mode(h) == LoopMode::Overdub);
    beats(h, 6);
    // It runs, over the whole loop and more, but is no layer.
    CHECK(h.node.transport.is_taking());
    CHECK(h.passes() == 2);
    at_beat(h, in.play);
    beats(h, 1);
    CHECK(mode(h) == LoopMode::Play);
    CHECK(h.passes() == 2);
    CHECK(h.takes() == 2);
    CHECK(same(h.signature(1), s1));
    // Undo takes out the last layer there is.
    press(h, in.undo);
    CHECK(h.passes() == 1);
    CHECK(same(h.signature(0), s0));
    press(h, in.redo);
    CHECK(h.passes() == 2);
  }
  SECTION("undo while it gets nothing")
  {
    h.playing = false;
    at_beat(h, in.overdub);
    beats(h, 1);
    press(h, in.undo);
    CHECK(h.passes() == 1);
    CHECK(mode(h) == LoopMode::Overdub);
    at_beat(h, in.play);
    CHECK(h.passes() == 1);
    CHECK(h.takes() == 2);
  }
  SECTION("the first event makes the layer, from where the overdub began")
  {
    h.playing = false;
    press_off_beat(h, in.overdub);
    const int64_t start = up_to_beat(h);
    h.tick_once();
    REQUIRE(mode(h) == LoopMode::Overdub);
    beats(h, 1);
    CHECK(h.passes() == 2);
    h.playing = true;
    h.tick_once();
    REQUIRE(h.passes() == 3);
    CHECK(h.node.loop.pass_start == start);
    // As far as it went since then: a beat and two ticks.
    CHECK(h.node.loop.pass_written == H::beat + 2 * H::frames);
    CHECK(h.signature(2) > 0.);
    at_beat(h, in.play);
    CHECK(h.passes() == 3);
    CHECK(h.takes() == 3);
  }
  SECTION("Per cycle: a round that gets nothing is no layer")
  {
    in.passes.value = Passes::PerCycle;
    while(h.position() != 0)
      h.tick_once();
    in.mode = LoopMode::Overdub;
    // A round with events, two without, one with.
    beats(h, 4);
    h.playing = false;
    beats(h, 8);
    CHECK(h.passes() == 3);
    h.playing = true;
    beats(h, 4);
    in.mode = LoopMode::Play;
    next_beat(h);
    CHECK(h.passes() == 4);
    CHECK(h.takes() == 4);
    for(int k = 0; k < h.passes(); k++)
      CHECK(h.signature(k) > 0.);
  }
  SECTION("at Max layers, room is made on the first event")
  {
    h.set_max_layers(2);
    const auto starve = [&] {
      h.playing = false;
      at_beat(h, in.overdub);
      beats(h, 2);
    };
    SECTION("merge oldest")
    {
      in.at_max_layers = AtMaxLayers::MergeOldest;
      starve();
      CHECK(h.passes() == 2);
      CHECK(same(h.signature(0), s0));
      CHECK(same(h.signature(1), s1));
      h.playing = true;
      h.tick_once();
      CHECK(h.passes() == 2);
      CHECK(same(h.signature(0), s0 + s1));
    }
    SECTION("merge into newest: nothing to merge")
    {
      in.at_max_layers = AtMaxLayers::MergeIntoNewest;
      starve();
      at_beat(h, in.play);
      beats(h, 1);
      CHECK(h.passes() == 2);
      CHECK(same(h.signature(1), s1));
      // Undo is as it was: the newest layer goes.
      press(h, in.undo);
      CHECK(h.passes() == 1);
    }
    SECTION("replace a muted layer")
    {
      in.at_max_layers = AtMaxLayers::ReplaceMuted;
      in.layer.value = 1;
      at_beat(h, in.mute_layer);
      REQUIRE(h.muted(0));
      starve();
      CHECK(h.passes() == 2);
      CHECK(h.muted(0));
      h.playing = true;
      h.tick_once();
      CHECK(h.passes() == 2);
      CHECK(!h.muted(0));
      CHECK(same(h.signature(0), s1));
    }
  }
}
}
