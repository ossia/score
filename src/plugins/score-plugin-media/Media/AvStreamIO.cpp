#include "AvStreamIO.hpp"

#if SCORE_HAS_LIBAV
#include <ossia/detail/libav.hpp>

#include <QFile>
#include <QString>

#if defined(__EMSCRIPTEN__)
#include <emscripten/proxying.h>
#include <emscripten/threading.h>
#endif

#include <memory>
#include <type_traits>

namespace Media
{
static constexpr int av_io_buffer_size = 1 << 18;

#if defined(__EMSCRIPTEN__)
// A weblocalfile: QFile is backed by a JS File/Blob whose emscripten::val is
// bound to the thread that created it; using it from another thread is
// undefined behaviour (it dereferences an invalid handle, crashing on
// Blob.size). Marshal the non-suspending QFile operations (open, seek, size,
// close) to the main runtime thread so the val is only ever touched by its
// owner. Reads are handled separately in av_io_read (they suspend via JSPI,
// which a proxied job cannot do).
template <typename F>
static void run_on_main(F&& f) noexcept
{
  if(emscripten_is_main_runtime_thread())
  {
    f();
    return;
  }
  emscripten_proxy_sync(
      emscripten_proxy_get_system_queue(), emscripten_main_runtime_thread_id(),
      [](void* p) { (*static_cast<std::remove_reference_t<F>*>(p))(); }, &f);
}
#else
template <typename F>
static void run_on_main(F&& f) noexcept
{
  f();
}
#endif

static int av_io_read(void* opaque, uint8_t* out, int size)
{
#if defined(__EMSCRIPTEN__)
  // Reading the backing Blob suspends via JSPI, which is only possible on the
  // main thread's promising call stack (a proxied job is not one). A worker
  // therefore cannot stream this source; fail cleanly so the off-thread decode
  // aborts instead of hitting an uncatchable SuspendError. Worker-thread
  // streaming needs the async-proxy reader documented in WASM-B2-ANALYSIS.md.
  if(!emscripten_is_main_runtime_thread())
    return AVERROR(EIO);
#endif
  auto* dev = static_cast<QFile*>(opaque);
  const qint64 r = dev->read(reinterpret_cast<char*>(out), size);
  if(r > 0)
    return static_cast<int>(r);
  if(r == 0)
    return AVERROR_EOF;
  return AVERROR(EIO);
}

static int64_t av_io_seek(void* opaque, int64_t offset, int whence)
{
  auto* dev = static_cast<QFile*>(opaque);
  int64_t result = -1;
  run_on_main([&] {
    switch(whence & ~AVSEEK_FORCE)
    {
      case AVSEEK_SIZE:
        result = dev->size();
        break;
      case SEEK_SET:
        result = dev->seek(offset) ? offset : -1;
        break;
      case SEEK_CUR:
      {
        const qint64 abs = dev->pos() + offset;
        result = dev->seek(abs) ? abs : -1;
        break;
      }
      case SEEK_END:
      {
        const qint64 abs = dev->size() + offset;
        result = dev->seek(abs) ? abs : -1;
        break;
      }
    }
  });
  return result;
}

bool isStreamedMediaPath(const QString& path) noexcept
{
  return path.startsWith(QLatin1String("weblocalfile:"));
}

bool isStreamedMediaPath(const std::string& path) noexcept
{
  return path.rfind("weblocalfile:", 0) == 0;
}

AvIoDevice::AvIoDevice(const QString& path)
{
  QFile* f = nullptr;
  run_on_main([&] {
    f = new QFile(path);
    if(!f->open(QIODevice::ReadOnly))
    {
      delete f;
      f = nullptr;
    }
  });
  if(!f)
    return;

  auto* buffer = static_cast<unsigned char*>(av_malloc(av_io_buffer_size));
  if(!buffer)
  {
    run_on_main([&] { delete f; });
    return;
  }

  avio = avio_alloc_context(
      buffer, av_io_buffer_size, 0, f, &av_io_read, nullptr, &av_io_seek);
  if(!avio)
  {
    av_free(buffer);
    run_on_main([&] { delete f; });
    return;
  }
  file = f;
}

AvIoDevice::~AvIoDevice()
{
  if(avio)
  {
    av_freep(&avio->buffer);
    avio_context_free(&avio);
  }
  if(file)
  {
    QFile* f = file;
    run_on_main([&] { delete f; });
    file = nullptr;
  }
}

AvIoDevice::AvIoDevice(AvIoDevice&& other) noexcept
    : avio{other.avio}
    , file{other.file}
{
  other.avio = nullptr;
  other.file = nullptr;
}

AvIoDevice& AvIoDevice::operator=(AvIoDevice&& other) noexcept
{
  if(this != &other)
  {
    if(avio)
    {
      av_freep(&avio->buffer);
      avio_context_free(&avio);
    }
    if(file)
    {
      QFile* f = file;
      run_on_main([&] { delete f; });
    }
    avio = other.avio;
    file = other.file;
    other.avio = nullptr;
    other.file = nullptr;
  }
  return *this;
}

bool open_input_custom_io(
    AVFormatContext*& format, AvIoDevice& io, const QString& path) noexcept
{
  io = AvIoDevice{path};
  if(!io)
    return false;

  AVFormatContext* fmt = avformat_alloc_context();
  if(!fmt)
  {
    io = {};
    return false;
  }

  fmt->pb = io.avio;
  fmt->flags |= AVFMT_FLAG_CUSTOM_IO;

  if(avformat_open_input(&fmt, nullptr, nullptr, nullptr) != 0)
  {
    io = {};
    return false;
  }

  format = fmt;
  return true;
}

// libossia's libav_handle is Qt-agnostic; it delegates the browser-only Blob
// streaming back here through a hook so the submodule keeps no Qt dependency.
static AVFormatContext* libav_stream_open_hook(
    const char* path, void*& io_owner, void (*&io_free)(void*)) noexcept
{
  const QString qpath = QString::fromUtf8(path);
  if(!isStreamedMediaPath(qpath))
    return nullptr;

  auto io = std::make_unique<AvIoDevice>();
  AVFormatContext* fmt{};
  if(!open_input_custom_io(fmt, *io, qpath))
    return nullptr;

  io_owner = io.release();
  io_free = [](void* p) { delete static_cast<AvIoDevice*>(p); };
  return fmt;
}

static const bool registered_libav_hook = [] {
  ossia::libav_custom_open() = &libav_stream_open_hook;
  return true;
}();
}
#endif
