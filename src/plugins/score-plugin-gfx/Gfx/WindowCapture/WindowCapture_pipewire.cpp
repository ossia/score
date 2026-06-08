#include <Gfx/WindowCapture/WindowCaptureBackend.hpp>

#include <ossia/detail/dylib_loader.hpp>

#include <QDebug>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unistd.h>

#if defined(SCORE_HAS_PIPEWIRE_VIDEO_IO)

#include <pipewire/pipewire.h>
#include <pipewire/stream.h>
#include <spa/param/format-utils.h>
#include <spa/param/video/format-utils.h>
#include <spa/pod/builder.h>
#include <spa/utils/result.h>

#include <libremidi/backends/linux/pipewire/context.hpp>
#include <libremidi/backends/linux/pipewire/instance.hpp>
#include <libremidi/backends/linux/pipewire/loader.hpp>
#include <libremidi/backends/linux/pipewire/subscription.hpp>
// Note: WindowCapture doesn't currently use format_negotiation because
// the portal source defines its own width/height/format; we just
// accept whatever it offers. If we ever want to advertise GPU-supported
// modifiers to the portal compositor, we'd add format_negotiation here.

// ─── sd-bus types (no systemd headers needed) ───────────────────────────────

extern "C"
{
struct sd_bus;
struct sd_bus_message;
struct sd_bus_slot;

struct sd_bus_error
{
  const char* name;
  const char* message;
  int _need_free;
};

using sd_bus_message_handler_t
    = int (*)(sd_bus_message*, void*, sd_bus_error*);
} // extern "C"

namespace Gfx::WindowCapture
{

// Forward declaration from the X11 backend
std::unique_ptr<WindowCaptureBackend> createX11Backend();

namespace
{

// Build the EnumFormat object pod for portal video capture.
// media.type=video, subtype=raw — the portal source provides its own
// width/height/format, so we don't constrain further. The buffer is
// caller-owned and must outlive the returned spa_pod*.
inline const spa_pod*
buildEnumFormatPod(uint8_t* buffer, std::size_t buffer_size)
{
  spa_pod_builder b
      = SPA_POD_BUILDER_INIT(buffer, static_cast<uint32_t>(buffer_size));
  spa_pod_frame frame{};
  spa_pod_builder_push_object(
      &b, &frame, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat);
  spa_pod_builder_add(
      &b, SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
      SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw), 0);
  return static_cast<const spa_pod*>(spa_pod_builder_pop(&b, &frame));
}

} // anonymous namespace

// ─── sd-bus symbol loader ───────────────────────────────────────────────────

extern "C"
{
using sd_bus_open_user_t = int (*)(sd_bus**);
using sd_bus_unref_t = sd_bus* (*)(sd_bus*);
using sd_bus_get_unique_name_t = int (*)(sd_bus*, const char**);
using sd_bus_call_t = int (*)(
    sd_bus*, sd_bus_message*, uint64_t usec, sd_bus_error*,
    sd_bus_message**);
using sd_bus_message_new_method_call_t = int (*)(
    sd_bus*, sd_bus_message**, const char*, const char*, const char*,
    const char*);
using sd_bus_message_unref_t = sd_bus_message* (*)(sd_bus_message*);
using sd_bus_message_append_basic_t
    = int (*)(sd_bus_message*, char, const void*);
using sd_bus_message_open_container_t
    = int (*)(sd_bus_message*, char, const char*);
using sd_bus_message_close_container_t = int (*)(sd_bus_message*);
using sd_bus_message_read_basic_t
    = int (*)(sd_bus_message*, char, void*);
using sd_bus_message_enter_container_t
    = int (*)(sd_bus_message*, char, const char*);
using sd_bus_message_exit_container_t = int (*)(sd_bus_message*);
using sd_bus_message_skip_t = int (*)(sd_bus_message*, const char*);
using sd_bus_add_match_t = int (*)(
    sd_bus*, sd_bus_slot**, const char*, sd_bus_message_handler_t,
    void*);
using sd_bus_slot_unref_t = sd_bus_slot* (*)(sd_bus_slot*);
using sd_bus_process_t = int (*)(sd_bus*, sd_bus_message**);
using sd_bus_wait_t = int (*)(sd_bus*, uint64_t);
using sd_bus_error_free_t = void (*)(sd_bus_error*);
}

