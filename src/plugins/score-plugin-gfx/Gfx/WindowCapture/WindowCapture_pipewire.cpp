#include <Gfx/WindowCapture/WindowCaptureBackend.hpp>

#include <ossia/detail/dylib_loader.hpp>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QDebug>
#include <QEventLoop>
#include <QRandomGenerator>
#include <QTimer>

#include <verdigris>
#include <wobjectimpl.h>

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unistd.h>

// ─── PipeWire types and constants (no PipeWire headers needed) ───────────────

extern "C"
{
// Opaque PipeWire types
struct pw_thread_loop;
struct pw_context;
struct pw_core;
struct pw_stream;
struct pw_properties;
struct pw_loop;

// spa_hook: doubly-linked listener node.
// Must be fully defined because PipeWire writes into it.
struct spa_list
{
  struct spa_list* next;
  struct spa_list* prev;
};

struct spa_hook
{
  struct spa_list link;
  struct
  {
    struct spa_list link;
    const void* funcs;
    void* data;
  } cb;
  void (*removed)(struct spa_hook* hook);
  void* priv;
};

// PipeWire stream states
enum pw_stream_state
{
  PW_STREAM_STATE_ERROR = -1,
  PW_STREAM_STATE_UNCONNECTED = 0,
  PW_STREAM_STATE_CONNECTING = 1,
  PW_STREAM_STATE_PAUSED = 2,
  PW_STREAM_STATE_STREAMING = 3
};

enum pw_direction
{
  PW_DIRECTION_INPUT = 0,
  PW_DIRECTION_OUTPUT = 1
};

enum pw_stream_flags
{
  PW_STREAM_FLAG_AUTOCONNECT = (1 << 0),
  PW_STREAM_FLAG_MAP_BUFFERS = (1 << 2)
};

// SPA data types
enum spa_data_type
{
  SPA_DATA_Invalid = 0,
  SPA_DATA_MemPtr = 1,
  SPA_DATA_MemFd = 2,
  SPA_DATA_DmaBuf = 3
};

// SPA video formats matching PipeWire/SPA definitions
enum spa_video_format
{
  SPA_VIDEO_FORMAT_UNKNOWN = 0,
  SPA_VIDEO_FORMAT_ENCODED = 1,
  SPA_VIDEO_FORMAT_I420 = 2,
  SPA_VIDEO_FORMAT_YV12 = 3,
  SPA_VIDEO_FORMAT_YUY2 = 4,
  SPA_VIDEO_FORMAT_UYVY = 5,
  SPA_VIDEO_FORMAT_AYUV = 6,
  SPA_VIDEO_FORMAT_RGBx = 7,
  SPA_VIDEO_FORMAT_BGRx = 8,
  SPA_VIDEO_FORMAT_xRGB = 9,
  SPA_VIDEO_FORMAT_xBGR = 10,
  SPA_VIDEO_FORMAT_RGBA = 11,
  SPA_VIDEO_FORMAT_BGRA = 12,
  SPA_VIDEO_FORMAT_ARGB = 13,
  SPA_VIDEO_FORMAT_ABGR = 14,
  SPA_VIDEO_FORMAT_RGB = 15,
  SPA_VIDEO_FORMAT_BGR = 16
};

// SPA buffer layout
struct spa_chunk
{
  uint32_t offset;
  uint32_t size;
  int32_t stride;
  int32_t flags;
};

struct spa_data
{
  uint32_t type;
  uint32_t flags;
  int fd;
  uint32_t mapoffset;
  uint32_t maxsize;
  void* data;
  struct spa_chunk* chunk;
};

struct spa_meta
{
  uint32_t type;
  uint32_t size;
  void* data;
};

struct spa_buffer
{
  uint32_t n_metas;
  uint32_t n_datas;
  struct spa_meta* metas;
  struct spa_data* datas;
};

struct pw_buffer
{
  struct spa_buffer* buffer;
  void* user_data;
  uint64_t size;
  uint64_t requested;
};

// Stream events callback table
struct pw_stream_events
{
  uint32_t version;
  void (*destroy)(void*);
  void (*state_changed)(
      void*, enum pw_stream_state old, enum pw_stream_state state,
      const char* error);
  void (*control_info)(void*, uint32_t id, const void* info);
  void (*io_changed)(void*, uint32_t id, void* area, uint32_t size);
  void (*param_changed)(void*, uint32_t id, const void* param);
  void (*add_buffer)(void*, struct pw_buffer*);
  void (*remove_buffer)(void*, struct pw_buffer*);
  void (*process)(void*);
  void (*drained)(void*);
  void (*command)(void*, const void*);
  void (*trigger_done)(void*);
};

#define PW_VERSION_STREAM_EVENTS 2

// SPA pod type system identifiers
#define SPA_TYPE_None 1
#define SPA_TYPE_Bool 2
#define SPA_TYPE_Id 3
#define SPA_TYPE_Int 4
#define SPA_TYPE_Long 5
#define SPA_TYPE_Float 6
#define SPA_TYPE_Double 7
#define SPA_TYPE_String 8
#define SPA_TYPE_Bytes 9
#define SPA_TYPE_Rectangle 10
#define SPA_TYPE_Fraction 11
#define SPA_TYPE_Bitmap 12
#define SPA_TYPE_Array 13
#define SPA_TYPE_POINTER_BASE 0x10000
#define SPA_TYPE_Fd (SPA_TYPE_POINTER_BASE + 1)
#define SPA_TYPE_OBJECT_BASE 0x20000
#define SPA_TYPE_OBJECT_PropInfo (SPA_TYPE_OBJECT_BASE + 1)
#define SPA_TYPE_OBJECT_Props (SPA_TYPE_OBJECT_BASE + 2)
#define SPA_TYPE_OBJECT_Format (SPA_TYPE_OBJECT_BASE + 3)
#define SPA_TYPE_OBJECT_ParamBuffers (SPA_TYPE_OBJECT_BASE + 4)
#define SPA_TYPE_OBJECT_ParamMeta (SPA_TYPE_OBJECT_BASE + 5)
#define SPA_TYPE_OBJECT_ParamIO (SPA_TYPE_OBJECT_BASE + 6)

// SPA pod structures
struct spa_pod
{
  uint32_t size;
  uint32_t type;
};

struct spa_pod_object_body
{
  uint32_t type;
  uint32_t id;
};

struct spa_pod_object
{
  struct spa_pod pod;
  struct spa_pod_object_body body;
};

// SPA param IDs
#define SPA_PARAM_EnumFormat 3
#define SPA_PARAM_Format 4

// SPA format property keys
#define SPA_FORMAT_mediaType 1
#define SPA_FORMAT_mediaSubtype 2
#define SPA_FORMAT_VIDEO_format 3
#define SPA_FORMAT_VIDEO_size 4
#define SPA_FORMAT_VIDEO_framerate 5

// SPA media type/subtype
#define SPA_MEDIA_TYPE_video 2
#define SPA_MEDIA_SUBTYPE_raw 1

} // extern "C"

