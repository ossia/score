#pragma once
#include <algorithm>
#include <cstdint>
#include <memory>

namespace Media
{
// Remove the compile time overhead of std::pair for this
struct FloatPair
{
  float first, second;
};

//! Per-channel minimum and maximum of the samples in fixed buckets of frames.
//!
//! Drawing a waveform zoomed out far enough that one pixel column spans
//! thousands of samples otherwise costs a full pass over the file per redraw,
//! whatever the width of the window: the work is proportional to the length of
//! the sound, not to the number of columns. Reducing whole buckets instead
//! divides that by the bucket size.
//!
//! Indexed in source frames -- the same coordinates AudioFile::ViewHandle
//! takes -- so loops, start offsets, tempo and the device pixel ratio never
//! enter into it: a range has passed through all of those before it gets here.
//!
//! Built once, when the file has finished decoding, and never written again.
//! It is passed around as a shared_ptr<const WaveformSummary> and read from the
//! waveform threads without synchronisation of its own; an earlier attempt at
//! this grew its table while readers were walking it, and that is what made it
//! unreliable.
struct WaveformSummary
{
  //! Frames per bucket. A power of two, at least 64.
  int64_t bucket{};

  //! Buckets per channel. The last one may cover fewer than `bucket` frames.
  int64_t bucketCount{};

  int32_t channels{};

  //! Frames the summary accounts for.
  int64_t frames{};

  //! channels * bucketCount pairs, channel-major.
  std::unique_ptr<FloatPair[]> data;

  const FloatPair* channel(int c) const noexcept
  {
    return data.get() + c * bucketCount;
  }

  //! Bucket size keeping the table within a fixed budget whatever the length of
  //! the sound. Short sounds get the finest bucket, which is what decides the
  //! zoom at which the summary starts paying: it only helps once a pixel column
  //! spans several buckets, so 64 frames means it engages from roughly 256
  //! samples per pixel -- well inside the range one actually works at. Long
  //! sounds trade that back for memory: an hour of stereo lands on 128 frames
  //! and 20 MB, ten hours on 1024 and 25 MB.
  static int64_t bucketFor(int64_t frames, int32_t channels) noexcept
  {
    constexpr int64_t max_bytes = 32ll * 1024 * 1024;
    constexpr int64_t max_bucket = 1ll << 20;
    const int64_t max_buckets
        = max_bytes / (int64_t(sizeof(FloatPair)) * std::max(1, int(channels)));

    int64_t b = 64;
    while(b < max_bucket && frames / b > max_buckets)
      b *= 2;
    return b;
  }
};
}