struct libsdbus
{
  sd_bus_open_user_t open_user{};
  sd_bus_unref_t unref{};
  sd_bus_get_unique_name_t get_unique_name{};
  sd_bus_call_t call{};
  sd_bus_message_new_method_call_t message_new_method_call{};
  sd_bus_message_unref_t message_unref{};
  sd_bus_message_append_basic_t message_append_basic{};
  sd_bus_message_open_container_t message_open_container{};
  sd_bus_message_close_container_t message_close_container{};
  sd_bus_message_read_basic_t message_read_basic{};
  sd_bus_message_enter_container_t message_enter_container{};
  sd_bus_message_exit_container_t message_exit_container{};
  sd_bus_message_skip_t message_skip{};
  sd_bus_add_match_t add_match{};
  sd_bus_slot_unref_t slot_unref{};
  sd_bus_process_t process{};
  sd_bus_wait_t wait{};
  sd_bus_error_free_t error_free{};

  bool available{};

  static const libsdbus& instance()
  {
    static const libsdbus self;
    return self;
  }

private:
  ossia::dylib_loader m_lib;

  template <typename T>
  T sym(const char* name)
  {
    return m_lib.symbol<T>(name);
  }

  libsdbus()
  try
    : m_lib{std::vector<std::string_view>{
          "libsystemd.so.0", "libsystemd.so"}}
  {
    open_user = sym<sd_bus_open_user_t>("sd_bus_open_user");
    unref = sym<sd_bus_unref_t>("sd_bus_unref");
    get_unique_name
        = sym<sd_bus_get_unique_name_t>("sd_bus_get_unique_name");
    call = sym<sd_bus_call_t>("sd_bus_call");
    message_new_method_call = sym<sd_bus_message_new_method_call_t>(
        "sd_bus_message_new_method_call");
    message_unref
        = sym<sd_bus_message_unref_t>("sd_bus_message_unref");
    message_append_basic = sym<sd_bus_message_append_basic_t>(
        "sd_bus_message_append_basic");
    message_open_container = sym<sd_bus_message_open_container_t>(
        "sd_bus_message_open_container");
    message_close_container = sym<sd_bus_message_close_container_t>(
        "sd_bus_message_close_container");
    message_read_basic = sym<sd_bus_message_read_basic_t>(
        "sd_bus_message_read_basic");
    message_enter_container = sym<sd_bus_message_enter_container_t>(
        "sd_bus_message_enter_container");
    message_exit_container = sym<sd_bus_message_exit_container_t>(
        "sd_bus_message_exit_container");
    message_skip
        = sym<sd_bus_message_skip_t>("sd_bus_message_skip");
    add_match = sym<sd_bus_add_match_t>("sd_bus_add_match");
    slot_unref = sym<sd_bus_slot_unref_t>("sd_bus_slot_unref");
    process = sym<sd_bus_process_t>("sd_bus_process");
    wait = sym<sd_bus_wait_t>("sd_bus_wait");
    error_free = sym<sd_bus_error_free_t>("sd_bus_error_free");

    available = open_user && unref && get_unique_name && call
                && message_new_method_call && message_unref
                && message_append_basic && message_open_container
                && message_close_container && message_read_basic
                && message_enter_container && message_exit_container
                && message_skip && add_match && slot_unref && process
                && wait && error_free;
  }
  catch(...)
  {
    available = false;
  }
};

// ─── xdg-desktop-portal helpers (sd-bus) ────────────────────────────────────

