#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Allocation-free, host-independent scheduling for Nodes::PulseToNote::Node.

#include <cmath>

#include <algorithm>
#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>

namespace Nodes::PulseToNote::detail
{
using frame_t = std::int64_t;
using position_t = long double;
inline constexpr position_t flicks_per_second = 705600000.L;

enum class EndMode
{
  quantized,
  duration
};
enum class DurationUnit
{
  model_seconds,
  quarters
};
enum class PitchDirection
{
  Both,
  Higher,
  Lower
};

struct DecodedNote
{
  std::uint8_t pitch{};    // Source identity, BEFORE transposition / randomization.
  std::uint8_t velocity{}; // Zero is an explicit release.
};

inline std::optional<int> midi_number(double x) noexcept
{
  if(!std::isfinite(x))
    return std::nullopt;
  return static_cast<int>(std::clamp(x, 0., 127.));
}

// Float velocities are normalized; integer velocities use the MIDI range.
// A positive normalized velocity remains a note-on, even below 1 / 127.
inline std::optional<DecodedNote>
decode_pair(double pitch, double velocity, bool normalized_velocity) noexcept
{
  const auto p = midi_number(pitch);
  if(!p || !std::isfinite(velocity))
    return std::nullopt;
  const double v = normalized_velocity ? std::clamp(velocity, 0., 1.) * 127.
                                       : std::clamp(velocity, 0., 127.);
  const int iv = v > 0. ? std::max(1, static_cast<int>(v)) : 0;
  return DecodedNote{static_cast<std::uint8_t>(*p), static_cast<std::uint8_t>(iv)};
}

struct Settings
{
  // The score selectors contain fractions of a whole note, NOT rates.
  // For compatibility with score, 1 and above mean one bar and multiples
  // of a bar. Fractions below 1 mean 4 * value quarter notes, reset per bar.
  double start_quant{0.25};
  double tightness{1.};
  double end_quant{0.25}; // > 0: grid; == 0: one sample; < 0: explicit release.
  EndMode end_mode{EndMode::quantized};
  DurationUnit duration_unit{DurationUnit::model_seconds};
  double duration{}; // Already converted to the unit above by the host adapter.
  int channel{1};    // User-facing, one-based.
  int pitch_shift{};
  int pitch_random{};
  int velocity_random{};
  PitchDirection pitch_direction{PitchDirection::Both};
};

struct Block
{
  int frames{};
  frame_t first_frame{}; // Absolute AUDIO clock of this slice's first sample.
  double quarters_begin{};
  double quarters_end{}; // One-past-end musical position.
  int numerator{4};
  int denominator{4};
  double bar_begin{}; // Last bar line at slice start.
  double bar_end{};   // Last bar line at slice end.
  double last_signature{};
  std::int64_t model_begin{}; // Logical flicks; NOT the wall / sample clock.
  std::int64_t model_end{};
  bool discontinuity{};
  bool end_discontinuity{};
};

struct MidiEvent
{
  int frame{};            // Slice-relative; always in [0, Block::frames).
  std::uint8_t channel{}; // Wire format, ZERO-based.
  std::uint8_t pitch{};
  std::uint8_t velocity{}; // Release velocity is deliberately zero.
  bool on{};
  friend bool operator==(const MidiEvent&, const MidiEvent&) = default;
};

struct Statistics
{
  std::uint64_t input_overflows{};
  std::uint64_t rejected_inputs{};
  std::uint64_t dropped_triggers{};
  std::uint64_t coalesced_starts{};
  std::uint64_t retriggers{};
  std::uint64_t resets{};
  std::uint64_t invalid_blocks{};
  std::uint64_t output_failures{};
};

inline double finite_clamp(double x, double lo, double hi, double fallback) noexcept
{
  return std::isfinite(x) ? std::clamp(x, lo, hi) : fallback;
}

inline Settings sanitize(Settings s) noexcept
{
  // Explicit lower bound prevents unrepresentable / denormal grid spacing.
  auto quant = [](double q) {
    if(!std::isfinite(q) || q <= 0.)
      return 0.;
    return std::clamp(q, 1. / 1024., 64.);
  };
  s.start_quant = quant(s.start_quant);
  s.tightness = finite_clamp(s.tightness, 0., 1., 1.);
  s.end_quant
      = std::isfinite(s.end_quant) && s.end_quant < 0. ? -1. : quant(s.end_quant);
  s.duration = finite_clamp(s.duration, 0., 86400., 0.);
  s.channel = std::clamp(s.channel, 1, 16);
  s.pitch_shift = std::clamp(s.pitch_shift, -127, 127);
  s.pitch_random = std::clamp(s.pitch_random, 0, 127);
  s.velocity_random = std::clamp(s.velocity_random, 0, 127);
  return s;
}

// Correct only rounding noise at a sample boundary, never quantize a duration
// to a grid. Interpolation uses wide arithmetic; endpoint inputs may be doubles.
inline position_t clean_integer(position_t x) noexcept
{
  const auto n = std::round(x);
  const auto eps = std::clamp(
      64.L * std::numeric_limits<double>::epsilon() * std::max(1.L, std::abs(x)), 1e-9L,
      1e-5L);
  return std::abs(x - n) <= eps ? n : x;
}
template <typename T>
inline int direction(T a, T z) noexcept
{
  return (z > a) - (z < a);
}

// Analytic grid: no per-block grid allocation and no arbitrary event-count cap.
// Fractions < 1 are whole-note fractions, reset at every bar. Values >= 1
// denote multiples of a bar counted from the last signature change, as in score.
class Grid
{
public:
  explicit Grid(const Block& b) noexcept
      : length_{
            b.numerator > 0 && b.denominator > 0 ? 4.L * b.numerator / b.denominator
                                                 : 4.L}
      , near_{std::min(b.bar_begin, b.bar_end)}
      , far_{std::max(b.bar_begin, b.bar_end)}
      , origin_{b.last_signature}
  {
    const auto lo = std::min(b.quarters_begin, b.quarters_end);
    const auto hi = std::max(b.quarters_begin, b.quarters_end);
    // Defensive fallback only for missing/inconsistent bar metadata.
    if(near_ > lo || near_ + length_ < lo)
      near_ = origin_ + std::floor((lo - origin_) / length_) * length_;
    if(far_ < near_ || far_ > hi)
      far_ = near_;
  }

