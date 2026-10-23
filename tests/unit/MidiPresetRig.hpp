#pragma once

// Runs a MIDI object at 48 kHz and 120 BPM (a quarter is 24000 frames, a
// sixteenth 6000) with a preset of the default package applied as the host
// applies it, and records what comes out.

#include "MidiPresetHarness.hpp"

#include <halp/audio.hpp>
#include <libremidi/message.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace midi_presets
{
static constexpr double rig_rate = 48000.;
static constexpr double rig_tempo = 120.;
static constexpr int64_t quarter = 24000;
static constexpr int64_t sixteenth = quarter / 4;
static constexpr int64_t bar = quarter * 4;

//! A choice (combobox) whose value is an int.
template <typename F>
concept int_choice = requires(F f) {
  typename F::range;
  (typename F::range{}).values[0].second;
  requires std::is_same_v<decltype(f.value), int>;
};

//! Applies a preset the way the host does: a choice whose value is an int is
//! saved by its name.
template <typename Inputs>
QStringList apply_as_host(Inputs& inputs, preset p, double tempo = rig_tempo)
{
  QJsonArray values;
  for(const auto& entry : p.values)
  {
    const auto pair = entry.toArray();
    const int idx = pair[0].toInt();
    QJsonObject val = pair[1].toObject();
    if(val.contains("String"))
    {
      const auto name = val["String"].toString().toStdString();
      int n = 0;
      avnd::pfr::for_each_field(inputs, [&](auto& field) {
        using F = std::decay_t<decltype(field)>;
        if(n++ != idx)
          return;
        if constexpr(int_choice<F>)
        {
          for(const auto& choice : typename F::range{}.values)
            if(choice.first == name)
              val = QJsonObject{{"Int", int(choice.second)}};
        }
      });
    }
    values.push_back(QJsonArray{idx, val});
  }
  p.values = values;
  return apply(inputs, p, tempo);
}

//! The preset called `name` in `folder`.
inline preset find(const QString& folder, const QString& uuid, const QString& name)
{
  for(auto& p : load(folder, uuid))
    if(p.name == name)
      return p;
  FAIL("no preset " << name.toStdString() << " in " << folder.toStdString());
  return {};
}

struct event
{
  int64_t at{};
  int status{}; //!< 0x90 / 0x80 for notes (a note-on at 0 is 0x80), else the type
  int channel{};
  int data1{};
  int data2{};
  bool on() const noexcept { return status == 0x90; }
  bool off() const noexcept { return status == 0x80; }
};

template <typename Node, typename Tick = halp::tick_musical>
struct rig
{
  Node node;
  int buffer{512};
  int64_t pos{};
  std::vector<event> events;
  std::vector<std::string> errors;
  std::map<std::pair<int, int>, int> sounding;

  rig()
  {
    if constexpr(requires { node.prepare(halp::setup{}); })
      node.prepare(
          {.input_channels = 0, .output_channels = 0, .frames = buffer, .rate = rig_rate});
  }

  QStringList load(const preset& p) { return apply_as_host(node.inputs, p); }

  using input = std::vector<std::pair<int64_t, libremidi::message>>;

  void tick(int frames, const input& in)
  {
    node.inputs.midi.midi_messages.clear();
    for(auto& [at, m] : in)
      if(at >= pos && at < pos + std::max(1, frames))
      {
        auto msg = m;
        msg.timestamp = at - pos;
        node.inputs.midi.midi_messages.push_back(msg);
      }
    node.outputs.midi.midi_messages.clear();
    Tick tk{};
    tk.frames = frames;
    tk.position_in_frames = pos;
    tk.start_position_in_quarters = double(pos) / quarter;
    tk.end_position_in_quarters = double(pos + frames) / quarter;
    tk.tempo = rig_tempo;
    tk.signature = {4, 4};
    node(tk);
    int64_t last = 0;
    for(auto& m : node.outputs.midi.midi_messages)
    {
      if(frames > 0 && (m.timestamp < 0 || m.timestamp >= frames))
        errors.push_back("timestamp " + std::to_string(m.timestamp) + " outside the tick");
      if(frames == 0 && m.timestamp != 0)
        errors.push_back("timestamp in a 0-frame tick");
      if(m.timestamp < last)
        errors.push_back("out of order at " + std::to_string(pos + m.timestamp));
      last = m.timestamp;
      record(m, pos + m.timestamp);
    }
    pos += frames;
  }

  void record(const libremidi::message& m, int64_t at)
  {
    event e{at, m.bytes[0] & 0xF0, (m.bytes[0] & 0x0F) + 1, m.size() > 1 ? m.bytes[1] : 0,
            m.size() > 2 ? m.bytes[2] : 0};
    if(m.bytes[0] >= 0xF0)
      e.status = m.bytes[0];
    if(e.status == 0x90 && e.data2 == 0)
      e.status = 0x80;
    events.push_back(e);
    if(!e.on() && !e.off())
      return;
    auto& n = sounding[{e.channel, e.data1}];
    if(e.on())
    {
      if(n > 0)
        errors.push_back("started twice: " + std::to_string(e.data1) + " at " + std::to_string(at));
      n++;
    }
    else if(n > 0)
      n--;
    else
      errors.push_back("stopped while silent: " + std::to_string(e.data1) + " at " + std::to_string(at));
  }

  //! Runs until the absolute frame `until`, feeding `in` at its frames.
  void run(int64_t until, const input& in = {})
  {
    while(pos < until)
      tick(int(std::min<int64_t>(buffer, until - pos)), in);
  }

  void stop()
  {
    node.stop();
    tick(0, {});
  }

  std::vector<event> ons() const
  {
    std::vector<event> r;
    for(auto& e : events)
      if(e.on())
        r.push_back(e);
    return r;
  }
  std::vector<event> offs() const
  {
    std::vector<event> r;
    for(auto& e : events)
      if(e.off())
        r.push_back(e);
    return r;
  }
  std::vector<int> on_pitches() const
  {
    std::vector<int> r;
    for(auto& e : ons())
      r.push_back(e.data1);
    return r;
  }
  std::vector<int64_t> on_dates() const
  {
    std::vector<int64_t> r;
    for(auto& e : ons())
      r.push_back(e.at);
    return r;
  }
  //! The end of the note of `pitch` that starts at `at`, or -1.
  int64_t end_of(int pitch, int64_t at, int channel = 1) const
  {
    bool started = false;
    for(auto& e : events)
    {
      if(e.channel != channel || e.data1 != pitch || !(e.on() || e.off()))
        continue;
      if(!started)
        started = e.on() && e.at == at;
      else if(e.off() || e.on())
        return e.at;
    }
    return -1;
  }

  //! Every note-on paired with one note-off, every timestamp in its tick.
  void check_clean() const
  {
    INFO((errors.empty() ? std::string{} : errors.front()));
    CHECK(errors.empty());
    int left = 0;
    for(auto& [k, n] : sounding)
      left += n;
    CHECK(left == 0);
  }
};

inline libremidi::message on(int pitch, int vel = 100, int chan = 1)
{
  return libremidi::channel_events::note_on(chan, pitch, vel);
}
inline libremidi::message off(int pitch, int vel = 0, int chan = 1)
{
  return libremidi::channel_events::note_off(chan, pitch, vel);
}
inline libremidi::message cc(int ctl, int value, int chan = 1)
{
  return libremidi::channel_events::control_change(chan, ctl, value);
}
}