namespace
{

static std::string makeToken()
{
  static std::atomic<int> counter{0};
  return "ossia_score_"
         + std::to_string(static_cast<unsigned>(getpid())) + "_"
         + std::to_string(counter.fetch_add(1));
}

// Helper: append a string→string entry into an open a{sv} container
static void appendDictString(
    const libsdbus& sd, sd_bus_message* m, const char* key,
    const char* value)
{
  sd.message_open_container(m, 'e', "sv");
  sd.message_append_basic(m, 's', key);
  sd.message_open_container(m, 'v', "s");
  sd.message_append_basic(m, 's', value);
  sd.message_close_container(m);
  sd.message_close_container(m);
}

// Helper: append a string→uint32 entry into an open a{sv} container
static void appendDictUint32(
    const libsdbus& sd, sd_bus_message* m, const char* key,
    uint32_t value)
{
  sd.message_open_container(m, 'e', "sv");
  sd.message_append_basic(m, 's', key);
  sd.message_open_container(m, 'v', "u");
  sd.message_append_basic(m, 'u', &value);
  sd.message_close_container(m);
  sd.message_close_container(m);
}

// Helper: append a string→bool entry into an open a{sv} container
static void appendDictBool(
    const libsdbus& sd, sd_bus_message* m, const char* key, bool value)
{
  sd.message_open_container(m, 'e', "sv");
  sd.message_append_basic(m, 's', key);
  sd.message_open_container(m, 'v', "b");
  int v = value ? 1 : 0;
  sd.message_append_basic(m, 'b', &v);
  sd.message_close_container(m);
  sd.message_close_container(m);
}

// Data passed to the Response signal callback
struct PortalResponse
{
  const libsdbus* sd{};
  uint32_t code{99};
  std::string session_handle;
  uint32_t pipewire_node{0}; // PipeWire node ID from Start response
  bool received{false};
};

// sd-bus callback for the Response(u, a{sv}) signal
static int onPortalResponse(
    sd_bus_message* m, void* userdata, sd_bus_error*)
{
  auto* resp = static_cast<PortalResponse*>(userdata);
  auto& sd = *resp->sd;

  // Read response code
  sd.message_read_basic(m, 'u', &resp->code);

  // Read results dict a{sv}
  if(sd.message_enter_container(m, 'a', "{sv}") >= 0)
  {
    while(sd.message_enter_container(m, 'e', "sv") > 0)
    {
      const char* key = nullptr;
      sd.message_read_basic(m, 's', &key);

      if(key && std::strcmp(key, "session_handle") == 0)
      {
        if(sd.message_enter_container(m, 'v', "s") >= 0)
        {
          const char* val = nullptr;
          sd.message_read_basic(m, 's', &val);
          if(val)
            resp->session_handle = val;
          sd.message_exit_container(m);
        }
      }
      else if(key && std::strcmp(key, "streams") == 0)
      {
        // streams: a(ua{sv}) — array of structs with node_id + properties
        // We read the first stream's node ID.
        if(sd.message_enter_container(m, 'v', "a(ua{sv})") >= 0)
        {
          if(sd.message_enter_container(m, 'a', "(ua{sv})") >= 0)
          {
            if(sd.message_enter_container(m, 'r', "ua{sv}") >= 0)
            {
              sd.message_read_basic(m, 'u', &resp->pipewire_node);
              sd.message_skip(m, "a{sv}"); // skip properties
              sd.message_exit_container(m);
            }
            sd.message_exit_container(m);
          }
          sd.message_exit_container(m);
        }
      }
      else
      {
        sd.message_skip(m, "v");
      }
      sd.message_exit_container(m);
    }
    sd.message_exit_container(m);
  }

  resp->received = true;
  return 0;
}

// Wait for a Response signal on the bus, with timeout.
static bool waitForResponse(
    const libsdbus& sd, sd_bus* bus, PortalResponse& resp,
    int timeoutMs)
{
  auto deadline = std::chrono::steady_clock::now()
                  + std::chrono::milliseconds(timeoutMs);

  while(!resp.received)
  {
    auto now = std::chrono::steady_clock::now();
    if(now >= deadline)
      break;

    int r = sd.process(bus, nullptr);
    if(r < 0)
      break;
    if(r > 0)
      continue; // more work queued

    int64_t remaining_us
        = std::chrono::duration_cast<std::chrono::microseconds>(
              deadline - now)
              .count();
    if(remaining_us > 100000)
      remaining_us = 100000;
    sd.wait(bus, static_cast<uint64_t>(remaining_us));
  }

  return resp.received;
}

// Build the request object path for the portal Response signal.
static std::string makeRequestPath(
    const libsdbus& sd, sd_bus* bus, const std::string& token)
{
  const char* uniqueName = nullptr;
  sd.get_unique_name(bus, &uniqueName);
  if(!uniqueName)
    return {};

  // Skip leading ':' and replace '.' with '_'
  std::string sender(uniqueName + 1);
  for(auto& c : sender)
    if(c == '.')
      c = '_';

  return "/org/freedesktop/portal/desktop/request/" + sender + "/"
         + token;
}

// Portal source types
static constexpr uint32_t PORTAL_SOURCE_MONITOR = 1;
static constexpr uint32_t PORTAL_SOURCE_WINDOW = 2;

// Portal cursor modes
static constexpr uint32_t PORTAL_CURSOR_HIDDEN = 1;
static constexpr uint32_t PORTAL_CURSOR_EMBEDDED = 2;
static constexpr uint32_t PORTAL_CURSOR_METADATA = 4;

static constexpr const char* PORTAL_DEST
    = "org.freedesktop.portal.Desktop";
static constexpr const char* PORTAL_PATH
    = "/org/freedesktop/portal/desktop";
static constexpr const char* PORTAL_SCREENCAST_IFACE
    = "org.freedesktop.portal.ScreenCast";

// Result of the portal ScreenCast flow
struct PortalScreenCastResult
{
  int fd{-1};
  uint32_t pipewire_node{0};
};

// Perform the full portal ScreenCast flow and return the PipeWire fd + node ID.
// sourceType: 1=MONITOR, 2=WINDOW
static PortalScreenCastResult openPortalScreenCast(uint32_t sourceType)
{
  auto& sd = libsdbus::instance();
  if(!sd.available)
    return {};

  sd_bus* bus = nullptr;
  if(sd.open_user(&bus) < 0 || !bus)
  {
    qDebug() << "WindowCapture PipeWire: sd_bus_open_user failed";
    return {};
  }

  PortalScreenCastResult result;
  sd_bus_error error{nullptr, nullptr, 0};

  // ── Step 1: CreateSession ──
  std::string sessionToken = makeToken();
  std::string requestToken = makeToken();
  std::string requestPath = makeRequestPath(sd, bus, requestToken);

  PortalResponse resp{&sd, {}, {}, {}, false};
  sd_bus_slot* slot = nullptr;

  {
    std::string matchRule
        = "type='signal',interface='org.freedesktop.portal.Request'"
          ",member='Response',path='"
          + requestPath + "'";
    sd.add_match(bus, &slot, matchRule.c_str(), onPortalResponse, &resp);
  }

  {
    sd_bus_message* msg = nullptr;
    sd.message_new_method_call(
        bus, &msg, PORTAL_DEST, PORTAL_PATH, PORTAL_SCREENCAST_IFACE,
        "CreateSession");
    sd.message_open_container(msg, 'a', "{sv}");
    appendDictString(sd, msg, "handle_token", requestToken.c_str());
    appendDictString(
        sd, msg, "session_handle_token", sessionToken.c_str());
    sd.message_close_container(msg);

    sd_bus_message* reply = nullptr;
    int r = sd.call(bus, msg, 0, &error, &reply);
    sd.message_unref(msg);
    if(reply)
      sd.message_unref(reply);

    if(r < 0)
    {
      qDebug() << "WindowCapture PipeWire: CreateSession call failed:"
               << (error.message ? error.message : "unknown");
      sd.error_free(&error);
      sd.slot_unref(slot);
      sd.unref(bus);
      return {};
    }
  }

  if(!waitForResponse(sd, bus, resp, 30000) || resp.code != 0)
  {
    qDebug() << "WindowCapture PipeWire: CreateSession rejected or timed out";
    sd.slot_unref(slot);
    sd.unref(bus);
    return {};
  }

  std::string sessionHandle = resp.session_handle;
  sd.slot_unref(slot);
  slot = nullptr;

  if(sessionHandle.empty())
  {
    qDebug() << "WindowCapture PipeWire: no session handle in response";
    sd.unref(bus);
    return {};
  }

  // ── Step 2: SelectSources ──
  requestToken = makeToken();
  requestPath = makeRequestPath(sd, bus, requestToken);
  resp = PortalResponse{&sd, {}, {}, {}, false};

  {
    std::string matchRule
        = "type='signal',interface='org.freedesktop.portal.Request'"
          ",member='Response',path='"
          + requestPath + "'";
    sd.add_match(bus, &slot, matchRule.c_str(), onPortalResponse, &resp);
  }

  {
    sd_bus_message* msg = nullptr;
    sd.message_new_method_call(
        bus, &msg, PORTAL_DEST, PORTAL_PATH, PORTAL_SCREENCAST_IFACE,
        "SelectSources");
    sd.message_append_basic(msg, 'o', sessionHandle.c_str());
    sd.message_open_container(msg, 'a', "{sv}");
    appendDictString(sd, msg, "handle_token", requestToken.c_str());
    appendDictUint32(sd, msg, "types", sourceType);
    appendDictBool(sd, msg, "multiple", false);
    appendDictUint32(sd, msg, "cursor_mode", PORTAL_CURSOR_EMBEDDED);
    sd.message_close_container(msg);

    sd_bus_message* reply = nullptr;
    int r = sd.call(bus, msg, 0, &error, &reply);
    sd.message_unref(msg);
    if(reply)
      sd.message_unref(reply);

    if(r < 0)
    {
      qDebug() << "WindowCapture PipeWire: SelectSources call failed:"
               << (error.message ? error.message : "unknown");
      sd.error_free(&error);
      sd.slot_unref(slot);
      sd.unref(bus);
      return {};
    }
  }

  if(!waitForResponse(sd, bus, resp, 60000) || resp.code != 0)
  {
    qDebug() << "WindowCapture PipeWire: SelectSources rejected or timed out";
    sd.slot_unref(slot);
    sd.unref(bus);
    return {};
  }
  sd.slot_unref(slot);
  slot = nullptr;

  // ── Step 3: Start (user confirms the picker) ──
  requestToken = makeToken();
  requestPath = makeRequestPath(sd, bus, requestToken);
  resp = PortalResponse{&sd, {}, {}, {}, false};

  {
    std::string matchRule
        = "type='signal',interface='org.freedesktop.portal.Request'"
          ",member='Response',path='"
          + requestPath + "'";
    sd.add_match(bus, &slot, matchRule.c_str(), onPortalResponse, &resp);
  }

  {
    sd_bus_message* msg = nullptr;
    sd.message_new_method_call(
        bus, &msg, PORTAL_DEST, PORTAL_PATH, PORTAL_SCREENCAST_IFACE,
        "Start");
    sd.message_append_basic(msg, 'o', sessionHandle.c_str());
    sd.message_append_basic(msg, 's', ""); // parent window identifier
    sd.message_open_container(msg, 'a', "{sv}");
    appendDictString(sd, msg, "handle_token", requestToken.c_str());
    sd.message_close_container(msg);

    sd_bus_message* reply = nullptr;
    int r = sd.call(bus, msg, 0, &error, &reply);
    sd.message_unref(msg);
    if(reply)
      sd.message_unref(reply);

    if(r < 0)
    {
      qDebug() << "WindowCapture PipeWire: Start call failed:"
               << (error.message ? error.message : "unknown");
      sd.error_free(&error);
      sd.slot_unref(slot);
      sd.unref(bus);
      return {};
    }
  }

  if(!waitForResponse(sd, bus, resp, 60000) || resp.code != 0)
  {
    qDebug() << "WindowCapture PipeWire: Start rejected or timed out";
    sd.slot_unref(slot);
    sd.unref(bus);
    return {};
  }

  // Extract the PipeWire node ID from the Start response
  result.pipewire_node = resp.pipewire_node;
  qDebug() << "WindowCapture PipeWire: got node id:" << result.pipewire_node;

  sd.slot_unref(slot);
  slot = nullptr;

  // ── Step 4: OpenPipeWireRemote ──
  {
    sd_bus_message* msg = nullptr;
    sd.message_new_method_call(
        bus, &msg, PORTAL_DEST, PORTAL_PATH, PORTAL_SCREENCAST_IFACE,
        "OpenPipeWireRemote");
    sd.message_append_basic(msg, 'o', sessionHandle.c_str());
    sd.message_open_container(msg, 'a', "{sv}");
    sd.message_close_container(msg);

    sd_bus_message* reply = nullptr;
    int r = sd.call(bus, msg, 0, &error, &reply);
    sd.message_unref(msg);

    if(r < 0 || !reply)
    {
      qDebug() << "WindowCapture PipeWire: OpenPipeWireRemote failed:"
               << (error.message ? error.message : "unknown");
      sd.error_free(&error);
      if(reply)
        sd.message_unref(reply);
      sd.unref(bus);
      return {};
    }

    int fd = -1;
    sd.message_read_basic(reply, 'h', &fd);
    sd.message_unref(reply);

    if(fd < 0)
    {
      qDebug() << "WindowCapture PipeWire: invalid fd from OpenPipeWireRemote";
      sd.unref(bus);
      return {};
    }

    // dup() so the fd outlives the bus connection
    result.fd = ::dup(fd);
  }

  sd.unref(bus);
  return result;
}

static bool portalAvailable()
{
  auto& sd = libsdbus::instance();
  if(!sd.available)
    return false;

  sd_bus* bus = nullptr;
  if(sd.open_user(&bus) < 0 || !bus)
    return false;

  // Try to introspect the portal — just create a method call to verify it exists
  sd_bus_message* msg = nullptr;
  int r = sd.message_new_method_call(
      bus, &msg, PORTAL_DEST, PORTAL_PATH,
      "org.freedesktop.DBus.Properties", "Get");
  if(r >= 0 && msg)
  {
    sd.message_append_basic(msg, 's', PORTAL_SCREENCAST_IFACE);
    sd.message_append_basic(msg, 's', "AvailableSourceTypes");

    sd_bus_error error{nullptr, nullptr, 0};
    sd_bus_message* reply = nullptr;
    r = sd.call(bus, msg, 5000000, &error, &reply); // 5s timeout
    sd.message_unref(msg);
    if(reply)
      sd.message_unref(reply);
    sd.error_free(&error);

    sd.unref(bus);
    return r >= 0;
  }

  sd.unref(bus);
  return false;
}

} // anonymous namespace

