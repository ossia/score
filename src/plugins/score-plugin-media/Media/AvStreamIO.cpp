#include "AvStreamIO.hpp"

#if SCORE_HAS_LIBAV
#include <ossia/detail/libav.hpp>

#include <QString>

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#if defined(__EMSCRIPTEN__)
#include <QtCore/private/qstdweb_p.h>
#include <QtCore/private/qwasmlocalfileengine_p.h>

#include <emscripten.h>
#include <emscripten/atomic.h>
#include <emscripten/proxying.h>
#include <emscripten/threading.h>
#include <emscripten/threading_primitives.h>
#include <emscripten/val.h>

#include <cmath>
#endif

namespace Media
{
// ffmpeg pulls 4 MB per read, served from a 16 MB read-ahead window so
// sequential demuxing round-trips to the main thread once per 16 MB.
static constexpr int av_io_buffer_size = 1 << 22;
static constexpr int64_t prefetch_size = 16ll << 20;

struct AvStreamState
{
  std::string url;
  int64_t pos{};
  int64_t size{};
  std::vector<uint8_t> cache;
  int64_t cache_off{-1};
  int64_t cache_len{};
};

bool isStreamedMediaPath(const QString& path) noexcept
{
  return path.startsWith(QLatin1String("weblocalfile:"));
}

bool isStreamedMediaPath(const std::string& path) noexcept
{
  return path.rfind("weblocalfile:", 0) == 0;
}

#if defined(__EMSCRIPTEN__)

// getFile() resolves against a process-wide singleton internally, so any handler
// instance returns the File registered at drop time.
static qstdweb::File getWebFile(const std::string& url)
{
  static QWasmFileEngineHandler handler;
  return handler.getFile(QString::fromStdString(url));
}

// Blocking read on the main thread: the Blob read suspends via JSPI, which is
// legal here because the main event loop is a promising call stack.
static int read_blob_main(const std::string& url, int64_t off, int32_t want, uint8_t* out)
{
  qstdweb::File f = getWebFile(url);
  if(f.file().isUndefined() || f.file().isNull())
    return -1;

  qstdweb::ArrayBuffer ab
      = f.slice((uint64_t)off, (uint64_t)(off + want)).arrayBuffer_sync();
  const int got = (int)ab.byteLength();
  if(got > 0)
    qstdweb::Uint8Array(ab).copyTo((char*)out);
  return got;
}

// A worker cannot touch the main-thread Blob val, and cannot suspend via JSPI.
// So it proxies an *async* read to the main thread and blocks on a futex: the
// main job kicks off Blob.arrayBuffer() (no suspend) and its .then copies the
// bytes into shared memory and wakes the worker. `state` must stay first so its
// address is the futex word.
struct AsyncRead
{
  int32_t state{}; // 0 = pending, 1 = complete
  int32_t result{}; // bytes read, or -1 on error
  const std::string* url{};
  int64_t off{};
  int32_t want{};
  uint8_t* dst{};
};

static void async_read_job(void* p)
{
  auto* a = static_cast<AsyncRead*>(p);
  qstdweb::File f = getWebFile(*a->url);
  if(f.file().isUndefined() || f.file().isNull())
  {
    a->result = -1;
    emscripten_atomic_store_u32(&a->state, 1);
    emscripten_futex_wake(&a->state, 1);
    return;
  }

  qstdweb::Blob blob = f.slice((uint64_t)a->off, (uint64_t)(a->off + a->want));
  EM_ASM(
      {
        var blob = Emval.toValue($0);
        var dst = $1;
        var statePtr = $2;
        var resultPtr = $3;
        blob.arrayBuffer()
            .then(function(ab) {
              var u8 = new Uint8Array(ab);
              HEAPU8.set(u8, dst);
              HEAP32[resultPtr >> 2] = u8.length;
              Atomics.store(HEAP32, statePtr >> 2, 1);
              Atomics.notify(HEAP32, statePtr >> 2);
            })
            .catch(function(e) {
              HEAP32[resultPtr >> 2] = -1;
              Atomics.store(HEAP32, statePtr >> 2, 1);
              Atomics.notify(HEAP32, statePtr >> 2);
            });
      },
      blob.val().as_handle(), a->dst, &a->state, &a->result);
}

static int read_blob_worker(const std::string& url, int64_t off, int32_t want, uint8_t* out)
{
  AsyncRead a;
  a.url = &url;
  a.off = off;
  a.want = want;
  a.dst = out;

  if(!emscripten_proxy_async(
         emscripten_proxy_get_system_queue(), emscripten_main_runtime_thread_id(),
         &async_read_job, &a))
    return -1;

  while(emscripten_atomic_load_u32(&a.state) == 0)
    emscripten_futex_wait(&a.state, 0, INFINITY);

  return a.result;
}

static int fetch_bytes(const std::string& url, int64_t off, int32_t want, uint8_t* dst)
{
  return emscripten_is_main_runtime_thread()
             ? read_blob_main(url, off, want, dst)
             : read_blob_worker(url, off, want, dst);
}

static int av_io_read(void* opaque, uint8_t* out, int size)
{
  auto* st = static_cast<AvStreamState*>(opaque);
  const int64_t avail = st->size - st->pos;
  if(avail <= 0)
    return AVERROR_EOF;

  const int32_t want = (int32_t)std::min<int64_t>(size, avail);

  if(st->cache_off >= 0 && st->pos >= st->cache_off
     && st->pos + want <= st->cache_off + st->cache_len)
  {
    std::memcpy(out, st->cache.data() + (st->pos - st->cache_off), want);
    st->pos += want;
    return want;
  }

  const int32_t win = (int32_t)std::min<int64_t>(prefetch_size, st->size - st->pos);
  const int got = fetch_bytes(st->url, st->pos, win, st->cache.data());
  if(got <= 0)
    return got == 0 ? AVERROR_EOF : AVERROR(EIO);
  st->cache_off = st->pos;
  st->cache_len = got;

  const int n = std::min(want, got);
  std::memcpy(out, st->cache.data(), n);
  st->pos += n;
  return n;
}

static int64_t av_io_seek(void* opaque, int64_t offset, int whence)
{
  auto* st = static_cast<AvStreamState*>(opaque);
  switch(whence & ~AVSEEK_FORCE)
  {
    case AVSEEK_SIZE:
      return st->size;
    case SEEK_SET:
      st->pos = offset;
      break;
    case SEEK_CUR:
      st->pos += offset;
      break;
    case SEEK_END:
      st->pos = st->size + offset;
      break;
    default:
      return -1;
  }
  if(st->pos < 0)
    st->pos = 0;
  return st->pos;
}

#endif

AvIoDevice::AvIoDevice(const QString& path)
{
  auto st = std::make_unique<AvStreamState>();
  st->url = path.toStdString();

#if defined(__EMSCRIPTEN__)
  bool ok = false;
  const auto fetch_size = [&] {
    qstdweb::File f = getWebFile(st->url);
    if(f.file().isUndefined() || f.file().isNull())
      return;
    st->size = (int64_t)f.size();
    ok = st->size > 0;
  };
  if(emscripten_is_main_runtime_thread())
  {
    fetch_size();
  }
  else
  {
    emscripten_proxy_sync(
        emscripten_proxy_get_system_queue(), emscripten_main_runtime_thread_id(),
        [](void* p) { (*static_cast<const decltype(fetch_size)*>(p))(); },
        const_cast<void*>(static_cast<const void*>(&fetch_size)));
  }
  if(!ok)
    return;

  st->cache.resize((size_t)std::min<int64_t>(prefetch_size, st->size));

  auto* buffer = static_cast<unsigned char*>(av_malloc(av_io_buffer_size));
  if(!buffer)
    return;

  avio = avio_alloc_context(
      buffer, av_io_buffer_size, 0, st.get(), &av_io_read, nullptr, &av_io_seek);
  if(!avio)
  {
    av_free(buffer);
    return;
  }
  state = st.release();
#endif
}

AvIoDevice::~AvIoDevice()
{
  if(avio)
  {
    av_freep(&avio->buffer);
    avio_context_free(&avio);
  }
  delete state;
  state = nullptr;
}

AvIoDevice::AvIoDevice(AvIoDevice&& other) noexcept
    : avio{other.avio}
    , state{other.state}
{
  other.avio = nullptr;
  other.state = nullptr;
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
    delete state;
    avio = other.avio;
    state = other.state;
    other.avio = nullptr;
    other.state = nullptr;
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
