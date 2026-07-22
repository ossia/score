#include "AvStreamIO.hpp"

#if SCORE_HAS_LIBAV
#include <ossia/detail/libav.hpp>

#include <QFile>
#include <QString>

namespace Media
{
static constexpr int av_io_buffer_size = 1 << 18;

static int av_io_read(void* opaque, uint8_t* out, int size)
{
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
  switch(whence & ~AVSEEK_FORCE)
  {
    case AVSEEK_SIZE:
      return dev->size();
    case SEEK_SET:
      return dev->seek(offset) ? offset : -1;
    case SEEK_CUR:
    {
      const qint64 abs = dev->pos() + offset;
      return dev->seek(abs) ? abs : -1;
    }
    case SEEK_END:
    {
      const qint64 abs = dev->size() + offset;
      return dev->seek(abs) ? abs : -1;
    }
    default:
      return -1;
  }
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
  auto f = std::make_unique<QFile>(path);
  if(!f->open(QIODevice::ReadOnly))
    return;

  auto* buffer = static_cast<unsigned char*>(av_malloc(av_io_buffer_size));
  if(!buffer)
    return;

  avio = avio_alloc_context(
      buffer, av_io_buffer_size, 0, f.get(), &av_io_read, nullptr, &av_io_seek);
  if(!avio)
  {
    av_free(buffer);
    return;
  }
  file = std::move(f);
}

AvIoDevice::~AvIoDevice()
{
  if(avio)
  {
    av_freep(&avio->buffer);
    avio_context_free(&avio);
  }
}

AvIoDevice::AvIoDevice(AvIoDevice&& other) noexcept
    : avio{other.avio}
    , file{std::move(other.file)}
{
  other.avio = nullptr;
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
    avio = other.avio;
    file = std::move(other.file);
    other.avio = nullptr;
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