// ─── PipeWire Window Capture Backend ─────────────────────────────────────────

class PipeWireWindowCaptureBackend final : public WindowCaptureBackend
{
public:
  ~PipeWireWindowCaptureBackend() override { stop(); }

  bool available() const override
  {
    auto& pw = libremidi::pipewire::load();
    return pw.stream_available && pw.context_connect_fd && portalAvailable();
  }

  bool supportsMode(CaptureMode mode) const override
  {
    switch(mode)
    {
      case CaptureMode::Window:
      case CaptureMode::SingleScreen:
      case CaptureMode::AllScreens:
        return true;
      case CaptureMode::Region:
        return false;
    }
    return false;
  }

  std::vector<CapturableWindow> enumerate() override
  {
    // Wayland does not allow window enumeration.
    // The portal picker dialog handles selection during start().
    return {};
  }

  std::vector<CapturableScreen> enumerateScreens() override
  {
    // Wayland does not allow screen enumeration.
    // The portal picker dialog handles selection during start().
    return {};
  }

  bool start(const CaptureTarget& target) override
  {
    stop();

    auto& pw = libremidi::pipewire::load();
    if(!pw.stream_available || !pw.context_connect_fd)
      return false;

    // Choose portal source type based on capture mode
    uint32_t sourceType = PORTAL_SOURCE_WINDOW;
    switch(target.mode)
    {
      case CaptureMode::Window:
        sourceType = PORTAL_SOURCE_WINDOW;
        break;
      case CaptureMode::AllScreens:
      case CaptureMode::SingleScreen:
        sourceType = PORTAL_SOURCE_MONITOR;
        break;
      case CaptureMode::Region:
        // Region not supported on PipeWire
        return false;
    }

    // Run the portal flow to get a PipeWire fd and node ID.
    auto portalResult = openPortalScreenCast(sourceType);
    if(portalResult.fd < 0)
    {
      qDebug() << "WindowCapture PipeWire: portal flow failed";
      return false;
    }
    m_pipewireNode = portalResult.pipewire_node;

    // Build a per-session pipewire context attached to the portal's
    // socket fd. Different from the process-wide shared_context() —
    // portal sessions get a private connection scoped to the
    // screencast permission grant. The context takes ownership of
    // the fd and closes it on destruction.
    libremidi::pipewire::context_config cfg{};
    cfg.kind = libremidi::pipewire::loop_kind::thread;
    cfg.loop_name = "score-wincap";
    cfg.fd = portalResult.fd;
    cfg.auto_reconnect = false; // Portal session is one-shot; if the
                                // socket dies, the permission is gone.

    m_pwContext = libremidi::pipewire::context::make(
        libremidi::pipewire::shared_instance(), cfg);
    if(!m_pwContext || !m_pwContext->ok())
    {
      qDebug()
          << "WindowCapture PipeWire: context attach to portal fd failed";
      m_pwContext.reset();
      return false;
    }

    // Watch for the portal revoking the screencast permission (user
    // hits "Stop sharing" or the source closes). The daemon takes
    // away our node id when that happens; we flip m_running so the
    // next onProcess bails and frames stop being produced.
    m_pwSubs.clear();
    const std::uint32_t targetNodeId = m_pipewireNode;
    m_pwSubs.push_back(m_pwContext->on_node_removed(
        [this, targetNodeId](std::uint32_t removed_id) {
      if(removed_id == targetNodeId)
      {
        qDebug() << "WindowCapture PipeWire: portal source revoked";
        m_running.store(false, std::memory_order_release);
      }
    }));
    m_pwSubs.push_back(m_pwContext->on_state_changed(
        [](libremidi::pipewire::connection_state s) {
      using cs = libremidi::pipewire::connection_state;
      const char* name = (s == cs::connected)    ? "connected"
                         : (s == cs::connecting) ? "connecting"
                         : (s == cs::broken)     ? "broken"
                                                 : "disconnected";
      qDebug() << "WindowCapture PipeWire: portal context ->" << name;
    }));

    // Stream lifecycle under the thread-loop lock. The new shared
    // context drives its own thread; we just create + connect the
    // stream and stash the listener.
    int connect_rc = 0;
    m_pwContext->with_lock([&] {
      pw_properties* props = pw.properties_new(
          PW_KEY_MEDIA_TYPE, "Video", PW_KEY_MEDIA_CATEGORY, "Capture",
          PW_KEY_MEDIA_ROLE, "Screen", nullptr);

      m_stream = pw.stream_new(
          m_pwContext->pw_core_ptr(), "score-window-capture", props);
      if(!m_stream)
      {
        qDebug() << "WindowCapture PipeWire: pw_stream_new failed";
        return;
      }

      m_streamEvents = {};
      m_streamEvents.version = PW_VERSION_STREAM_EVENTS;
      m_streamEvents.state_changed
          = &PipeWireWindowCaptureBackend::onStateChanged;
      m_streamEvents.param_changed
          = &PipeWireWindowCaptureBackend::onParamChanged;
      m_streamEvents.process
          = &PipeWireWindowCaptureBackend::onProcess;

      m_streamListener = {};
      pw.stream_add_listener(
          m_stream, &m_streamListener, &m_streamEvents, this);

      // Build EnumFormat into a scratch buffer; lifetime is fine as
      // pipewire copies the pod during stream_connect.
      uint8_t pod_buf[1024];
      const spa_pod* params[1] = {
          buildEnumFormatPod(pod_buf, sizeof(pod_buf))};

      connect_rc = pw.stream_connect(
          m_stream, PW_DIRECTION_INPUT, m_pipewireNode,
          (pw_stream_flags)(
              PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
          params, 1);
    });

    if(!m_stream || connect_rc < 0)
    {
      qDebug() << "WindowCapture PipeWire: pw_stream_connect failed:"
               << connect_rc;
      stop();
      return false;
    }

    m_running.store(true, std::memory_order_release);
    return true;
  }

  void stop() override
  {
    m_running.store(false, std::memory_order_release);

    auto& pw = libremidi::pipewire::load();

    // Subscriptions hold weak refs to m_pwContext; release before
    // dropping the context so the dtor path doesn't double-dispatch.
    m_pwSubs.clear();

    // Tear down our stream under the thread-loop lock BEFORE releasing
    // the context. The shared context's own destructor handles the
    // remaining proxy/core/loop teardown in the right order (locked +
    // pre-stop), matching the pattern from libremidi MIDI fix.
    if(m_pwContext && m_stream)
    {
      m_pwContext->with_lock([&] {
        if(pw.stream_disconnect)
          pw.stream_disconnect(m_stream);
        if(pw.stream_destroy)
          pw.stream_destroy(m_stream);
        m_stream = nullptr;
      });
    }

    // Releasing the shared_ptr triggers context::~context, which:
    //   - acquires the thread-loop lock
    //   - tears down registry/core/context proxies
    //   - releases the lock, stops + destroys the thread_loop
    // No "wrong context" errors because everything proxy-shaped is
    // destroyed while the loop is still alive (we patched this in
    // libremidi earlier).
    m_pwContext.reset();

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
    (void)old_state;
    (void)data;

    static constexpr const char* stateNames[]
        = {"error", "unconnected", "connecting", "paused", "streaming"};
    int idx = static_cast<int>(state) + 1;
    if(idx >= 0 && idx < 5)
      qDebug() << "WindowCapture PipeWire: stream state:"
               << stateNames[idx];
    if(error)
      qDebug() << "WindowCapture PipeWire: stream error:" << error;
  }

  static void
  onParamChanged(void* data, uint32_t id, const spa_pod* param)
  {
    if(!param || id != SPA_PARAM_Format)
      return;

    auto* self = static_cast<PipeWireWindowCaptureBackend*>(data);

    // Real spa_format_video_raw_parse from <spa/param/video/format-utils.h>
    // is a static-inline that walks the object via SPA's iteration
    // macros; no extern symbol, no DT_NEEDED.
    spa_video_info_raw info{};
    if(spa_format_video_raw_parse(param, &info) < 0)
      return;

    self->m_spaVideoFormat = info.format;
    self->m_negotiatedWidth = info.size.width;
    self->m_negotiatedHeight = info.size.height;
    qDebug() << "WindowCapture PipeWire: negotiated"
             << info.size.width << "x" << info.size.height
             << "format:" << info.format;
  }

  static void onProcess(void* data)
  {
    auto* self = static_cast<PipeWireWindowCaptureBackend*>(data);
    auto& pw = libremidi::pipewire::load();
    if(!pw.stream_available || !self->m_stream)
      return;

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

  std::shared_ptr<libremidi::pipewire::context> m_pwContext;
  pw_stream* m_stream{};
  pw_stream_events m_streamEvents{};
  spa_hook m_streamListener{};
  uint32_t m_pipewireNode{0};
  std::atomic<bool> m_running{false};

  std::vector<libremidi::pipewire::subscription> m_pwSubs;

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

#else // !SCORE_HAS_PIPEWIRE_VIDEO_IO

namespace Gfx::WindowCapture
{
std::unique_ptr<WindowCaptureBackend> createX11Backend();

std::unique_ptr<WindowCaptureBackend> createWindowCaptureBackend()
{
  return createX11Backend();
}
} // namespace Gfx::WindowCapture

#endif // SCORE_HAS_PIPEWIRE_VIDEO_IO