  [[nodiscard]] position_t
  next(position_t q, double selection, int dir, bool strict = false) const noexcept
  {
    if(selection >= 1.)
    {
      const auto unit = length_ * selection;
      const auto x = clean_integer((q - origin_) / unit);
      const auto k = dir > 0 ? (strict ? std::floor(x) + 1.L : std::ceil(x))
                             : (strict ? std::ceil(x) - 1.L : std::floor(x));
      return origin_ + k * unit;
    }
    const auto unit = 4.L * selection;
    const auto anchor = q >= far_ ? far_ : near_;
    const auto bar
        = anchor + std::floor(clean_integer((q - anchor) / length_)) * length_;
    auto end = bar + length_;
    if(bar < far_ && end > far_)
      end = far_; // An irregular reported bar line.
    const auto x = clean_integer((q - bar) / unit);
    if(dir > 0)
    {
      const auto k = strict ? std::floor(x) + 1.L : std::ceil(x);
      return std::min(bar + k * unit, end);
    }
    const auto k = strict ? std::ceil(x) - 1.L : std::floor(x);
    if(k >= 0.L)
      return bar + k * unit;
    // We crossed the start of this bar. The preceding bar can be truncated.
    const auto previous
        = bar == far_ && far_ > near_
              ? near_
                    + (std::ceil(clean_integer((far_ - near_) / length_)) - 1.L)
                          * length_
              : bar - length_;
    const auto last = std::ceil(clean_integer((bar - previous) / unit)) - 1.L;
    return previous + std::max(0.L, last) * unit;
  }

private:
  position_t length_, near_, far_, origin_;
};

// Directed phase uses the actual adjacent points, including the short cell at
// an odd-meter bar boundary. Every target is causal in either travel direction.
inline position_t onset_target(
    const Grid& grid, position_t arrival, double quant, double tightness,
    int dir) noexcept
{
  if(quant <= 0. || tightness <= 0.)
    return arrival;
  const auto next = grid.next(arrival, quant, dir);
  if(next == arrival)
    return arrival;
  if(tightness >= 1.)
    return next;
  const auto previous = grid.next(arrival, quant, -dir);
  const auto phase = (arrival - previous) / (next - previous);
  const auto grace = (1.L - tightness) / 2.L;
  const auto x = std::clamp((phase - grace) / grace, 0.L, 1.L);
  const auto weight = tightness * x * x * (3.L - 2.L * x);
  return arrival + weight * (next - arrival);
}

// Not a source of cryptographic randomness. Instance-owned, reproducible, no
// locking / allocation / process-global random generator on the audio thread.
class Random
{
public:
  void seed(std::uint64_t value) noexcept { state_ = value; }
  int between(int low, int high) noexcept
  {
    if(low == high)
      return low;
    state_ += 0x9e3779b97f4a7c15ULL;
    auto z = state_;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    z ^= z >> 31;
    // At most 255 outcomes. The modulo bias is negligible (less than 2^-56).
    return low + static_cast<int>(z % static_cast<unsigned>(high - low + 1));
  }
  int symmetric(int radius) noexcept { return between(-radius, radius); }

private:
  std::uint64_t state_{0x71cc53a295a3b2e1ULL};
};

// Single execution thread. The sink accepts only CURRENT-slice messages.
// A void sink promises delivery; a bool sink can refuse a message. Refused
// note-offs are retained locally and must succeed before any more note-ons.
template <std::size_t MaxVoices = 256, std::size_t MaxInputs = 4096>
class Engine
{
  static_assert(MaxVoices > 0 && MaxInputs > 0);
  static constexpr frame_t frame_limit = std::numeric_limits<frame_t>::max() / 4;
  enum class State : std::uint8_t
  {
    free,
    pending,
    active
  };
  enum class EndKind : std::uint8_t
  {
    grid,
    sample,
    model,
    musical,
    hold
  };
  struct Voice
  {
    State state{};
    EndKind end_kind{};
    std::uint8_t source{}, pitch{}, velocity{}, channel{};
    std::uint64_t serial{};
    frame_t arrival_frame{}, not_before{};
    std::int64_t model_anchor{}; // end_target is relative to this for model durations.
    position_t arrival_quarter{}, start_target{}, end_target{};
    double start_quant{}, tightness{}, end_quant{}, duration{};
    int direction{};
    int due{}; // Slice-local deadline; Block::frames means not in this slice.
  };
  struct Input
  {
    int frame{};
    DecodedNote note{};
    std::size_t order{};
  };

public:
  static constexpr std::size_t max_voices = MaxVoices;
  static constexpr std::size_t max_inputs = MaxInputs;
  static constexpr std::size_t wire_notes = 16 * 128;
  // Two messages per new/carried voice, plus every locally owed wire release.
  static constexpr std::size_t max_output_events
      = 2 * MaxInputs + 2 * MaxVoices + wire_notes;