// ─── SPA pod builder (minimal, raw bytes) ────────────────────────────────────

namespace
{

struct PodBuilder
{
  uint8_t buf[1024]{};
  uint32_t offset{0};

  void write_bytes(const void* data, uint32_t len)
  {
    std::memcpy(buf + offset, data, len);
    offset += len;
  }

  void write_u32(uint32_t v) { write_bytes(&v, 4); }

  void write_pod(uint32_t size, uint32_t type)
  {
    write_u32(size);
    write_u32(type);
  }

  void write_id(uint32_t value)
  {
    write_pod(4, SPA_TYPE_Id);
    write_u32(value);
  }

  void write_prop(uint32_t key, uint32_t flags)
  {
    write_u32(key);
    write_u32(flags);
  }

  // Build the EnumFormat object pod for video capture.
  // Specifies media type=video, subtype=raw, and lets PipeWire
  // negotiate format, size, and framerate from the portal source.
  spa_pod* buildEnumFormat()
  {
    offset = 0;
    uint32_t objStart = offset;

    // Object pod header (placeholder size, type)
    write_pod(0, SPA_TYPE_OBJECT_Format);
    // Object body: type, id
    write_u32(SPA_TYPE_OBJECT_Format);
    write_u32(SPA_PARAM_EnumFormat);

    // Property: mediaType = video
    write_prop(SPA_FORMAT_mediaType, 0);
    write_id(SPA_MEDIA_TYPE_video);

    // Property: mediaSubtype = raw
    write_prop(SPA_FORMAT_mediaSubtype, 0);
    write_id(SPA_MEDIA_SUBTYPE_raw);

    // No further constraints: the portal source provides its own format.

    // Patch the object pod size (excludes the 8-byte pod header)
    uint32_t totalSize = offset - objStart - 8;
    std::memcpy(buf + objStart, &totalSize, 4);

    return reinterpret_cast<spa_pod*>(buf + objStart);
  }
};

} // anonymous namespace

// ─── PipeWire symbol loader ──────────────────────────────────────────────────

