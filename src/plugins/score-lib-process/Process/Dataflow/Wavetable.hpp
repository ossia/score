#pragma once

#include <ossia/network/value/value.hpp>
#include <ossia/math/safe_math.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace Process
{
//! Periodic samples, independent of a synthesizer's playback caches.
//! Contiguous frames of equal length (2 to maxFrameSize samples, at most
//! maxFrames frames and maxSamples samples in all), each holding distinct
//! phase samples: the closing endpoint is implicit, the frame wraps. Amplitude
//! is bipolar [-1, 1]. This form allocates and belongs to the UI thread; the
//! execution side reads the port value in place, see prepareWaveform.
struct WavetableData
{
  static constexpr std::size_t maxFrameSize = 65536;
  static constexpr std::size_t maxFrames = 256;
  static constexpr std::size_t maxSamples = 1048576;

  std::size_t frameSize{};
  std::vector<float> samples;

  std::size_t frameCount() const noexcept
  {
    return frameSize ? samples.size() / frameSize : 0;
  }

  std::span<const float> frame(std::size_t index) const noexcept
  {
    assert(index < frameCount());
    return {samples.data() + index * frameSize, frameSize};
  }

  bool valid() const noexcept
  {
    return frameSize >= 2 && frameSize <= maxFrameSize
           && !samples.empty() && samples.size() <= maxSamples
           && samples.size() % frameSize == 0 && frameCount() <= maxFrames
           && std::all_of(samples.begin(), samples.end(), [](float value) {
                return ossia::safe_isfinite(value) && value >= -1.f && value <= 1.f;
              });
  }

  bool operator==(const WavetableData&) const = default;
};

//! A number as waveform values hold one: a float, an int or a bool. Empty for
//! anything else and for a value that is not finite.
inline std::optional<float> waveformNumber(const ossia::value& value) noexcept
{
  float number;
  if(const auto* real = value.target<float>())
    number = *real;
  else if(const auto* integer = value.target<int>())
    number = float(*integer);
  else if(const auto* flag = value.target<bool>())
    number = *flag ? 1.f : 0.f;
  else
    return std::nullopt;
  if(!ossia::safe_isfinite(number))
    return std::nullopt;
  return number;
}

//! A sample of a waveform value: a waveformNumber clamped to [-1, 1].
inline std::optional<float> waveformSample(const ossia::value& value) noexcept
{
  if(const auto number = waveformNumber(value))
    return std::clamp(*number, -1.f, 1.f);
  return std::nullopt;
}

//! Canonical control value: a list of frames, each a list of bipolar samples.
//! Stricter than waveformFromValue: a flat list or a sample outside [-1, 1]
//! is not canonical.
inline std::optional<WavetableData> wavetableFromValue(const ossia::value& value)
{
  const auto* frames = value.target<std::vector<ossia::value>>();
  if(!frames || frames->empty() || frames->size() > WavetableData::maxFrames)
    return std::nullopt;
  const auto* first = frames->front().target<std::vector<ossia::value>>();
  if(!first || first->size() < 2 || first->size() > WavetableData::maxFrameSize
     || frames->size() > WavetableData::maxSamples / first->size())
    return std::nullopt;

  WavetableData result;
  result.frameSize = first->size();
  result.samples.reserve(result.frameSize * frames->size());
  for(const auto& frameValue : *frames)
  {
    const auto* frame = frameValue.target<std::vector<ossia::value>>();
    if(!frame || frame->size() != result.frameSize)
      return std::nullopt;
    for(const auto& sample : *frame)
    {
      const auto amplitude = waveformNumber(sample);
      if(!amplitude || *amplitude < -1.f || *amplitude > 1.f)
        return std::nullopt;
      result.samples.push_back(*amplitude);
    }
  }
  return result;
}

inline ossia::value wavetableToValue(const WavetableData& table)
{
  if(!table.valid())
    throw std::invalid_argument("Invalid periodic sample table");
  std::vector<ossia::value> frames;
  frames.reserve(table.frameCount());
  for(std::size_t index = 0; index < table.frameCount(); ++index)
  {
    std::vector<ossia::value> samples;
    samples.reserve(table.frameSize);
    for(float sample : table.frame(index))
      samples.emplace_back(sample);
    frames.emplace_back(std::move(samples));
  }
  return frames;
}

namespace detail
{
inline bool isWaveformFrame(std::span<const ossia::value> samples) noexcept
{
  return samples.size() >= 2 && samples.size() <= WavetableData::maxFrameSize
         && std::all_of(samples.begin(), samples.end(), [](const ossia::value& v) {
              return bool(waveformSample(v));
            });
}
}

//! One frame of a waveform value as it arrives through a cable or an address,
//! read in place without allocating. A list of numbers is a single frame, as a
//! multislider or an OSC message with several arguments produce; a list of
//! number lists is a bank, as the canonical value is. Samples are clamped to
//! [-1, 1]; non-numeric or non-finite samples and frames shorter than two
//! samples are rejected.
inline std::optional<std::span<const ossia::value>>
waveformFrame(const ossia::value& value, std::size_t frame = 0) noexcept
{
  const auto* list = value.target<std::vector<ossia::value>>();
  if(!list || list->empty())
    return std::nullopt;
  std::span<const ossia::value> samples;
  if(const auto* nested = list->front().target<std::vector<ossia::value>>(); nested)
  {
    if(frame >= list->size())
      return std::nullopt;
    const auto* selected = (*list)[frame].target<std::vector<ossia::value>>();
    if(!selected)
      return std::nullopt;
    samples = *selected;
  }
  else
  {
    if(frame != 0)
      return std::nullopt;
    samples = *list;
  }
  if(!detail::isWaveformFrame(samples))
    return std::nullopt;
  return samples;
}

//! Every frame of a value accepted by waveformFrame, for display on the UI
//! thread. Unlike wavetableFromValue it takes a flat list as one frame and
//! clamps the samples; a bank needs frames of equal size.
inline std::optional<WavetableData> waveformFromValue(const ossia::value& value)
{
  const auto* list = value.target<std::vector<ossia::value>>();
  if(!list || list->empty())
    return std::nullopt;
  const bool bank = bool(list->front().target<std::vector<ossia::value>>());
  const std::size_t frames = bank ? list->size() : 1;
  if(frames > WavetableData::maxFrames)
    return std::nullopt;
  WavetableData result;
  for(std::size_t index = 0; index < frames; ++index)
  {
    const auto samples = waveformFrame(value, index);
    if(!samples || (index > 0 && samples->size() != result.frameSize)
       || frames > WavetableData::maxSamples / samples->size())
      return std::nullopt;
    result.frameSize = samples->size();
    for(const auto& sample : *samples)
      result.samples.push_back(*waveformSample(sample));
  }
  return result;
}

inline WavetableData makeTriangleWavetable(std::size_t frameSize = 32)
{
  if(frameSize < 2 || frameSize > WavetableData::maxFrameSize)
    throw std::invalid_argument("Invalid waveform resolution");
  WavetableData table{frameSize, std::vector<float>(frameSize)};
  for(std::size_t sample = 0; sample < frameSize; ++sample)
  {
    const double phase = double(sample) / frameSize;
    table.samples[sample] = float(1. - 4. * std::abs(phase - .5));
  }
  return table;
}

//! Rounds a bipolar amplitude to the nearest of `levels` evenly spaced values
//! spanning [-1, 1]. Zero levels means continuous: the value is only clamped.
inline float quantizeSample(double value, int levels) noexcept
{
  value = ossia::safe_isfinite(value) ? std::clamp(value, -1., 1.) : 0.;
  if(levels >= 2)
    value = 2. * std::round((value + 1.) * .5 * (levels - 1)) / (levels - 1) - 1.;
  return std::clamp(float(value), -1.f, 1.f);
}

//! Index of the amplitude level a quantized sample sits on, from 0 (-1) to
//! levels - 1 (+1).
inline int sampleLevel(float value, int levels) noexcept
{
  assert(levels >= 2);
  return int(std::lround((std::clamp(value, -1.f, 1.f) + 1.) * .5 * (levels - 1)));
}

enum class WaveformShape
{
  Sine,
  Triangle,
  SawUp,
  SawDown,
  Square,
  Pulse25,
  Pulse12,
  Noise
};

//! Fills one frame with a periodic shape, quantized to `levels`. Noise is a
//! deterministic function of `seed`.
inline void generateWaveform(
    std::span<float> frame, WaveformShape shape, int levels, std::uint32_t seed = 1) noexcept
{
  const std::size_t size = frame.size();
  if(size == 0)
    return;
  std::uint32_t state = seed ? seed : 0x9E3779B9u;
  for(std::size_t i = 0; i < size; ++i)
  {
    const double phase = double(i) / size;
    const double ramp = size > 1 ? double(i) / (size - 1) : 0.;
    double value{};
    switch(shape)
    {
      case WaveformShape::Sine:
        value = std::sin(2. * 3.14159265358979323846 * phase);
        break;
      case WaveformShape::Triangle:
        value = 1. - 4. * std::abs(phase - .5);
        break;
      case WaveformShape::SawUp:
        value = 2. * ramp - 1.;
        break;
      case WaveformShape::SawDown:
        value = 1. - 2. * ramp;
        break;
      case WaveformShape::Square:
        value = phase < .5 ? 1. : -1.;
        break;
      case WaveformShape::Pulse25:
        value = phase < .25 ? 1. : -1.;
        break;
      case WaveformShape::Pulse12:
        value = phase < .125 ? 1. : -1.;
        break;
      case WaveformShape::Noise:
        // xorshift32: the same seed always draws the same frame.
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        value = 2. * (double(state) / 4294967295.) - 1.;
        break;
    }
    frame[i] = quantizeSample(value, levels);
  }
}

//! Scales the frame around zero so that its largest magnitude reaches 1. A
//! silent frame is left as it is.
inline void normalizeWaveform(std::span<float> frame, int levels) noexcept
{
  float peak{};
  for(float sample : frame)
    peak = std::max(peak, std::abs(sample));
  if(peak <= 0.f)
    return;
  for(float& sample : frame)
    sample = quantizeSample(double(sample) / peak, levels);
}

inline void invertWaveform(std::span<float> frame, int levels) noexcept
{
  for(float& sample : frame)
    sample = quantizeSample(-double(sample), levels);
}

inline void reverseWaveform(std::span<float> frame) noexcept
{
  std::reverse(frame.begin(), frame.end());
}

//! Moves every sample one position earlier (left) or later, wrapping.
inline void rotateWaveform(std::span<float> frame, bool left) noexcept
{
  if(frame.size() < 2)
    return;
  if(left)
    std::rotate(frame.begin(), frame.begin() + 1, frame.end());
  else
    std::rotate(frame.rbegin(), frame.rbegin() + 1, frame.rend());
}

//! One pass of a periodic [1 2 1] / 4 low-pass.
inline void smoothWaveform(std::span<float> frame, int levels)
{
  const std::size_t size = frame.size();
  if(size < 3)
    return;
  const std::vector<float> source(frame.begin(), frame.end());
  for(std::size_t i = 0; i < size; ++i)
  {
    const double previous = source[(i + size - 1) % size];
    const double next = source[(i + 1) % size];
    frame[i] = quantizeSample((previous + 2. * source[i] + next) * .25, levels);
  }
}

//! Sets every sample to the level nearest to zero.
inline void clearWaveform(std::span<float> frame, int levels) noexcept
{
  std::fill(frame.begin(), frame.end(), quantizeSample(0., levels));
}

//! Periodic linear interpolation. The caller supplies a validated table,
//! existing frame and finite phase. No preparation or allocation occurs here.
inline float sampleWavetable(
    const WavetableData& table, std::size_t frame, double phase) noexcept
{
  assert(table.frameSize >= 2 && frame < table.frameCount() && ossia::safe_isfinite(phase));
  phase -= std::floor(phase);
  const double position = phase * table.frameSize;
  const auto left = std::min(std::size_t(position), table.frameSize - 1);
  const auto right = (left + 1) % table.frameSize;
  const auto samples = table.frame(frame);
  return float(samples[left] + (samples[right] - samples[left]) * (position - left));
}

//! Every frame of a valid table resampled to `size` samples by periodic
//! linear interpolation.
inline WavetableData resampleWavetable(const WavetableData& source, std::size_t size)
{
  WavetableData result{size, std::vector<float>(source.frameCount() * size)};
  for(std::size_t frame = 0; frame < source.frameCount(); ++frame)
    for(std::size_t sample = 0; sample < size; ++sample)
      result.samples[frame * size + sample]
          = sampleWavetable(source, frame, double(sample) / size);
  return result;
}

namespace detail
{
template <typename T, std::size_t N, typename Sample>
bool resampleWaveform(
    std::size_t size, Sample&& sample, std::array<T, N>& destination, double low,
    double high) noexcept
{
  static_assert(N > 0 && std::is_arithmetic_v<T>);
  if(size < 2 || !ossia::safe_isfinite(low) || !ossia::safe_isfinite(high) || low >= high
     || low < double(std::numeric_limits<T>::lowest())
     || high > double(std::numeric_limits<T>::max()))
    return false;
  std::array<T, N> prepared;
  for(std::size_t index = 0; index < N; ++index)
  {
    const double position = (double(index) / N) * size;
    const auto left = std::min(std::size_t(position), size - 1);
    const auto right = (left + 1) % size;
    const float a = sample(left), b = sample(right);
    const double interpolated = float(a + (b - a) * (position - left));
    const double mapped = low + (high - low) * ((interpolated + 1.) * .5);
    if constexpr(std::is_integral_v<T>)
      prepared[index] = T(std::round(mapped));
    else
      prepared[index] = T(mapped);
  }
  destination = prepared;
  return true;
}
}

//! Periodic resampling into a fixed-size playback representation, mapping
//! [-1, 1] onto [low, high]. Invalid input leaves destination untouched;
//! integral chips round once, after resampling. Engines own their prepared
//! arrays, not this model. Neither overload allocates.
template <typename T, std::size_t N>
bool prepareWaveform(
    const WavetableData& table, std::array<T, N>& destination,
    double low, double high, std::size_t frame = 0) noexcept
{
  if(!table.valid() || frame >= table.frameCount())
    return false;
  const auto samples = table.frame(frame);
  return detail::resampleWaveform(
      samples.size(), [samples](std::size_t i) { return samples[i]; }, destination, low,
      high);
}

//! The same from a port value as execution delivers it, read in place: this
//! one is safe on the audio thread. See waveformFrame for the accepted shapes.
template <typename T, std::size_t N>
bool prepareWaveform(
    const ossia::value& value, std::array<T, N>& destination, double low,
    double high, std::size_t frame = 0) noexcept
{
  const auto samples = waveformFrame(value, frame);
  if(!samples)
    return false;
  return detail::resampleWaveform(
      samples->size(),
      [samples](std::size_t i) { return *waveformSample((*samples)[i]); },
      destination, low, high);
}
}
