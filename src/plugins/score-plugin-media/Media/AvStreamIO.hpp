#pragma once
#include <Media/Libav.hpp>

#if SCORE_HAS_LIBAV
extern "C" {
#include <libavformat/avformat.h>
}

#include <score_plugin_media_export.h>

#include <string>

class QString;

namespace Media
{
struct AvStreamState;

// A path that must be streamed on demand rather than opened with fopen/avio's
// default file protocol. On the wasm build these are the "weblocalfile:/N/name"
// URLs Qt assigns to browser File objects: fopen cannot open them, but the
// backing Blob can be read in byte-ranges, so a multi-GB file never has to be
// copied into MEMFS/RAM.
SCORE_PLUGIN_MEDIA_EXPORT
bool isStreamedMediaPath(const QString& path) noexcept;
SCORE_PLUGIN_MEDIA_EXPORT
bool isStreamedMediaPath(const std::string& path) noexcept;

// Owns the AVIOContext that streams a browser Blob and the position bookkeeping
// it reads through. Must outlive the AVFormatContext it is attached to (see
// open_input_custom_io).
struct SCORE_PLUGIN_MEDIA_EXPORT AvIoDevice
{
  AvIoDevice() = default;
  explicit AvIoDevice(const QString& path);
  ~AvIoDevice();

  AvIoDevice(AvIoDevice&&) noexcept;
  AvIoDevice& operator=(AvIoDevice&&) noexcept;
  AvIoDevice(const AvIoDevice&) = delete;
  AvIoDevice& operator=(const AvIoDevice&) = delete;

  explicit operator bool() const noexcept { return avio != nullptr; }

  AVIOContext* avio{};
  AvStreamState* state{};
};

// Allocates `format` and opens `path` through a custom AVIOContext backed by a
// QFile. On success `format` is set and `io` owns the streaming resources; the
// caller must destroy `format` (avformat_close_input) before `io`.
// Returns false and leaves `format` null on failure.
SCORE_PLUGIN_MEDIA_EXPORT
bool open_input_custom_io(
    AVFormatContext*& format, AvIoDevice& io, const QString& path) noexcept;
}
#endif