namespace Gfx::WindowCapture
{

// Forward declaration from the X11 backend
std::unique_ptr<WindowCaptureBackend> createX11Backend();

// Function pointer types for the PipeWire C API (no headers needed)
extern "C"
{
using pw_init_t = void (*)(int* argc, char*** argv);
using pw_deinit_t = void (*)();

using pw_thread_loop_new_t
    = pw_thread_loop* (*)(const char* name, const void* props);
using pw_thread_loop_destroy_t = void (*)(pw_thread_loop*);
using pw_thread_loop_start_t = int (*)(pw_thread_loop*);
using pw_thread_loop_stop_t = void (*)(pw_thread_loop*);
using pw_thread_loop_get_loop_t = pw_loop* (*)(pw_thread_loop*);
using pw_thread_loop_lock_t = void (*)(pw_thread_loop*);
using pw_thread_loop_unlock_t = void (*)(pw_thread_loop*);
using pw_thread_loop_signal_t = void (*)(pw_thread_loop*, bool);
using pw_thread_loop_wait_t = void (*)(pw_thread_loop*);

using pw_context_new_t
    = pw_context* (*)(pw_loop*, pw_properties*, size_t);
using pw_context_destroy_t = void (*)(pw_context*);
using pw_context_connect_fd_t
    = pw_core* (*)(pw_context*, int fd, pw_properties*, size_t);

using pw_core_disconnect_t = int (*)(pw_core*);

using pw_stream_new_t
    = pw_stream* (*)(pw_core*, const char* name, pw_properties*);
using pw_stream_destroy_t = void (*)(pw_stream*);
using pw_stream_connect_t = int (*)(
    pw_stream*, enum pw_direction, uint32_t target_id,
    enum pw_stream_flags flags, const spa_pod** params,
    uint32_t n_params);
using pw_stream_disconnect_t = int (*)(pw_stream*);
using pw_stream_dequeue_buffer_t = pw_buffer* (*)(pw_stream*);
using pw_stream_queue_buffer_t = int (*)(pw_stream*, pw_buffer*);
using pw_stream_add_listener_t = void (*)(
    pw_stream*, spa_hook*, const pw_stream_events*, void*);
using pw_stream_set_active_t = int (*)(pw_stream*, bool);

using pw_properties_new_t = pw_properties* (*)(const char* key, ...);
using pw_properties_free_t = void (*)(pw_properties*);
}

struct libpipewire_capture
{
  pw_init_t init{};
  pw_deinit_t deinit{};

  pw_thread_loop_new_t thread_loop_new{};
  pw_thread_loop_destroy_t thread_loop_destroy{};
  pw_thread_loop_start_t thread_loop_start{};
  pw_thread_loop_stop_t thread_loop_stop{};
  pw_thread_loop_get_loop_t thread_loop_get_loop{};
  pw_thread_loop_lock_t thread_loop_lock{};
  pw_thread_loop_unlock_t thread_loop_unlock{};
  pw_thread_loop_signal_t thread_loop_signal{};
  pw_thread_loop_wait_t thread_loop_wait{};

  pw_context_new_t context_new{};
  pw_context_destroy_t context_destroy{};
  pw_context_connect_fd_t context_connect_fd{};

  pw_core_disconnect_t core_disconnect{};

  pw_stream_new_t stream_new{};
  pw_stream_destroy_t stream_destroy{};
  pw_stream_connect_t stream_connect{};
  pw_stream_disconnect_t stream_disconnect{};
  pw_stream_dequeue_buffer_t stream_dequeue_buffer{};
  pw_stream_queue_buffer_t stream_queue_buffer{};
  pw_stream_add_listener_t stream_add_listener{};
  pw_stream_set_active_t stream_set_active{};

  pw_properties_new_t properties_new{};
  pw_properties_free_t properties_free{};

  bool available{};

  static const libpipewire_capture& instance()
  {
    static const libpipewire_capture self;
    return self;
  }

private:
  ossia::dylib_loader m_lib;

  template <typename T>
  T sym(const char* name)
  {
    return m_lib.symbol<T>(name);
  }

