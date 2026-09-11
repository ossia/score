#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
#include "VelToNoteCore.hpp"

#include <Fx/Types.hpp>

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/token_request.hpp>
#include <ossia/dataflow/value_port.hpp>

#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <libremidi/message.hpp>

#include <cmath>

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace Nodes::PulseToNote
{
namespace detail
{
struct NumericValue
{
  std::optional<double> operator()() const noexcept { return std::nullopt; }
  std::optional<double> operator()(int x) const noexcept { return x; }
  std::optional<double> operator()(char x) const noexcept { return x; }
  std::optional<double> operator()(float x) const noexcept { return x; }
  template <typename T>
  std::optional<double> operator()(const T&) const noexcept
  {
    return std::nullopt;
  }
};

struct ValueDecoder
{
  int base_pitch{}, base_velocity{};
  std::optional<DecodedNote> operator()() const noexcept { return std::nullopt; }
  std::optional<DecodedNote> operator()(ossia::impulse) const noexcept
  {
    return decode_pair(base_pitch, base_velocity, false);
  }
  std::optional<DecodedNote> operator()(int pitch) const noexcept
  {
    return decode_pair(pitch, base_velocity, false);
  }
  std::optional<DecodedNote> operator()(char pitch) const noexcept
  {
    return decode_pair(pitch, base_velocity, false);
  }
  std::optional<DecodedNote> operator()(float pitch) const noexcept
  {
    return decode_pair(pitch, base_velocity, false);
  }
  template <typename T>
  std::optional<DecodedNote> operator()(const T&) const noexcept
  {
    return std::nullopt;
  }
  std::optional<DecodedNote>
  operator()(const std::vector<ossia::value>& list) const noexcept
  {
    if(list.empty())
      return operator()(ossia::impulse{});
    const auto pitch = list[0].apply(NumericValue{});
    if(!pitch)
      return std::nullopt;
    if(list.size() == 1)
      return decode_pair(*pitch, base_velocity, false);
    const auto velocity = list[1].apply(NumericValue{});
    if(!velocity)
      return std::nullopt;
    // Extra elements are ignored, but NEVER change how the first pair is decoded.
    return decode_pair(*pitch, *velocity, list[1].get_type() == ossia::val_type::FLOAT);
  }
  template <std::size_t N>
  std::optional<DecodedNote>
  operator()(const std::array<float, N>& vector) const noexcept
  {
    if constexpr(N >= 2)
      return decode_pair(vector[0], vector[1], true);
    else if constexpr(N == 1)
      return decode_pair(vector[0], base_velocity, false);
    else
      return operator()(ossia::impulse{});
  }
};

// Recover the chooser's exact notated durations from its float round-trip.
// Without this, e.g. a quarter at 90 BPM becomes 1.00000003 quarters and the
// conservative duration-to-sample ceil schedules its release one sample late.
// These are the 22 score QGraphicsTimeChooser detents, expressed in quarters.
// Arbitrary automated values outside float rounding error are NOT snapped.
inline double canonical_quarters(double q) noexcept
{
  constexpr double notes[]{1. / 16, 1. / 12, 1. / 8, 1. / 6, 3. / 16, 1. / 4,
                           1. / 3,  3. / 8,  1. / 2, 2. / 3, 3. / 4,  1.,
                           4. / 3,  3. / 2,  2.,     8. / 3, 3.,      4.,
                           6.,      8.,      12.,    16.};
  if(!std::isfinite(q) || q <= 0.)
    return q;
  for(const auto n : notes)
    if(std::abs(q - n) <= 4. * std::numeric_limits<float>::epsilon() * n)
      return n;
  return q;
}

// The ossia binding passes free values through. In synchronized mode it gives
// q * 60 / tempo, where q is the selected quarter-note duration. Recover q ONCE.
inline Settings
settings_from_time_chooser(Settings s, double value, bool sync, double tempo) noexcept
{
  s.duration_unit = sync ? DurationUnit::quarters : DurationUnit::model_seconds;
  s.duration = sync ? (std::isfinite(tempo) && tempo > 0.
                           ? canonical_quarters(value * tempo / 60.)
                           : 0.)
                    : value;
  return sanitize(s);
}
} // namespace detail

struct Node
{
  halp_meta(name, "Pulse to Midi")
  halp_meta(c_name, "VelToNote")
  halp_meta(category, "Midi")
  halp_meta(author, "ossia score")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/midi-utilities.html#pulse-to-note")
  halp_meta(uuid, "2c6493c3-5449-4e52-ae04-9aee3be5fb6a")
  halp_meta(process_exec, true)
  // Aggregate already-produced slices until the graph's begin_execution boundary.
  // Pending notes and future releases remain exclusively in engine_.
  halp_meta(local_midi_tick_batch, true)
  halp_meta(local_midi_tick_batch_reserve, detail::Engine<>::max_output_events)
  halp_meta(
      description,
      "Impulse: default pitch and velocity. Scalar number: pitch. "
      "[pitch, integer velocity]: MIDI velocity; float velocity: normalized 0..1. "
      "Zero velocity releases owned notes for that source pitch. "
      "Starts may be quantized; ends use a grid or a duration. "
      "Tightness blends from immediate (0) to the next grid point (1).")

  struct
  {
    // Existing port order is retained. New controls are appended, not inserted.
    ossia_port<"in", ossia::value_port, true> port{};
    quant_selector<"Start quant."> start_quant;
    halp::hslider_f32<"Tightness", halp::range{0.f, 1.f, 0.8f}> tightness;
    quant_selector<"End quant."> end_quant;
    midi_spinbox<"Default pitch"> basenote;
    midi_spinbox<"Default vel."> basevel;
    octave_slider<"Pitch shift", -5, 5> shift_note;
    octave_slider<"Pitch random", 0, 2> note_random;
    octave_slider<"Vel. random", 0, 2> vel_random;
    midi_channel<"Channel"> channel;
    halp::toggle<"Fixed duration"> fixed_duration;
    halp::time_chooser<"Duration", halp::range{0., 10., 0.}> duration;
    halp::enum_t<detail::PitchDirection, "Pitch direction"> pitch_direction;
  } inputs;
  struct
  {
    midi_out midi;
  } outputs;

  // score's Crousti::Executor injects this member. A native token plus the
  // facade gives the EXACT executed sample span, including partial callbacks.
  // Avendish explicitly supports using ossia::token_request as T::tick.
  using tick = ossia::token_request;
  ossia::exec_state_facade ossia_state{};
  struct Setup
  {
    int frames{};
    double rate{};
    std::uint64_t instance{};
  };

  void prepare(Setup setup)
  {
    // Reserve the outer message buffer off the audio thread. Allocation-free
    // publication additionally requires inline short-message storage in
    // libremidi and adequately reserved buffers in the host binding.
    outputs.midi.midi_messages.reserve(detail::Engine<>::max_output_events);
    if(rate_ != setup.rate)
      engine_.request_reset();
    rate_ = setup.rate;
    if(!seeded_)
    {
      engine_.seed(setup.instance ^ 0xa35371f99772b987ULL);
      seeded_ = true;
    }
  }

  // Processing-thread hooks only: never write, retain, or read output messages.
  void start() noexcept
  {
    running_ = true;
    engine_.request_reset();
  }
  void stop() noexcept
  {
    running_ = false;
    engine_.request_reset();
  }
  void pause() noexcept { stop(); }
  void resume() noexcept { start(); }
  template <typename Time>
  void transport(Time) noexcept
  {
    engine_.request_reset();
  }

  [[nodiscard]] const detail::Statistics& statistics() const noexcept
  {
    return engine_.statistics();
  }
  [[nodiscard]] bool needs_service() const noexcept { return engine_.needs_service(); }

  // A host with no normal tick after stop/removal must provide one final writable
  // cleanup tick before destroying the node or its route. This is a TICK API,
  // not an out-of-band write from stop(). No output survives as deferred state.
  void cleanup_tick(const tick& tk) noexcept
  {
    outputs.midi.midi_messages.clear();
    engine_.begin_input();
    engine_.request_reset();
    if(const auto span = execution_span(tk); span && span->length > 0)
    {
      auto sink = [this](const detail::MidiEvent& e) noexcept { emit(e); };
      engine_.drain_releases(0, static_cast<int>(span->length), sink);
    }
  }

  void operator()(const tick& tk) noexcept
  {
    // A stop may leave graph tokens already requested by the interval. They
    // may publish cleanup, but cannot trigger again until start or resume.
    if(!running_)
    {
      cleanup_tick(tk);
      return;
    }
    // Output ports are scratch space for this invocation only. Host code may
    // clear/move/destroy their message contents immediately after publication.
    outputs.midi.midi_messages.clear();
    engine_.begin_input();
    const auto span = execution_span(tk);
    if(!span)
    {
      engine_.request_reset();
      return;
    }

    const auto total = ossia_state.samplesSinceStart();
    const auto size = ossia_state.bufferSize();
    // In score's buffer_tick and precise_score_tick, the counter has already
    // advanced to the END of the current buffer when a dataflow node runs.
    if(total < std::numeric_limits<std::int64_t>::min() + size
       || total - size > std::numeric_limits<std::int64_t>::max() - span->start_sample)
    {
      engine_.request_reset();
      return;
    }
    const detail::Block b{
        .frames = static_cast<int>(span->length),
        .first_frame = total - size + span->start_sample,
        .quarters_begin = tk.musical_start_position,
        .quarters_end = tk.musical_end_position,
        .numerator = tk.signature.upper,
        .denominator = tk.signature.lower,
        .bar_begin = tk.musical_start_last_bar,
        .bar_end = tk.musical_end_last_bar,
        .last_signature = tk.musical_start_last_signature,
        .model_begin = tk.prev_date.impl,
        .model_end = tk.date.impl,
        .discontinuity = tk.start_discontinuous,
        .end_discontinuity = tk.end_discontinuous};

    const detail::ValueDecoder decoder{inputs.basenote.value, inputs.basevel.value};
    if(inputs.port.value && b.frames > 0)
    {
      const auto& data = inputs.port.value->get_data();
      // Bounded scanning, including malformed and out-of-slice inputs. A
      // discarded suffix could contain a release; fail closed on overload.
      if(data.size() > detail::Engine<>::max_inputs)
        engine_.input_overflow();
      else
        for(const auto& input : data)
        {
          if(input.timestamp < span->start_sample
             || input.timestamp - span->start_sample >= span->length)
            continue;
          if(const auto note = input.value.apply(decoder))
            if(!engine_.push(
                   static_cast<int>(input.timestamp - span->start_sample), *note))
              break;
        }
    }
    detail::Settings settings{
        .start_quant = inputs.start_quant.value,
        .tightness = inputs.tightness.value,
        .end_quant = inputs.end_quant.value,
        .end_mode = inputs.fixed_duration.value ? detail::EndMode::duration
                                                : detail::EndMode::quantized,
        .channel = inputs.channel.value,
        .pitch_shift = inputs.shift_note.value,
        .pitch_random = inputs.note_random.value,
        .velocity_random = inputs.vel_random.value,
        .pitch_direction = inputs.pitch_direction.value};
    settings = detail::settings_from_time_chooser(
        settings, inputs.duration.value, inputs.duration.sync, tk.tempo);
    auto sink = [this](const detail::MidiEvent& e) noexcept { emit(e); };
    engine_.process(b, settings, sink);
  }

  // Public data preserve aggregate reflection. Only these local members survive
  // invocations; none is a pointer/reference/iterator into a message port.
  detail::Engine<> engine_{};
  bool running_{true};
  double rate_{};
  bool seeded_{};

  std::optional<ossia::exec_state_facade::sample_timings>
  execution_span(const tick& tk) const noexcept
  {
    if(!ossia_state.impl || ossia_state.bufferSize() < 0)
      return std::nullopt;
    const auto size = ossia_state.bufferSize();
    // Do not rescale the full model interval into a host-clipped physical span.
    // A malformed carried span is rejected, leaving releases local for retry.
    if(tk.start_sample >= 0 && tk.length_sample >= 0
       && (tk.start_sample > size || tk.length_sample > size - tk.start_sample))
      return std::nullopt;
    const auto span = ossia_state.timings(tk);
    if(span.start_sample < 0 || span.length < 0 || span.start_sample > size
       || span.length > size - span.start_sample)
      return std::nullopt;
    return span;
  }

  void emit(const detail::MidiEvent& e) noexcept
  {
    auto& msg = outputs.midi.midi_messages.emplace_back();
    msg.bytes.assign(
        {static_cast<unsigned char>((e.on ? 0x90 : 0x80) | e.channel), e.pitch,
         e.velocity});
    // Slice-relative. Avendish's MIDI postprocessor adds the slice start once.
    msg.timestamp = e.frame;
  }
};
} // namespace Nodes::PulseToNote
