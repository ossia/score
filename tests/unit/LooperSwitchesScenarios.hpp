#pragma once
// The actions of the loopers' Switches tab, the same for the audio and the
// MIDI looper. Each test file drives its looper through a harness H:
//
//   H::beat                      frames of a quarter note, the grid
//   H::frames                    frames of a tick; a beat is whole ticks
//   h.node                       the looper
//   h.tick_once()                runs one tick
//   h.passes(), h.muted(k), h.length(), h.position()
//   h.shown                      the last mode Loop was shown with
//   h.shown_count                how many times it was
//
// The grid is a quarter note: a mode asked for between two beats is taken up
// on the tick that starts on the next one.

#include <Fx/Looper/Switches.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>

namespace looper_switches
{
using Nodes::Looper::AfterOverdub;
using Nodes::Looper::AfterRecord;
using Nodes::Looper::LoopMode;
using Nodes::Looper::StopAt;

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

//! Four passes: the recording, and three comped over it, then plays.
template <typename H>
void four_passes(H& h)
{
  auto& in = h.node.inputs;
  record_loop(h);
  for(int i = 0; i < 3; i++)
  {
    press_off_beat(h, in.comp_next);
    next_beat(h);
  }
  press_off_beat(h, in.play);
  next_beat(h);
  REQUIRE(h.passes() == 4);
  REQUIRE(mode(h) == LoopMode::Play);
}

template <typename H>
void rec_play_dub(H& h)
{
  auto& in = h.node.inputs;
  press_off_beat(h, in.rec_play_dub);
  // Loop shows what is asked for at once; the looper waits for the beat.
  CHECK(mode(h) == LoopMode::Stop);
  CHECK(in.mode.value == LoopMode::Record);
  CHECK(h.shown == "Record");
  next_beat(h);
  CHECK(mode(h) == LoopMode::Record);
  beats(h, 3);

  SECTION("after record: play")
  {
    press(h, in.rec_play_dub);
    CHECK(mode(h) == LoopMode::Record);
    next_beat(h);
    CHECK(mode(h) == LoopMode::Play);
    CHECK(h.shown == "Play");
    // Recorded from a beat to a beat.
    CHECK(h.length() == 4 * H::beat);

    press_off_beat(h, in.rec_play_dub);
    CHECK(mode(h) == LoopMode::Play);
    next_beat(h);
    CHECK(mode(h) == LoopMode::Overdub);
    CHECK(h.shown == "Overdub");
    CHECK(h.passes() == 2);
    beats(h, 1);

    SECTION("after overdub: play")
    {
      press_off_beat(h, in.rec_play_dub);
      next_beat(h);
      CHECK(mode(h) == LoopMode::Play);
      CHECK(h.shown == "Play");
      CHECK(h.passes() == 2);

      // Stopped with a loop, it plays.
      press_off_beat(h, in.stop);
      next_beat(h);
      CHECK(mode(h) == LoopMode::Stop);
      press_off_beat(h, in.rec_play_dub);
      next_beat(h);
      CHECK(mode(h) == LoopMode::Play);
    }
    SECTION("after overdub: a new layer, each press")
    {
      in.after_overdub.value = AfterOverdub::NewLayer;
      press_off_beat(h, in.rec_play_dub);
      next_beat(h);
      CHECK(mode(h) == LoopMode::Comp);
      CHECK(h.shown == "Comp");
      CHECK(h.passes() == 3);
      press_off_beat(h, in.rec_play_dub);
      CHECK(h.passes() == 3);
      next_beat(h);
      CHECK(mode(h) == LoopMode::Comp);
      CHECK(h.passes() == 4);
      CHECK(h.node.transport.is_taking());
    }
  }
  SECTION("after record: overdub")
  {
    in.after_record.value = AfterRecord::Overdub;
    press(h, in.rec_play_dub);
    next_beat(h);
    CHECK(mode(h) == LoopMode::Overdub);
    CHECK(h.shown == "Overdub");
    CHECK(h.length() == 4 * H::beat);
    CHECK(h.passes() == 2);
  }
  SECTION("after record: comp")
  {
    in.after_record.value = AfterRecord::Comp;
    press(h, in.rec_play_dub);
    next_beat(h);
    CHECK(mode(h) == LoopMode::Comp);
    CHECK(h.passes() == 2);
  }
}

//! Record from any mode, and Record while recording ends it.
template <typename H>
void record_action(H& h)
{
  auto& in = h.node.inputs;
  press_off_beat(h, in.record);
  next_beat(h);
  CHECK(mode(h) == LoopMode::Record);
  beats(h, 1);
  press_off_beat(h, in.record);
  next_beat(h);
  CHECK(mode(h) == LoopMode::Play);
  CHECK(h.length() == 2 * H::beat);

  // A new recording, in place of the loop.
  press_off_beat(h, in.overdub);
  next_beat(h);
  REQUIRE(h.passes() == 2);
  press_off_beat(h, in.record);
  next_beat(h);
  CHECK(mode(h) == LoopMode::Record);
  CHECK(h.passes() == 1);
  beats(h, 2);
  in.after_record.value = AfterRecord::Overdub;
  press_off_beat(h, in.record);
  next_beat(h);
  CHECK(mode(h) == LoopMode::Overdub);
  CHECK(h.length() == 3 * H::beat);
}

//! Overdub pressed while overdubbing does nothing: the pass goes on.
template <typename H>
void overdub_twice(H& h)
{
  auto& in = h.node.inputs;
  record_loop(h);
  press_off_beat(h, in.overdub);
  next_beat(h);
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
  CHECK(h.node.transport.is_taking());
  CHECK(!h.node.transport.new_pass_pending());
}

//! Three presses of Comp next are three new layers, the looper overdubbing
//! all along.
template <typename H>
void comp_next_three_times(H& h)
{
  auto& in = h.node.inputs;
  record_loop(h);
  for(int i = 0; i < 3; i++)
  {
    press_off_beat(h, in.comp_next);
    CHECK(h.passes() == 1 + i);
    next_beat(h);
    CHECK(mode(h) == LoopMode::Comp);
    CHECK(h.passes() == 2 + i);
    CHECK(h.node.transport.is_taking());
    beats(h, 1);
    CHECK(h.passes() == 2 + i);
  }
  CHECK(h.shown == "Comp");
  press_off_beat(h, in.play);
  next_beat(h);
  CHECK(mode(h) == LoopMode::Play);
  CHECK(h.passes() == 4);
}

//! Comp taken up from Overdub, by setting Loop, starts a new layer.
template <typename H>
void comp_from_overdub(H& h)
{
  auto& in = h.node.inputs;
  record_loop(h);
  in.mode = LoopMode::Overdub;
  next_beat(h);
  CHECK(h.passes() == 2);
  beats(h, 1);
  h.tick_once();
  in.mode = LoopMode::Comp;
  h.tick_once();
  CHECK(h.passes() == 2);
  next_beat(h);
  CHECK(mode(h) == LoopMode::Comp);
  CHECK(h.passes() == 3);

  // Comp next while recording ends the recording into the first layer.
  press_off_beat(h, in.record);
  next_beat(h);
  REQUIRE(mode(h) == LoopMode::Record);
  beats(h, 2);
  press_off_beat(h, in.comp_next);
  next_beat(h);
  CHECK(mode(h) == LoopMode::Comp);
  CHECK(h.passes() == 2);
  CHECK(h.length() == 3 * H::beat);
}

template <typename H>
void restart(H& h)
{
  auto& in = h.node.inputs;
  record_loop(h);
  beats(h, 1);
  press_off_beat(h, in.restart_loop);
  CHECK(h.node.transport.restart_pending());
  next_beat(h);
  // On the beat, back to the start: a tick into the loop since.
  CHECK(h.position() == H::frames);
  CHECK(mode(h) == LoopMode::Play);

  SECTION("an overdub goes on from the start, in the same layer")
  {
    press_off_beat(h, in.overdub);
    next_beat(h);
    beats(h, 1);
    REQUIRE(h.passes() == 2);
    press_off_beat(h, in.restart_loop);
    next_beat(h);
    CHECK(h.position() == H::frames);
    CHECK(mode(h) == LoopMode::Overdub);
    CHECK(h.passes() == 2);
    CHECK(h.node.transport.is_taking());
  }
  SECTION("a recording starts again")
  {
    press_off_beat(h, in.record);
    next_beat(h);
    beats(h, 2);
    press_off_beat(h, in.restart_loop);
    next_beat(h);
    CHECK(mode(h) == LoopMode::Record);
    CHECK(h.length() == H::frames);
    beats(h, 1);
    press_off_beat(h, in.record);
    next_beat(h);
    CHECK(h.length() == 2 * H::beat);
  }
}

template <typename H>
void erase_and_mute_last(H& h)
{
  auto& in = h.node.inputs;
  four_passes(h);
  press_off_beat(h, in.mute_last);
  CHECK(!h.muted(3));
  next_beat(h);
  CHECK(h.muted(3));
  press_off_beat(h, in.mute_last);
  next_beat(h);
  CHECK(!h.muted(3));

  press_off_beat(h, in.erase_last);
  CHECK(h.passes() == 4);
  next_beat(h);
  CHECK(h.passes() == 3);

  SECTION("while overdubbing, the layer being overdubbed is undone")
  {
    press_off_beat(h, in.overdub);
    next_beat(h);
    REQUIRE(h.passes() == 4);
    beats(h, 1);
    press_off_beat(h, in.erase_last);
    next_beat(h);
    // Dropped, and overdubbing goes on in a new one.
    CHECK(h.passes() == 4);
    CHECK(mode(h) == LoopMode::Overdub);
    CHECK(h.node.transport.is_taking());
    press_off_beat(h, in.play);
    next_beat(h);
    CHECK(h.passes() == 4);
  }
}

template <typename H>
void select_and_solo(H& h)
{
  auto& in = h.node.inputs;
  four_passes(h);
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
  press(h, in.previous_layer);
  CHECK(in.layer.value == 3);
  for(int i = 0; i < 5; i++)
    press(h, in.previous_layer);
  CHECK(in.layer.value == 1);

  // The first layer muted, then the third soloed.
  press(h, in.mute_layer);
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
    press_off_beat(h, in.solo_layer);
    next_beat(h);
    CHECK(h.muted(0));
    CHECK(!h.muted(1));
    CHECK(!h.muted(2));
    CHECK(!h.muted(3));
  }
  SECTION("a mute in between drops the mutes from before")
  {
    press_off_beat(h, in.mute_last);
    next_beat(h);
    REQUIRE(!h.muted(3));
    press_off_beat(h, in.solo_layer);
    next_beat(h);
    // A solo of its own.
    CHECK(h.muted(0));
    CHECK(h.muted(1));
    CHECK(!h.muted(2));
    CHECK(h.muted(3));
  }
  SECTION("Mute layer and Erase layer act on the selected layer")
  {
    press(h, in.mute_layer);
    CHECK(h.muted(2) == true);
    press(h, in.erase_layer);
    CHECK(h.passes() == 3);
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
  press_off_beat(h, in.reverse_toggle);
  next_beat(h);
  CHECK(!in.reverse.value);
  CHECK(!reverse_shown);

  press_off_beat(h, in.half_speed);
  CHECK(in.speed.value == 1.f);
  next_beat(h);
  CHECK(in.speed.value == 0.5f);
  CHECK(speed_shown == 0.5f);
  press_off_beat(h, in.half_speed);
  next_beat(h);
  CHECK(in.speed.value == 1.f);
  press_off_beat(h, in.double_speed);
  next_beat(h);
  CHECK(in.speed.value == 2.f);
  CHECK(speed_shown == 2.f);
  press_off_beat(h, in.half_speed);
  next_beat(h);
  CHECK(in.speed.value == 0.5f);
  press_off_beat(h, in.double_speed);
  next_beat(h);
  CHECK(in.speed.value == 2.f);
  press_off_beat(h, in.double_speed);
  next_beat(h);
  CHECK(in.speed.value == 1.f);
  // The loop played on all along.
  CHECK(mode(h) == LoopMode::Play);
}

//! A recording the post-action ends shows the mode it gave way to, so that
//! asking for Record again is a change.
template <typename H>
void postaction_updates_loop(H& h)
{
  auto& in = h.node.inputs;
  in.postaction_bars.value = 1;
  in.mode = LoopMode::Record;
  next_beat(h);
  REQUIRE(mode(h) == LoopMode::Record);
  // To the bar line a bar after the start at least.
  beats(h, 8);
  CHECK(mode(h) == LoopMode::Play);
  CHECK(in.mode.value == LoopMode::Play);
  CHECK(h.shown == "Play");
  in.mode = LoopMode::Record;
  next_beat(h);
  CHECK(mode(h) == LoopMode::Record);
}

template <typename H>
void empty_loop(H& h)
{
  auto& in = h.node.inputs;
  press_off_beat(h, in.play_stop);
  CHECK(in.mode.value == LoopMode::Stop);
  CHECK(h.shown_count == 0);
  press_off_beat(h, in.restart_loop);
  CHECK(!h.node.transport.restart_pending());
  press_off_beat(h, in.erase_last);
  press_off_beat(h, in.mute_last);
  press_off_beat(h, in.solo_layer);
  press(h, in.next_layer);
  CHECK(in.layer.value == 1);
  next_beat(h);
  CHECK(h.passes() == 0);
  CHECK(mode(h) == LoopMode::Stop);

  // Play / Stop plays a loop, and stops anything else.
  record_loop(h);
  press_off_beat(h, in.play_stop);
  next_beat(h);
  CHECK(mode(h) == LoopMode::Stop);
  press_off_beat(h, in.play_stop);
  next_beat(h);
  CHECK(mode(h) == LoopMode::Play);
  press_off_beat(h, in.overdub);
  next_beat(h);
  press_off_beat(h, in.play_stop);
  next_beat(h);
  CHECK(mode(h) == LoopMode::Stop);
}

//! Stop at: the grid, the loop's end, or at once.
template <typename H>
void stop_at(H& h)
{
  auto& in = h.node.inputs;
  record_loop(h);
  // A beat into the loop, and a tick.
  beats(h, 1);
  h.tick_once();
  REQUIRE(h.position() == H::beat + 2 * H::frames);

  SECTION("now")
  {
    in.stop_at.value = StopAt::Now;
    press(h, in.stop);
    CHECK(mode(h) == LoopMode::Stop);
  }
  SECTION("on the grid")
  {
    press(h, in.play_stop);
    CHECK(mode(h) == LoopMode::Play);
    next_beat(h);
    CHECK(mode(h) == LoopMode::Stop);
  }
  SECTION("at the loop's end, through Loop")
  {
    in.stop_at.value = StopAt::LoopEnd;
    in.mode = LoopMode::Stop;
    next_beat(h);
    CHECK(mode(h) == LoopMode::Play);
    // Plays on until the playhead is back at the loop's start, and stops there.
    int64_t before = h.position();
    for(int i = 0; i < 100 && mode(h) == LoopMode::Play; i++)
    {
      before = h.position();
      h.tick_once();
    }
    CHECK(mode(h) == LoopMode::Stop);
    CHECK(before == 0);
    CHECK(h.position() == 0);
  }
}

//! Every action, from the modes it acts in, with no check in between: for
//! counting what they allocate.
template <typename H>
void exercise(H& h)
{
  auto& in = h.node.inputs;
  auto at_beat = [&](auto& button) {
    press_off_beat(h, button);
    next_beat(h);
  };
  at_beat(in.rec_play_dub);
  beats(h, 3);
  at_beat(in.rec_play_dub);
  at_beat(in.rec_play_dub);
  at_beat(in.overdub);
  at_beat(in.comp_next);
  at_beat(in.comp_next);
  in.after_overdub.value = AfterOverdub::NewLayer;
  at_beat(in.rec_play_dub);
  at_beat(in.restart_loop);
  at_beat(in.erase_last);
  at_beat(in.play);
  at_beat(in.mute_last);
  press(h, in.previous_layer);
  press(h, in.next_layer);
  at_beat(in.solo_layer);
  at_beat(in.solo_layer);
  at_beat(in.reverse_toggle);
  at_beat(in.half_speed);
  at_beat(in.double_speed);
  at_beat(in.reverse_toggle);
  at_beat(in.double_speed);
  at_beat(in.play_stop);
  at_beat(in.play_stop);
  in.stop_at.value = StopAt::LoopEnd;
  at_beat(in.stop);
  beats(h, 4);
  in.stop_at.value = StopAt::Now;
  at_beat(in.play);
  press(h, in.stop);
  in.after_record.value = AfterRecord::Overdub;
  at_beat(in.record);
  beats(h, 1);
  at_beat(in.record);
  beats(h, 1);
  at_beat(in.play);
}
}