  libpipewire_capture()
  try
    : m_lib{std::vector<std::string_view>{
          "libpipewire-0.3.so.0", "libpipewire-0.3.so"}}
  {
    init = sym<pw_init_t>("pw_init");
    deinit = sym<pw_deinit_t>("pw_deinit");

    thread_loop_new = sym<pw_thread_loop_new_t>("pw_thread_loop_new");
    thread_loop_destroy
        = sym<pw_thread_loop_destroy_t>("pw_thread_loop_destroy");
    thread_loop_start
        = sym<pw_thread_loop_start_t>("pw_thread_loop_start");
    thread_loop_stop
        = sym<pw_thread_loop_stop_t>("pw_thread_loop_stop");
    thread_loop_get_loop
        = sym<pw_thread_loop_get_loop_t>("pw_thread_loop_get_loop");
    thread_loop_lock
        = sym<pw_thread_loop_lock_t>("pw_thread_loop_lock");
    thread_loop_unlock
        = sym<pw_thread_loop_unlock_t>("pw_thread_loop_unlock");
    thread_loop_signal
        = sym<pw_thread_loop_signal_t>("pw_thread_loop_signal");
    thread_loop_wait
        = sym<pw_thread_loop_wait_t>("pw_thread_loop_wait");

    context_new = sym<pw_context_new_t>("pw_context_new");
    context_destroy = sym<pw_context_destroy_t>("pw_context_destroy");
    context_connect_fd
        = sym<pw_context_connect_fd_t>("pw_context_connect_fd");

    core_disconnect = sym<pw_core_disconnect_t>("pw_core_disconnect");

    stream_new = sym<pw_stream_new_t>("pw_stream_new");
    stream_destroy = sym<pw_stream_destroy_t>("pw_stream_destroy");
    stream_connect = sym<pw_stream_connect_t>("pw_stream_connect");
    stream_disconnect
        = sym<pw_stream_disconnect_t>("pw_stream_disconnect");
    stream_dequeue_buffer
        = sym<pw_stream_dequeue_buffer_t>("pw_stream_dequeue_buffer");
    stream_queue_buffer
        = sym<pw_stream_queue_buffer_t>("pw_stream_queue_buffer");
    stream_add_listener
        = sym<pw_stream_add_listener_t>("pw_stream_add_listener");
    stream_set_active
        = sym<pw_stream_set_active_t>("pw_stream_set_active");

    properties_new = sym<pw_properties_new_t>("pw_properties_new");
    properties_free = sym<pw_properties_free_t>("pw_properties_free");

    available = init && deinit && thread_loop_new && thread_loop_destroy
                && thread_loop_start && thread_loop_stop
                && thread_loop_get_loop && thread_loop_lock
                && thread_loop_unlock && context_new && context_destroy
                && context_connect_fd && core_disconnect && stream_new
                && stream_destroy && stream_connect && stream_disconnect
                && stream_dequeue_buffer && stream_queue_buffer
                && stream_add_listener && properties_new
                && properties_free;
  }
  catch(...)
  {
    available = false;
  }
};

// ─── Portal Response Watcher (verdigris QObject for D-Bus signal) ────────────

class PortalResponseWatcher : public QObject
{
  W_OBJECT(PortalResponseWatcher)
public:
  explicit PortalResponseWatcher(QObject* parent = nullptr)
      : QObject(parent)
  {
  }

  uint responseCode{99};
  QVariantMap results;
  bool received{false};

  void onResponse(uint response, QVariantMap res)
  {
    responseCode = response;
    results = std::move(res);
    received = true;
    done();
  }
  W_SLOT(onResponse)

