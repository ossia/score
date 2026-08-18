#pragma once

/**
 * @file GStreamerAudioBuffer.hpp
 * @brief Lock-free bridge between GStreamer's chunk size and the engine tick.
 *
 * Lives in a header rather than in GStreamerDevice.cpp because the resize /
 * span re-point invariant it carries is an audio-thread lifetime rule, and a
 * rule nothing can exercise is a rule nothing keeps.
 */

#include <ossia/detail/pod_vector.hpp>
#include <ossia/detail/small_vector.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <span>
#include <vector>

namespace Gfx::GStreamer
{

// Audio: GStreamer delivers large chunks (e.g. 1024 samples).
// The audio engine reads small chunks (e.g. 64 samples).
// We use a lock-free ring buffer to bridge the two.
struct AudioBuffer
{
  int sample_rate{48000};
  int num_channels{2};

  // Ring buffer per channel, written by GStreamer thread, read by audio engine
  static constexpr std::size_t ring_size = 65536;

  // Max block the audio thread may resize the output storage to; the
  // parameter reserves this up front so the per-tick resize never reallocates
  // (a realloc would free a buffer the audio thread is reading through).
  static constexpr std::size_t max_block = 1 << 15;
  std::vector<std::vector<float>> ring; // [channel][ring_size]
  std::atomic<std::size_t> write_pos{0};
  std::atomic<std::size_t> read_pos{0};

  // Backing storage for audio spans — audio engine reads from here
  std::vector<ossia::float_vector>* output_data{};

  void init(int nchannels)
  {
    num_channels = nchannels;
    ring.resize(nchannels);
    for(auto& ch : ring)
      ch.resize(ring_size, 0.f);
  }

  // Called by GStreamer thread: write deinterleaved samples into ring
  void write(const float* interleaved, int num_samples, int channels)
  {
    int nch = std::min(channels, num_channels);
    auto wp = write_pos.load(std::memory_order_relaxed);
    for(int s = 0; s < num_samples; s++)
    {
      for(int ch = 0; ch < nch; ch++)
        ring[ch][(wp + s) % ring_size] = interleaved[s * channels + ch];
    }
    write_pos.store(wp + num_samples, std::memory_order_release);
  }

  // Points at the parameter's audio spans so read_into_output can re-point
  // them after a resize. A raw pointer (not a std::function) so that clearing
  // or using it during teardown can never throw on the audio thread.
  ossia::small_vector<std::span<float>, 8>* output_spans{};

  // Called by audio engine (indirectly): copy from ring into output spans
  void read_into_output(int block_size)
  {
    if(!output_data)
      return;

    // The engine tick size can differ from the configured buffer size
    // (e.g. PipeWire dynamic quantum); the storage follows it, but never
    // beyond the capacity reserved at construction (so no reallocation).
    if(block_size > (int)max_block)
      block_size = max_block;
    bool resized = false;
    for(auto& v : *output_data)
    {
      if(std::ssize(v) != block_size)
      {
        v.resize(block_size);
        resized = true;
      }
    }
    if(resized && output_spans && output_data)
    {
      const std::size_t n = std::min(output_spans->size(), output_data->size());
      for(std::size_t i = 0; i < n; i++)
        (*output_spans)[i] = (*output_data)[i];
    }

    auto rp = read_pos.load(std::memory_order_relaxed);
    auto wp = write_pos.load(std::memory_order_acquire);

    // How many samples are available?
    std::size_t available = (wp >= rp) ? (wp - rp) : 0;

    int nch = std::min((int)output_data->size(), num_channels);
    if(available >= (std::size_t)block_size)
    {
      // Copy block_size samples from ring to output
      for(int ch = 0; ch < nch; ch++)
      {
        auto& dst = (*output_data)[ch];
        auto& src = ring[ch];
        for(int s = 0; s < block_size; s++)
          dst[s] = src[(rp + s) % ring_size];
      }
      read_pos.store(rp + block_size, std::memory_order_release);
    }
    else
    {
      // Underrun: output silence
      for(int ch = 0; ch < nch; ch++)
        std::fill_n((*output_data)[ch].data(), block_size, 0.f);
    }
  }
};

}