  void seed(std::uint64_t value) noexcept { random_.seed(value); }
  [[nodiscard]] const Statistics& statistics() const noexcept { return stats_; }
  [[nodiscard]] std::size_t active_count() const noexcept
  {
    return count(State::active);
  }
  [[nodiscard]] std::size_t pending_count() const noexcept
  {
    return count(State::pending);
  }
  [[nodiscard]] std::size_t deferred_release_count() const noexcept
  {
    return owed_offs_.count();
  }
  [[nodiscard]] bool needs_service() const noexcept
  {
    return active_count() || pending_count() || owed_offs_.any();
  }

  // NO port access here. Lifecycle, transport, and end-of-tick calls can safely
  // retire voices even if no output sample currently exists. Repeated calls
  // preserve the locally stored releases. Pending starts are discarded.
  void request_reset() noexcept
  {
    for(auto& v : voices_)
    {
      if(v.state == State::active)
        owe_off(v);
      v.state = State::free;
    }
    have_previous_ = false;
    ++stats_.resets;
  }

  void begin_input() noexcept
  {
    input_count_ = 0;
    overflow_ = false;
  }
  bool push(int frame, DecodedNote note) noexcept
  {
    if(input_count_ == MaxInputs)
    {
      overflow_ = true;
      return false;
    }
    inputs_[input_count_] = {frame, note, input_count_};
    ++input_count_;
    return true;
  }
  void input_overflow() noexcept { overflow_ = true; }