  void done() W_SIGNAL(done);
};
W_OBJECT_IMPL(PortalResponseWatcher)

// ─── xdg-desktop-portal D-Bus helpers ────────────────────────────────────────

namespace
{

static QString makeToken()
{
  return QStringLiteral("ossia_score_%1")
      .arg(QRandomGenerator::global()->generate());
}

// Helper: call a portal method, wait for the async Response signal, return
// the response code and results. Returns false on failure.
static bool portalCall(
    QDBusInterface& portal, const QString& method,
    const QList<QVariant>& args, const QString& requestToken,
    uint32_t& outResponse, QVariantMap& outResults, int timeoutMs = 60000)
{
  auto bus = QDBusConnection::sessionBus();
  QString senderName = bus.baseService().mid(1).replace('.', '_');
  QString requestPath
      = QStringLiteral(
            "/org/freedesktop/portal/desktop/request/%1/%2")
            .arg(senderName, requestToken);

  PortalResponseWatcher watcher;
  QEventLoop loop;
  QObject::connect(&watcher, &PortalResponseWatcher::done, &loop, &QEventLoop::quit);

  bus.connect(
      QString(), requestPath,
      QStringLiteral("org.freedesktop.portal.Request"),
      QStringLiteral("Response"),
      &watcher, SLOT(onResponse(uint,QVariantMap)));

  // Build and send the D-Bus call
  QDBusMessage msg = QDBusMessage::createMethodCall(
      portal.service(), portal.path(), portal.interface(), method);
  msg.setArguments(args);
  QDBusMessage reply = bus.call(msg);

  if(reply.type() == QDBusMessage::ErrorMessage)
  {
    qDebug() << "WindowCapture PipeWire:" << method
             << "failed:" << reply.errorMessage();
    return false;
  }

  QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
  if(!watcher.received)
    loop.exec();

  bus.disconnect(
      QString(), requestPath,
      QStringLiteral("org.freedesktop.portal.Request"),
      QStringLiteral("Response"),
      &watcher, SLOT(onResponse(uint,QVariantMap)));

  if(!watcher.received)
  {
    qDebug() << "WindowCapture PipeWire:" << method << "timed out";
    return false;
  }

  outResponse = watcher.responseCode;
  outResults = watcher.results;
  return true;
}

// Perform the full portal ScreenCast flow and return the PipeWire fd.
// Must be called from a thread with a Qt event loop (typically the UI thread).
// Returns -1 on failure.
static int openPortalScreenCast()
{
  auto bus = QDBusConnection::sessionBus();
  if(!bus.isConnected())
  {
    qDebug() << "WindowCapture PipeWire: D-Bus session bus not connected";
    return -1;
  }

  QDBusInterface portal(
      QStringLiteral("org.freedesktop.portal.Desktop"),
      QStringLiteral("/org/freedesktop/portal/desktop"),
      QStringLiteral("org.freedesktop.portal.ScreenCast"), bus);
  if(!portal.isValid())
  {
    qDebug() << "WindowCapture PipeWire: portal interface not available";
    return -1;
  }

  uint32_t response{};
  QVariantMap results;

  // ── Step 1: CreateSession ──
  QString sessionToken = makeToken();
  QString requestToken = makeToken();

  QVariantMap createOpts;
  createOpts[QStringLiteral("handle_token")] = requestToken;
  createOpts[QStringLiteral("session_handle_token")] = sessionToken;

  if(!portalCall(
         portal, QStringLiteral("CreateSession"),
         {QVariant::fromValue(createOpts)}, requestToken, response,
         results, 30000))
    return -1;

  if(response != 0)
  {
    qDebug() << "WindowCapture PipeWire: CreateSession rejected:"
             << response;
    return -1;
  }

  QDBusObjectPath sessionPath
      = results[QStringLiteral("session_handle")]
            .value<QDBusObjectPath>();
  if(sessionPath.path().isEmpty())
  {
    qDebug() << "WindowCapture PipeWire: no session handle in response";
    return -1;
  }

  // ── Step 2: SelectSources (types=2 for WINDOW) ──
  requestToken = makeToken();

  QVariantMap selectOpts;
  selectOpts[QStringLiteral("handle_token")] = requestToken;
  selectOpts[QStringLiteral("types")]
      = QVariant::fromValue(uint32_t(2)); // WINDOW
  selectOpts[QStringLiteral("multiple")] = false;

  if(!portalCall(
         portal, QStringLiteral("SelectSources"),
         {QVariant::fromValue(sessionPath), QVariant::fromValue(selectOpts)},
         requestToken, response, results, 60000))
    return -1;

  if(response != 0)
  {
    qDebug() << "WindowCapture PipeWire: SelectSources rejected:"
             << response;
    return -1;
  }

  // ── Step 3: Start (user confirms the window picker) ──
  requestToken = makeToken();

  QVariantMap startOpts;
  startOpts[QStringLiteral("handle_token")] = requestToken;

  if(!portalCall(
         portal, QStringLiteral("Start"),
         {QVariant::fromValue(sessionPath), QString(),
          QVariant::fromValue(startOpts)},
         requestToken, response, results, 60000))
    return -1;

  if(response != 0)
  {
    qDebug() << "WindowCapture PipeWire: Start rejected:" << response;
    return -1;
  }

  // ── Step 4: OpenPipeWireRemote ──
  QVariantMap openOpts;
  QDBusMessage openMsg = QDBusMessage::createMethodCall(
      portal.service(), portal.path(), portal.interface(),
      QStringLiteral("OpenPipeWireRemote"));
  openMsg.setArguments(
      {QVariant::fromValue(sessionPath), QVariant::fromValue(openOpts)});

  QDBusMessage openReply = bus.call(openMsg);
  if(openReply.type() == QDBusMessage::ErrorMessage)
  {
    qDebug() << "WindowCapture PipeWire: OpenPipeWireRemote failed:"
             << openReply.errorMessage();
    return -1;
  }

  if(openReply.arguments().isEmpty())
  {
    qDebug() << "WindowCapture PipeWire: OpenPipeWireRemote returned "
                "no arguments";
    return -1;
  }

  QDBusUnixFileDescriptor dbusfd
      = openReply.arguments().at(0).value<QDBusUnixFileDescriptor>();
  if(!dbusfd.isValid())
  {
    qDebug() << "WindowCapture PipeWire: invalid fd from "
                "OpenPipeWireRemote";
    return -1;
  }

  // dup() so the fd outlives the QDBusUnixFileDescriptor
  return ::dup(dbusfd.fileDescriptor());
}

static bool portalAvailable()
{
  auto bus = QDBusConnection::sessionBus();
  if(!bus.isConnected())
    return false;

  QDBusInterface portal(
      QStringLiteral("org.freedesktop.portal.Desktop"),
      QStringLiteral("/org/freedesktop/portal/desktop"),
      QStringLiteral("org.freedesktop.portal.ScreenCast"), bus);
  return portal.isValid();
}

} // anonymous namespace

// ─── PipeWire Window Capture Backend ─────────────────────────────────────────

class PipeWireWindowCaptureBackend final : public WindowCaptureBackend
{
public:
  ~PipeWireWindowCaptureBackend() override { stop(); }

