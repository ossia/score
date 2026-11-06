#pragma once

#include <ossia/network/value/value.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
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
                return std::isfinite(value) && value >= -1.f && value <= 1.f;
              });
  }

  bool operator==(const WavetableData&) const = default;
};

//! Canonical control value: a list of frames, each a list of bipolar samples.
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
      float amplitude;
      if(const auto* number = sample.target<float>())
        amplitude = *number;
      else if(const auto* number = sample.target<int>())
        amplitude = *number;
      else
        return std::nullopt;
      if(!std::isfinite(amplitude) || amplitude < -1.f || amplitude > 1.f)
        return std::nullopt;
      result.samples.push_back(amplitude);
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
inline std::optional<float> waveformSample(const ossia::value& sample) noexcept
{
  float amplitude;
  if(const auto* number = sample.target<float>())
    amplitude = *number;
  else if(const auto* number = sample.target<int>())
    amplitude = float(*number);
  else if(const auto* flag = sample.target<bool>())
    amplitude = *flag ? 1.f : 0.f;
  else
    return std::nullopt;
  if(!std::isfinite(amplitude))
    return std::nullopt;
  return std::clamp(amplitude, -1.f, 1.f);
}

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
      result.samples.push_back(*detail::waveformSample(sample));
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

//! Periodic linear interpolation. The caller supplies a validated table,
//! existing frame and finite phase. No preparation or allocation occurs here.
inline float sampleWavetable(
    const WavetableData& table, std::size_t frame, double phase) noexcept
{
  assert(table.frameSize >= 2 && frame < table.frameCount() && std::isfinite(phase));
  phase -= std::floor(phase);
  const double position = phase * table.frameSize;
  const auto left = std::min(std::size_t(position), table.frameSize - 1);
  const auto right = (left + 1) % table.frameSize;
  const auto samples = table.frame(frame);
  return float(samples[left] + (samples[right] - samples[left]) * (position - left));
}

namespace detail
{
template <typename T, std::size_t N, typename Sample>
bool resampleWaveform(
    std::size_t size, Sample&& sample, std::array<T, N>& destination, double low,
    double high) noexcept
{
  static_assert(N > 0 && std::is_arithmetic_v<T>);
  if(size < 2 || !std::isfinite(low) || !std::isfinite(high) || low >= high
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
      [samples](std::size_t i) { return *detail::waveformSample((*samples)[i]); },
      destination, low, high);
}
}