  // Service a real writable window without triggering or advancing any notes.
  // May be used by a host that gives stopped nodes a final cleanup tick.
  template <typename Sink>
  bool drain_releases(int frame, int frames, Sink&& sink) noexcept
  {
    if(frame < 0 || frame >= frames)
      return !owed_offs_.any();
    for(std::size_t k = 0; k < wire_notes; ++k)
    {
      if(!owed_offs_[k])
        continue;
      if(!send(
             MidiEvent{
                 frame, static_cast<std::uint8_t>(k / 128),
                 static_cast<std::uint8_t>(k % 128), 0, false},
             sink))
        return false;
      owed_offs_.reset(k);
    }
    return true;
  }

  template <typename Sink>
  void process(const Block& b, Settings settings, Sink&& sink) noexcept
  {
    // Inspect transport even when there is no writable frame. Never emit an
    // out-of-range message and never consume locally owed releases in this case.
    if(b.frames <= 0)
    {
      if(b.frames < 0 || b.discontinuity || b.end_discontinuity
         || b.model_begin != b.model_end || b.quarters_begin != b.quarters_end
         || changed_position(b))
        request_reset();
      begin_input();
      return;
    }
    if(!valid(b))
    {
      ++stats_.invalid_blocks;
      request_reset();
      drain_releases(0, b.frames, sink);
      begin_input();
      return;
    }
    const int md = direction(b.model_begin, b.model_end);
    const int qd = direction(b.quarters_begin, b.quarters_end);
    const bool reversed
        = have_previous_
          && ((md && previous_model_direction_ && md != previous_model_direction_)
              || (qd && previous_quarter_direction_
                  && qd != previous_quarter_direction_));
    if(b.discontinuity || changed_position(b) || reversed)
      request_reset();

    // Stationary scrubs audition no new triggers. Retire all previous voices.
    // A continuous reverse tick, unlike a stationary tick, is fully playable.
    if(md == 0)
      request_reset();
    if(!drain_releases(0, b.frames, sink))
    {
      begin_input();
      return; // Backpressure: do not permit a new on before its owed off.
    }
    if(md == 0)
    {
      begin_input();
      remember(b);
      return;
    }
    if(overflow_)
    {
      ++stats_.input_overflows;
      request_reset(); // A discarded input suffix could contain explicit offs.
      drain_releases(0, b.frames, sink);
      begin_input();
      remember(b);
      return;
    }

    settings = sanitize(settings);
    std::size_t kept = 0;
    for(std::size_t i = 0; i < input_count_; ++i)
      if(inputs_[i].frame >= 0 && inputs_[i].frame < b.frames
         && inputs_[i].note.pitch <= 127 && inputs_[i].note.velocity <= 127)
        inputs_[kept++] = inputs_[i];
      else
        ++stats_.rejected_inputs;
    input_count_ = kept;
    std::sort(
        inputs_.begin(), inputs_.begin() + input_count_,
        [](const Input& a, const Input& c) {
      return a.frame != c.frame ? a.frame < c.frame : a.order < c.order;
    });

    const Grid grid{b};
    const bool meter_changed = have_previous_
                               && (b.numerator != previous_numerator_
                                   || b.denominator != previous_denominator_
                                   || b.last_signature != previous_signature_);
    for(auto& v : voices_)
    {
      if(v.state == State::pending && meter_changed && v.start_quant > 0. && qd)
        set_start_target(v, grid, b, qd);
      if(v.state == State::active && v.end_kind == EndKind::grid && meter_changed && qd
         && qd * (v.end_target - b.quarters_begin) > 0.L)
        v.end_target = grid.next(b.quarters_begin, v.end_quant, qd);
      if(v.state != State::free)
        v.due = deadline(v, b, grid);
    }

    bool ok = true;
    std::size_t input = 0;
    while(ok)
    {
      int now = input < input_count_ ? inputs_[input].frame : b.frames;
      for(const auto& v : voices_)
        if(v.state != State::free)
          now = std::min(now, v.due);
      if(now >= b.frames)
        break;

      // Automatic offs, then timestamp-stable inputs, then surviving ons.
      for(auto& v : voices_)
        if(v.state == State::active && v.due == now)
          if(!release(v, now, sink))
          {
            ok = false;
            break;
          }
      if(!ok)
        break;
      while(input < input_count_ && inputs_[input].frame == now)
      {
        const auto note = inputs_[input++].note;
        if(note.velocity == 0)
          ok = cancel_source(note.pitch, now, sink);
        else
          enqueue(note, now, settings, b, grid);
        if(!ok)
          break;
      }
      if(!ok)
        break;

      // One voice per wire (channel,pitch); last arrival wins on equal samples.
      for(auto& v : voices_)
      {
        if(v.state != State::pending || v.due != now)
          continue;
        for(const auto& other : voices_)
          if(other.state == State::pending && other.due == now
             && other.channel == v.channel && other.pitch == v.pitch
             && other.serial > v.serial)
          {
            v.state = State::free;
            ++stats_.coalesced_starts;
            break;
          }
      }
      // Release collisions before ANY new on at this timestamp.
      for(const auto& p : voices_)
      {
        if(p.state != State::pending || p.due != now)
          continue;
        for(auto& a : voices_)
          if(a.state == State::active && a.channel == p.channel && a.pitch == p.pitch)
          {
            ok = release(a, now, sink);
            ++stats_.retriggers;
            if(!ok)
              break;
          }
        if(!ok)
          break;
      }
      if(!ok)
        break;
      for(;;)
      {
        Voice* next = nullptr;
        for(auto& v : voices_)
          if(v.state == State::pending && v.due == now
             && (!next || v.serial < next->serial))
            next = &v;
        if(!next)
          break;
        if(!activate(*next, now, b, grid, sink))
        {
          ok = false;
          break;
        }
      }
    }
    begin_input();
    if(!ok)
    {
      request_reset();
      return;
    }
    remember(b);
    // The next tick owns this boundary. Do not emit at frame == b.frames or
    // append a speculative future timestamp to the output port.
    if(b.end_discontinuity)
      request_reset();
  }

private:
  [[nodiscard]] std::size_t count(State state) const noexcept
  {
    return std::count_if(voices_.begin(), voices_.end(), [state](const Voice& v) {
      return v.state == state;
    });
  }
  void owe_off(const Voice& v) noexcept
  {
    owed_offs_.set(std::size_t(v.channel) * 128 + v.pitch);
  }
  template <typename Sink>
  bool send(const MidiEvent& e, Sink& sink) noexcept
  {
    if constexpr(std::is_void_v<std::invoke_result_t<Sink&, const MidiEvent&>>)
    {
      sink(e);
      return true;
    }
    else
    {
      if(sink(e))
        return true;
      ++stats_.output_failures;
      return false;
    }
  }
  static bool valid(const Block& b) noexcept
  {
    auto position = [](double q) { return std::isfinite(q) && std::abs(q) <= 1e9; };
    return b.first_frame >= -frame_limit && b.first_frame <= frame_limit - b.frames
           && position(b.quarters_begin) && position(b.quarters_end)
           && position(b.bar_begin) && position(b.bar_end) && position(b.last_signature)
           && b.numerator >= 0 && b.numerator <= 1024 && b.denominator >= 0
           && b.denominator <= 1024;
  }
  bool changed_position(const Block& b) const noexcept
  {
    return have_previous_
           && (b.first_frame != previous_frame_ || b.model_begin != previous_model_
               || std::abs(position_t(b.quarters_begin) - previous_quarter_) > 1e-8L);
  }
  static position_t at(position_t a, position_t z, int f, int frames) noexcept
  {
    return a + (z - a) * (position_t(f) / frames);
  }
  static position_t quarter_at(const Block& b, int f) noexcept
  {
    return at(b.quarters_begin, b.quarters_end, f, b.frames);
  }
  // Cast AFTER unsigned subtraction. This handles the complete int64 range
  // without signed overflow or losing small deltas at a large model origin on
  // platforms where long double has only double precision (notably MSVC).
  static position_t model_delta(std::int64_t to, std::int64_t from) noexcept
  {
    if(to >= from)
      return position_t(std::uint64_t(to) - std::uint64_t(from));
    return -position_t(std::uint64_t(from) - std::uint64_t(to));
  }
  static int
  frame_of(position_t a, position_t z, position_t target, int frames, bool ceil) noexcept
  {
    if(a == z)
      return frames;
    const auto x = clean_integer((target - a) / (z - a) * frames);
    if(!std::isfinite(x) || x >= frames)
      return frames;
    if(x <= 0.L)
      return 0;
    return static_cast<int>(ceil ? std::ceil(x) : std::floor(x));
  }
  static int local_frame(const Block& b, frame_t absolute) noexcept
  {
    if(absolute <= b.first_frame)
      return 0;
    if(absolute >= b.first_frame + b.frames)
      return b.frames;
    return static_cast<int>(absolute - b.first_frame);
  }
  static void
  set_start_target(Voice& v, const Grid& grid, const Block& b, int dir) noexcept
  {
    const auto from = dir > 0
                          ? std::max(v.arrival_quarter, position_t(b.quarters_begin))
                          : std::min(v.arrival_quarter, position_t(b.quarters_begin));
    v.start_target = onset_target(grid, from, v.start_quant, v.tightness, dir);
  }
  int deadline(const Voice& v, const Block& b, const Grid& grid) const noexcept
  {
    if(v.state == State::pending)
    {
      const int earliest = local_frame(b, v.arrival_frame);
      if(v.start_quant == 0. || v.tightness == 0.)
        return earliest;
      return std::max(
          earliest,
          frame_of(b.quarters_begin, b.quarters_end, v.start_target, b.frames, true));
    }
    const int earliest = local_frame(b, v.not_before);
    switch(v.end_kind)
    {
      case EndKind::sample:
        return earliest;
      case EndKind::model:
        return std::max(
            earliest, frame_of(
                          0.L, model_delta(b.model_end, b.model_begin),
                          model_delta(v.model_anchor, b.model_begin) + v.end_target,
                          b.frames, true));
      case EndKind::musical:
        return std::max(
            earliest,
            frame_of(b.quarters_begin, b.quarters_end, v.end_target, b.frames, true));
      case EndKind::grid:
        return std::max(
            earliest,
            frame_of(b.quarters_begin, b.quarters_end, v.end_target, b.frames, true));
      case EndKind::hold:
        return b.frames;
    }
    return b.frames;
  }
  template <typename Sink>
  bool release(Voice& v, int frame, Sink& sink) noexcept
  {
    const bool ok = send(MidiEvent{frame, v.channel, v.pitch, 0, false}, sink);
    if(!ok)
      owe_off(v);
    v.state = State::free;
    return ok;
  }
  template <typename Sink>
  bool cancel_source(std::uint8_t source, int frame, Sink& sink) noexcept
  {
    for(auto& v : voices_)
      if(v.state != State::free && v.source == source)
      {
        if(v.state == State::active)
        {
          if(!release(v, frame, sink))
            return false;
        }
        else
          v.state = State::free;
      }
    return true; // Unmatched releases never stop notes owned by other processors.
  }
  void enqueue(
      DecodedNote note, int frame, const Settings& s, const Block& b,
      const Grid& grid) noexcept
  {
    // A full pool rejects the trigger; it never silently evicts an unrelated
    // active note or forgets that note's eventual release.
    Voice* free = nullptr;
    for(auto& v : voices_)
    {
      if(v.state == State::free && !free)
        free = &v;
    }
    if(!free)
    {
      ++stats_.dropped_triggers;
      return;
    }
    auto& v = *free;
    v = {};
    v.state = State::pending;
    v.source = note.pitch;
    v.pitch = static_cast<std::uint8_t>(std::clamp(
        int(note.pitch) + s.pitch_shift
            + random_.between(
                s.pitch_direction == PitchDirection::Higher ? 0 : -s.pitch_random,
                s.pitch_direction == PitchDirection::Lower ? 0 : s.pitch_random),
        0, 127));
    v.velocity = static_cast<std::uint8_t>(
        std::clamp(int(note.velocity) + random_.symmetric(s.velocity_random), 1, 127));
    v.channel = static_cast<std::uint8_t>(s.channel - 1);
    v.serial = ++serial_;
    v.arrival_frame = b.first_frame + frame;
    v.arrival_quarter = quarter_at(b, frame);
    v.start_quant = s.start_quant;
    v.tightness = s.tightness;
    v.end_quant = s.end_quant;
    v.duration = s.duration;
    v.end_kind
        = s.end_mode == EndMode::duration
              ? (s.duration == 0.                                 ? EndKind::sample
                 : s.duration_unit == DurationUnit::model_seconds ? EndKind::model
                                                                  : EndKind::musical)
              : (s.end_quant > 0.   ? EndKind::grid
                 : s.end_quant < 0. ? EndKind::hold
                                    : EndKind::sample);
    const int qd = direction(b.quarters_begin, b.quarters_end);
    v.direction = qd ? qd : direction(b.model_begin, b.model_end);
    if(s.start_quant > 0.)
      set_start_target(v, grid, b, v.direction);
    v.due = deadline(v, b, grid);
  }
  template <typename Sink>
  bool
  activate(Voice& v, int frame, const Block& b, const Grid& grid, Sink& sink) noexcept
  {
    if(!send(MidiEvent{frame, v.channel, v.pitch, v.velocity, true}, sink))
    {
      v.state = State::free;
      return false;
    }
    v.state = State::active;
    v.not_before = b.first_frame + frame + 1;
    if(v.end_kind == EndKind::model)
    {
      v.model_anchor = b.model_begin;
      v.end_target
          = model_delta(b.model_end, b.model_begin) * (position_t(frame) / b.frames)
            + direction(b.model_begin, b.model_end) * v.duration * flicks_per_second;
    }
    else if(v.end_kind == EndKind::musical)
      v.end_target = quarter_at(b, frame) + v.direction * v.duration;
    else if(v.end_kind == EndKind::grid)
      v.end_target = grid.next(quarter_at(b, frame), v.end_quant, v.direction, true);
    v.due = deadline(v, b, grid);
    return true;
  }
  void remember(const Block& b) noexcept
  {
    previous_frame_ = b.first_frame + b.frames;
    previous_model_ = b.model_end;
    previous_quarter_ = b.quarters_end;
    previous_model_direction_ = direction(b.model_begin, b.model_end);
    previous_quarter_direction_ = direction(b.quarters_begin, b.quarters_end);
    previous_numerator_ = b.numerator;
    previous_denominator_ = b.denominator;
    previous_signature_ = b.last_signature;
    have_previous_ = true;
  }

  std::array<Voice, MaxVoices> voices_{};
  std::array<Input, MaxInputs> inputs_{};
  // Owed MIDI bytes are exactly determined by this key; release velocity is 0.
  // This is LOCAL STORAGE, not a retained output-port buffer or reference.
  std::bitset<wire_notes> owed_offs_{};
  std::size_t input_count_{};
  std::uint64_t serial_{};
  Random random_{};
  Statistics stats_{};
  frame_t previous_frame_{};
  std::int64_t previous_model_{};
  double previous_quarter_{}, previous_signature_{};
  int previous_model_direction_{}, previous_quarter_direction_{};
  int previous_numerator_{}, previous_denominator_{};
  bool have_previous_{}, overflow_{};
};
} // namespace Nodes::PulseToNote::detail