  bool available() const override
  {
    return libpipewire_capture::instance().available && portalAvailable();
  }

  std::vector<CapturableWindow> enumerate() override
  {
    // Wayland does not allow window enumeration.
    // The portal picker dialog handles selection during start().
    return {};
  }

  bool start(uint64_t windowId) override
  {
    Q_UNUSED(windowId)
    stop();

    auto& pw = libpipewire_capture::instance();
    if(!pw.available)
      return false;

    // Initialize PipeWire
    pw.init(nullptr, nullptr);

    // Run the portal flow to get a PipeWire fd.
    int fd = openPortalScreenCast();
    if(fd < 0)
    {
      qDebug() << "WindowCapture PipeWire: portal flow failed";
      return false;
    }

    // Create PipeWire thread loop
    m_loop = pw.thread_loop_new("score-wincap", nullptr);
    if(!m_loop)
    {
      qDebug() << "WindowCapture PipeWire: pw_thread_loop_new failed";
      ::close(fd);
      return false;
    }

    pw_loop* loop = pw.thread_loop_get_loop(m_loop);

    m_context = pw.context_new(loop, nullptr, 0);
    if(!m_context)
    {
      qDebug() << "WindowCapture PipeWire: pw_context_new failed";
      pw.thread_loop_destroy(m_loop);
      m_loop = nullptr;
      ::close(fd);
      return false;
    }

    if(pw.thread_loop_start(m_loop) != 0)
    {
      qDebug() << "WindowCapture PipeWire: pw_thread_loop_start failed";
      pw.context_destroy(m_context);
      m_context = nullptr;
      pw.thread_loop_destroy(m_loop);
      m_loop = nullptr;
      ::close(fd);
      return false;
    }

    pw.thread_loop_lock(m_loop);

    // Connect to PipeWire via the portal fd
    m_core = pw.context_connect_fd(m_context, fd, nullptr, 0);
    if(!m_core)
    {
      qDebug() << "WindowCapture PipeWire: pw_context_connect_fd failed";
      pw.thread_loop_unlock(m_loop);
      pw.thread_loop_stop(m_loop);
      pw.context_destroy(m_context);
      m_context = nullptr;
      pw.thread_loop_destroy(m_loop);
      m_loop = nullptr;
      return false;
    }

    // Create stream
    pw_properties* props = pw.properties_new(
        "media.type", "Video", "media.category", "Capture",
        "media.role", "Screen", nullptr);

    m_stream = pw.stream_new(m_core, "score-window-capture", props);
    if(!m_stream)
    {
      qDebug() << "WindowCapture PipeWire: pw_stream_new failed";
      pw.core_disconnect(m_core);
      m_core = nullptr;
      pw.thread_loop_unlock(m_loop);
      pw.thread_loop_stop(m_loop);
      pw.context_destroy(m_context);
      m_context = nullptr;
      pw.thread_loop_destroy(m_loop);
      m_loop = nullptr;
      return false;
    }

    // Set up stream events
    std::memset(&m_streamEvents, 0, sizeof(m_streamEvents));
    m_streamEvents.version = PW_VERSION_STREAM_EVENTS;
    m_streamEvents.state_changed
        = &PipeWireWindowCaptureBackend::onStateChanged;
    m_streamEvents.param_changed
        = &PipeWireWindowCaptureBackend::onParamChanged;
    m_streamEvents.process = &PipeWireWindowCaptureBackend::onProcess;

    std::memset(&m_streamListener, 0, sizeof(m_streamListener));
    pw.stream_add_listener(
        m_stream, &m_streamListener, &m_streamEvents, this);

    // Build format params
    PodBuilder podBuilder;
    spa_pod* formatPod = podBuilder.buildEnumFormat();
    const spa_pod* params[] = {formatPod};

    // Connect the stream as input (we consume video from the portal)
    int ret = pw.stream_connect(
        m_stream, PW_DIRECTION_INPUT, static_cast<uint32_t>(-1), // PW_ID_ANY
        static_cast<pw_stream_flags>(
            PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
        params, 1);

    pw.thread_loop_unlock(m_loop);

    if(ret < 0)
    {
      qDebug() << "WindowCapture PipeWire: pw_stream_connect failed:"
               << ret;
      stop();
      return false;
    }

    m_running.store(true, std::memory_order_release);
    return true;
  }

  void stop() override
  {
    m_running.store(false, std::memory_order_release);

    auto& pw = libpipewire_capture::instance();
    if(!pw.available)
      return;

    if(m_loop)
      pw.thread_loop_lock(m_loop);

    if(m_stream)
    {
      pw.stream_disconnect(m_stream);
      pw.stream_destroy(m_stream);
      m_stream = nullptr;
    }

    if(m_core)
    {
      pw.core_disconnect(m_core);
      m_core = nullptr;
    }

    if(m_loop)
      pw.thread_loop_unlock(m_loop);

    if(m_loop)
      pw.thread_loop_stop(m_loop);

    if(m_context)
    {
      pw.context_destroy(m_context);
      m_context = nullptr;
    }

    if(m_loop)
    {
      pw.thread_loop_destroy(m_loop);
      m_loop = nullptr;
    }

    {
      std::lock_guard lock(m_frameMutex);
      m_frameData.clear();
      m_frameWidth = 0;
      m_frameHeight = 0;
      m_frameStride = 0;
      m_frameFormat = CapturedFrame::None;
      m_dmabufFd = -1;
    }
  }

  CapturedFrame grab() override
  {
    if(!m_running.load(std::memory_order_acquire))
      return {};

    std::lock_guard lock(m_frameMutex);

    if(m_frameFormat == CapturedFrame::None)
      return {};

    CapturedFrame frame;
    frame.width = m_frameWidth;
    frame.height = m_frameHeight;

    if(m_frameFormat == CapturedFrame::DMA_BUF_FD)
    {
      frame.type = CapturedFrame::DMA_BUF_FD;
      frame.dmabufFd = m_dmabufFd;
      frame.drmFormat = m_drmFormat;
      frame.drmModifier = m_drmModifier;
      frame.dmabufStride = m_dmabufStride;
      frame.dmabufOffset = m_dmabufOffset;
    }
    else
    {
      frame.type = m_frameFormat;
      frame.data = m_frameData.data();
      frame.stride = m_frameStride;
    }

    return frame;
  }

private:
  // ── PipeWire stream callbacks (static, dispatched via user_data) ──

  static void onStateChanged(
      void* data, enum pw_stream_state old_state,
      enum pw_stream_state state, const char* error)
  {
    Q_UNUSED(old_state)
    Q_UNUSED(data)

    static constexpr const char* stateNames[]
        = {"error", "unconnected", "connecting", "paused", "streaming"};
    int idx = static_cast<int>(state) + 1;
    if(idx >= 0 && idx < 5)
      qDebug() << "WindowCapture PipeWire: stream state:"
               << stateNames[idx];
    if(error)
      qDebug() << "WindowCapture PipeWire: stream error:" << error;
  }

  static void onParamChanged(void* data, uint32_t id, const void* param)
  {
    if(!param || id != SPA_PARAM_Format)
      return;

    auto* self = static_cast<PipeWireWindowCaptureBackend*>(data);

    // Parse the spa_pod_object to extract video format, width, height.
    const auto* pod = static_cast<const spa_pod*>(param);
    if(pod->type != SPA_TYPE_OBJECT_Format)
      return;

    const auto* obj = static_cast<const spa_pod_object*>(param);
    const uint8_t* body = reinterpret_cast<const uint8_t*>(&obj->body);
    const uint8_t* end = body + pod->size;

    // Skip object body header (type 4 + id 4 = 8 bytes)
    const uint8_t* p = body + 8;

    while(p + 8 <= end)
    {
      uint32_t key, flags;
      std::memcpy(&key, p, 4);
      p += 4;
      std::memcpy(&flags, p, 4);
      p += 4;

      if(p + 8 > end)
        break;

      uint32_t valSize, valType;
      std::memcpy(&valSize, p, 4);
      std::memcpy(&valType, p + 4, 4);
      const uint8_t* valData = p + 8;

      if(key == SPA_FORMAT_VIDEO_format && valType == SPA_TYPE_Id
         && valSize >= 4)
      {
        uint32_t fmt;
        std::memcpy(&fmt, valData, 4);
        self->m_spaVideoFormat = static_cast<spa_video_format>(fmt);
        qDebug() << "WindowCapture PipeWire: negotiated format:" << fmt;
      }
      else if(
          key == SPA_FORMAT_VIDEO_size
          && valType == SPA_TYPE_Rectangle && valSize >= 8)
      {
        uint32_t w, h;
        std::memcpy(&w, valData, 4);
        std::memcpy(&h, valData + 4, 4);
        self->m_negotiatedWidth = w;
        self->m_negotiatedHeight = h;
        qDebug() << "WindowCapture PipeWire: negotiated size:" << w
                 << "x" << h;
      }

      // Advance past the value pod (8-byte header + padded size)
      uint32_t paddedSize = (valSize + 7) & ~7u;
      p = p + 8 + paddedSize;
    }
  }

  static void onProcess(void* data)
  {
    auto* self = static_cast<PipeWireWindowCaptureBackend*>(data);
    auto& pw = libpipewire_capture::instance();

    pw_buffer* buf = pw.stream_dequeue_buffer(self->m_stream);
    if(!buf)
      return;

    spa_buffer* spaBuf = buf->buffer;
    if(!spaBuf || spaBuf->n_datas == 0)
    {
      pw.stream_queue_buffer(self->m_stream, buf);
      return;
    }

    spa_data& d = spaBuf->datas[0];
    int width = self->m_negotiatedWidth;
    int height = self->m_negotiatedHeight;

    if(width <= 0 || height <= 0)
    {
      pw.stream_queue_buffer(self->m_stream, buf);
      return;
    }

    if(d.type == SPA_DATA_MemPtr || d.type == SPA_DATA_MemFd)
    {
      if(d.data && d.chunk && d.chunk->size > 0)
      {
        int stride = d.chunk->stride;
        if(stride <= 0)
          stride = width * 4;

        uint32_t frameOffset = d.chunk->offset;
        uint32_t size = d.chunk->size;
        const uint8_t* src
            = static_cast<const uint8_t*>(d.data) + frameOffset;

        CapturedFrame::Type frameType = CapturedFrame::CPU_BGRA;
        switch(self->m_spaVideoFormat)
        {
          case SPA_VIDEO_FORMAT_RGBx:
          case SPA_VIDEO_FORMAT_RGBA:
            frameType = CapturedFrame::CPU_RGBA;
            break;
          default:
            frameType = CapturedFrame::CPU_BGRA;
            break;
        }

        {
          std::lock_guard lock(self->m_frameMutex);
          self->m_frameData.resize(size);
          std::memcpy(self->m_frameData.data(), src, size);
          self->m_frameWidth = width;
          self->m_frameHeight = height;
          self->m_frameStride = stride;
          self->m_frameFormat = frameType;
          self->m_dmabufFd = -1;
        }
      }
    }
    else if(d.type == SPA_DATA_DmaBuf)
    {
      if(d.fd >= 0)
      {
        std::lock_guard lock(self->m_frameMutex);
        self->m_frameData.clear();
        self->m_frameWidth = width;
        self->m_frameHeight = height;
        self->m_frameStride = 0;
        self->m_frameFormat = CapturedFrame::DMA_BUF_FD;
        self->m_dmabufFd = d.fd;
        self->m_dmabufStride
            = d.chunk ? d.chunk->stride : (width * 4);
        self->m_dmabufOffset
            = d.chunk ? static_cast<int>(d.chunk->offset) : 0;

        // Map SPA video format to DRM fourcc
        switch(self->m_spaVideoFormat)
        {
          case SPA_VIDEO_FORMAT_BGRx:
            self->m_drmFormat = 0x34325258; // DRM_FORMAT_XRGB8888
            break;
          case SPA_VIDEO_FORMAT_BGRA:
            self->m_drmFormat = 0x34324152; // DRM_FORMAT_ARGB8888
            break;
          case SPA_VIDEO_FORMAT_RGBx:
            self->m_drmFormat = 0x34325842; // DRM_FORMAT_XBGR8888
            break;
          case SPA_VIDEO_FORMAT_RGBA:
            self->m_drmFormat = 0x34324241; // DRM_FORMAT_ABGR8888
            break;
          default:
            self->m_drmFormat = 0x34325258;
            break;
        }
        self->m_drmModifier = 0; // LINEAR
      }
    }

    pw.stream_queue_buffer(self->m_stream, buf);
  }

  // ── Member state ──

  pw_thread_loop* m_loop{};
  pw_context* m_context{};
  pw_core* m_core{};
  pw_stream* m_stream{};
  pw_stream_events m_streamEvents{};
  spa_hook m_streamListener{};
  std::atomic<bool> m_running{false};

  // Negotiated video format
  spa_video_format m_spaVideoFormat{SPA_VIDEO_FORMAT_UNKNOWN};
  int m_negotiatedWidth{0};
  int m_negotiatedHeight{0};

  // Latest frame (written from PipeWire thread, read from grab())
  std::mutex m_frameMutex;
  std::vector<uint8_t> m_frameData;
  int m_frameWidth{0};
  int m_frameHeight{0};
  int m_frameStride{0};
  CapturedFrame::Type m_frameFormat{CapturedFrame::None};

  // DMA-BUF frame info
  int m_dmabufFd{-1};
  uint32_t m_drmFormat{0};
  uint64_t m_drmModifier{0};
  int m_dmabufStride{0};
  int m_dmabufOffset{0};
};

// ─── Linux factory: X11 first, then PipeWire ────────────────────────────────

std::unique_ptr<WindowCaptureBackend> createWindowCaptureBackend()
{
  // Try X11 first (works on X11 sessions and XWayland)
  if(auto x11 = createX11Backend())
    return x11;

  // Try PipeWire (native Wayland via xdg-desktop-portal)
  auto pw = std::make_unique<PipeWireWindowCaptureBackend>();
  if(pw->available())
    return pw;

  return nullptr;
}

} // namespace Gfx::WindowCapture
