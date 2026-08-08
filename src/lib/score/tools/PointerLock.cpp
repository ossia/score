#include <score/tools/PointerLock.hpp>

#if !defined(__APPLE__)

#include <QWindow>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

// Qt puts its windows in a shadow tree and matches events on composedPath()[0];
// anything else we lock makes QWasmWindow::processPointer drop every event for
// the duration of the lock, release included.
// clang-format off
EM_JS(void, score_pointerlock_install, (), {
  if (Module.scorePointerLock)
    return;
  const st = { target: null, x: 0, y: 0, owned: false, scale: 1,
               lastX: null, lastY: 0, css: 0, mov: 0 };
  Module.scorePointerLock = st;
  // Only the press decides what gets locked: a move landing on another element
  // would otherwise retarget the lock and make Qt drop the release.
  document.addEventListener('pointerdown', (e) => {
    if (e.button !== 0)
      return;
    st.target = e.composedPath()[0] || e.target;
    st.x = e.clientX;
    st.y = e.clientY;
  }, true);
  // Only our own lock: a canvas rendering a 3D scene may hold one of its own.
  document.addEventListener('pointerup', (e) => {
    if (e.button === 0 && st.owned && document.pointerLockElement)
      document.exitPointerLock();
  }, true);
  document.addEventListener('pointerlockchange', () => {
    if (!document.pointerLockElement)
      st.owned = false;
  }, true);
  // movementX/Y are not in CSS pixels in every browser: measure them against
  // clientX/Y, which are, while the pointer is free.
  document.addEventListener('mousemove', (e) => {
    if (document.pointerLockElement) {
      st.lastX = null;
      return;
    }
    if (st.lastX !== null) {
      const css = Math.abs(e.clientX - st.lastX) + Math.abs(e.clientY - st.lastY);
      const mov = Math.abs(e.movementX) + Math.abs(e.movementY);
      if (css > 0 && mov > 0) {
        st.css += css;
        st.mov += mov;
      }
      if (st.css > 400) {
        const s = st.css / st.mov;
        if (s > 0.05 && s < 20)
          st.scale = s;
        st.css = 0;
        st.mov = 0;
      }
    }
    st.lastX = e.clientX;
    st.lastY = e.clientY;
  }, true);
});

EM_JS(double, score_pointerlock_scale, (), {
  const st = Module.scorePointerLock;
  return st && st.scale > 0 ? st.scale : 1;
});

EM_JS(void, score_pointerlock_disown, (), {
  const st = Module.scorePointerLock;
  if (st)
    st.owned = false;
});

EM_JS(int, score_pointerlock_request, (), {
  const st = Module.scorePointerLock;
  let el = st ? st.target : null;
  if (!el || !el.isConnected) {
    el = document.elementFromPoint(st ? st.x : 0, st ? st.y : 0);
    while (el && el.shadowRoot) {
      const inner = el.shadowRoot.elementFromPoint(st.x, st.y);
      if (!inner || inner === el)
        break;
      el = inner;
    }
  }
  if (!el || !el.requestPointerLock)
    return 0;
  try {
    const p = el.requestPointerLock();
    // Refusal (no user activation, element gone) also raises pointerlockerror,
    // which is what actually drives the state; this only silences the rejection.
    if (p && p.catch)
      p.catch(() => {});
  } catch (e) {
    return 0;
  }
  if (st)
    st.owned = true;
  return 1;
});
// clang-format on

namespace
{
struct PointerLockListeners
{
  PointerLockListeners() { score_pointerlock_install(); }
};

const PointerLockListeners g_listeners;
}

namespace score
{
namespace
{
enum class LockState
{
  Idle,
  Requested,
  Locked,
  Lost
};

LockState g_state{LockState::Idle};
PointerLock::MotionCallback g_callback{};
PointerLock::ReleaseCallback g_release{};
double g_dx{};
double g_dy{};
double g_scale{1.};

bool on_mousemove(int, const EmscriptenMouseEvent* e, void*)
{
  if(g_state != LockState::Locked)
    return false;

  const QPointF delta{e->movementX * g_scale, e->movementY * g_scale};
  g_dx += delta.x();
  g_dy += delta.y();
  if(g_callback)
    g_callback(delta);
  return false;
}

bool on_mouseup(int, const EmscriptenMouseEvent* e, void*)
{
  if(e->button == 0 && g_state != LockState::Idle && g_release)
    g_release();
  return false;
}

bool on_lockchange(int, const EmscriptenPointerlockChangeEvent* e, void*)
{
  if(e->isActive)
    g_state = LockState::Locked;
  else if(g_state != LockState::Idle)
    g_state = LockState::Lost;
  return false;
}

bool on_lockerror(int, const void*, void*)
{
  if(g_state == LockState::Requested)
    g_state = LockState::Lost;
  return false;
}
}

bool PointerLock::beginRelative(
    QWindow*, MotionCallback onMotion, ReleaseCallback onRelease) noexcept
{
  if(g_state == LockState::Requested || g_state == LockState::Locked)
    return true;

  static const bool listeners = [] {
    emscripten_set_mousemove_callback(
        EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, true, &on_mousemove);
    emscripten_set_mouseup_callback(
        EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, true, &on_mouseup);
    emscripten_set_pointerlockchange_callback(
        EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, true, &on_lockchange);
    emscripten_set_pointerlockerror_callback(
        EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, true, &on_lockerror);
    return true;
  }();
  (void)listeners;

  if(!score_pointerlock_request())
    return false;

  g_scale = score_pointerlock_scale();
  g_callback = onMotion;
  g_release = onRelease;
  g_dx = 0.;
  g_dy = 0.;
  g_state = LockState::Requested;
  return true;
}

bool PointerLock::active() noexcept
{
  // Only once the browser has actually granted the lock: the request is
  // asynchronous and may be refused, and no motion is delivered until then.
  return g_state == LockState::Locked;
}

QPointF PointerLock::takeDelta() noexcept
{
  const QPointF d{g_dx, g_dy};
  g_dx = 0.;
  g_dy = 0.;
  return d;
}

void PointerLock::endRelative() noexcept
{
  if(g_state == LockState::Idle)
    return;

  g_state = LockState::Idle;
  g_callback = nullptr;
  g_release = nullptr;
  g_dx = 0.;
  g_dy = 0.;
  score_pointerlock_disown();
  emscripten_exit_pointerlock();
}
}

#elif defined(_WIN32)
#include <QAbstractNativeEventFilter>
#include <QByteArray>
#include <QGuiApplication>

#include <windows.h>

#include <algorithm>
#include <vector>

namespace score
{
namespace
{
double g_dx{};
double g_dy{};
PointerLock::MotionCallback g_callback{};
PointerLock::ReleaseCallback g_release{};
HWND g_window{};

// Raw counts are what the mouse hardware reports, before the system applies
// its pointer speed to them; the scroller works in the logical pixels the
// other backends deliver.
double g_countScale{1.};
double g_pixelScale{1.};
bool g_haveAbsolute{};
QPointF g_lastAbsolute{};

double pointerSpeedFactor() noexcept
{
  // The multipliers the pointer-speed slider selects, per "Pointer Ballistics
  // for Windows XP"; index 10 (1.0) is the default.
  static constexpr double factors[]
      = {1. / 32., 1. / 16., 1. / 8., 2. / 8., 3. / 8., 4. / 8., 5. / 8.,
         6. / 8.,  7. / 8.,  1.,      1.25,    1.5,     1.75,    2.,
         2.25,     2.5,      2.75,    3.,      3.25,    3.5};

  int speed = 10;
  if(!SystemParametersInfoW(SPI_GETMOUSESPEED, 0, &speed, 0))
    speed = 10;
  return factors[std::clamp(speed, 1, 20) - 1];
}

QPointF absolutePosition(const RAWMOUSE& mouse) noexcept
{
  const bool virtualDesktop = mouse.usFlags & MOUSE_VIRTUAL_DESKTOP;
  const double left = virtualDesktop ? GetSystemMetrics(SM_XVIRTUALSCREEN) : 0;
  const double top = virtualDesktop ? GetSystemMetrics(SM_YVIRTUALSCREEN) : 0;
  const double width
      = GetSystemMetrics(virtualDesktop ? SM_CXVIRTUALSCREEN : SM_CXSCREEN);
  const double height
      = GetSystemMetrics(virtualDesktop ? SM_CYVIRTUALSCREEN : SM_CYSCREEN);

  return {
      left + (mouse.lLastX / 65535.) * width, top + (mouse.lLastY / 65535.) * height};
}

struct RawMouseFilter final : public QAbstractNativeEventFilter
{
  bool nativeEventFilter(const QByteArray&, void* message, qintptr*) override
  {
    auto* msg = static_cast<MSG*>(message);
    if(msg->message != WM_INPUT || !g_window)
      return false;

    UINT size = 0;
    if(GetRawInputData(
           (HRAWINPUT)msg->lParam, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER))
       != 0)
      return false;

    m_buffer.resize(size);
    if(GetRawInputData(
           (HRAWINPUT)msg->lParam, RID_INPUT, m_buffer.data(), &size,
           sizeof(RAWINPUTHEADER))
       != size)
      return false;

    const auto& raw = *reinterpret_cast<const RAWINPUT*>(m_buffer.data());
    if(raw.header.dwType != RIM_TYPEMOUSE)
      return false;

    const auto& mouse = raw.data.mouse;
    QPointF delta{};
    bool moved = false;

    // Absolute reporting is what RDP negotiates down to and what tablets and
    // virtual-machine pointing devices use: difference the samples instead of
    // dropping them, or the control is dead for the whole drag.
    if(mouse.usFlags & MOUSE_MOVE_ABSOLUTE)
    {
      const QPointF pos = absolutePosition(mouse);
      if(g_haveAbsolute && pos != g_lastAbsolute)
      {
        delta = (pos - g_lastAbsolute) * g_pixelScale;
        moved = true;
      }
      g_lastAbsolute = pos;
      g_haveAbsolute = true;
    }
    else if(mouse.lLastX != 0 || mouse.lLastY != 0)
    {
      delta = QPointF(mouse.lLastX, mouse.lLastY) * g_countScale;
      moved = true;
    }

    if(moved)
    {
      g_dx += delta.x();
      g_dy += delta.y();
      if(g_callback)
        g_callback(delta);
    }

    if((mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP) && g_release)
      g_release();
    return false;
  }

private:
  std::vector<char> m_buffer;
};

RawMouseFilter g_filter;
}

bool PointerLock::beginRelative(
    QWindow* window, MotionCallback onMotion, ReleaseCallback onRelease) noexcept
{
  if(g_window)
    return true;
  if(!window)
    return false;

  auto hwnd = (HWND)window->winId();
  if(!hwnd)
    return false;

  RAWINPUTDEVICE rid{};
  rid.usUsagePage = 0x01;
  rid.usUsage = 0x02;
  rid.dwFlags = RIDEV_INPUTSINK;
  rid.hwndTarget = hwnd;
  if(!RegisterRawInputDevices(&rid, 1, sizeof(rid)))
    return false;

  static const bool quitHandler = [] {
    QObject::connect(
        qGuiApp, &QCoreApplication::aboutToQuit, qGuiApp, [] { endRelative(); });
    return true;
  }();
  Q_UNUSED(quitHandler);

  const double dpr = window->devicePixelRatio();
  g_pixelScale = dpr > 0. ? 1. / dpr : 1.;
  g_countScale = pointerSpeedFactor() * g_pixelScale;

  g_haveAbsolute = false;
  g_lastAbsolute = {};
  g_dx = 0.;
  g_dy = 0.;
  g_callback = onMotion;
  g_release = onRelease;
  g_window = hwnd;
  qGuiApp->installNativeEventFilter(&g_filter);

  POINT p{};
  if(GetCursorPos(&p))
  {
    const RECT r{p.x, p.y, p.x + 1, p.y + 1};
    ClipCursor(&r);
  }
  return true;
}

bool PointerLock::active() noexcept
{
  return g_window != nullptr;
}

QPointF PointerLock::takeDelta() noexcept
{
  const QPointF d{g_dx, g_dy};
  g_dx = 0.;
  g_dy = 0.;
  return d;
}

void PointerLock::endRelative() noexcept
{
  if(!g_window)
    return;

  ClipCursor(nullptr);
  qGuiApp->removeNativeEventFilter(&g_filter);

  RAWINPUTDEVICE rid{};
  rid.usUsagePage = 0x01;
  rid.usUsage = 0x02;
  rid.dwFlags = RIDEV_REMOVE;
  rid.hwndTarget = nullptr;
  RegisterRawInputDevices(&rid, 1, sizeof(rid));

  g_window = nullptr;
  g_callback = nullptr;
  g_release = nullptr;
  g_haveAbsolute = false;
  g_dx = 0.;
  g_dy = 0.;
}
}

#elif defined(SCORE_HAS_WAYLAND_POINTER_LOCK) || defined(SCORE_HAS_X11_POINTER_LOCK)
#include <QGuiApplication>
#include <QWindow>
#include <qpa/qplatformnativeinterface.h>

#include <cstring>
#include <vector>

#if defined(SCORE_HAS_WAYLAND_POINTER_LOCK)
#include <pointer-constraints-unstable-v1-client-protocol.h>
#include <relative-pointer-unstable-v1-client-protocol.h>
#include <wayland-client.h>
#endif

#if defined(SCORE_HAS_X11_POINTER_LOCK)
#include <score/tools/PointerLockX11.hpp>
#endif

namespace score
{
namespace
{
#if defined(SCORE_HAS_WAYLAND_POINTER_LOCK)
namespace wayland
{
struct WaylandGlobals
{
  wl_display* display{};
  wl_registry* registry{};
  zwp_relative_pointer_manager_v1* relative_manager{};
  uint32_t relative_manager_name{};
  zwp_pointer_constraints_v1* constraints{};
  uint32_t constraints_name{};
  std::vector<uint32_t> seats;
  bool tried{};
};

WaylandGlobals g_wl;
zwp_relative_pointer_v1* g_relative{};
zwp_locked_pointer_v1* g_locked{};
bool g_lockActive{};
PointerLock::MotionCallback g_callback{};
PointerLock::ReleaseCallback g_release{};
double g_dx{};
double g_dy{};

void registry_global(
    void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t)
{
  auto& g = *static_cast<WaylandGlobals*>(data);
  if(std::strcmp(interface, zwp_relative_pointer_manager_v1_interface.name) == 0)
  {
    g.relative_manager = static_cast<zwp_relative_pointer_manager_v1*>(
        wl_registry_bind(registry, name, &zwp_relative_pointer_manager_v1_interface, 1));
    g.relative_manager_name = name;
  }
  else if(std::strcmp(interface, zwp_pointer_constraints_v1_interface.name) == 0)
  {
    g.constraints = static_cast<zwp_pointer_constraints_v1*>(
        wl_registry_bind(registry, name, &zwp_pointer_constraints_v1_interface, 1));
    g.constraints_name = name;
  }
  else if(std::strcmp(interface, wl_seat_interface.name) == 0)
  {
    g.seats.push_back(name);
  }
}

void registry_global_remove(void* data, wl_registry*, uint32_t name)
{
  auto& g = *static_cast<WaylandGlobals*>(data);
  std::erase(g.seats, name);

  if(g.relative_manager && name == g.relative_manager_name)
  {
    PointerLock::endRelative();
    zwp_relative_pointer_manager_v1_destroy(g.relative_manager);
    g.relative_manager = nullptr;
  }
  if(g.constraints && name == g.constraints_name)
  {
    PointerLock::endRelative();
    zwp_pointer_constraints_v1_destroy(g.constraints);
    g.constraints = nullptr;
  }
}

const wl_registry_listener registry_listener{registry_global, registry_global_remove};

void relative_motion(
    void*, zwp_relative_pointer_v1*, uint32_t, uint32_t, wl_fixed_t dx, wl_fixed_t dy,
    wl_fixed_t, wl_fixed_t)
{
  const QPointF delta{wl_fixed_to_double(dx), wl_fixed_to_double(dy)};
  g_dx += delta.x();
  g_dy += delta.y();
  if(g_callback)
    g_callback(delta);
}

const zwp_relative_pointer_v1_listener relative_listener{relative_motion};

void pointer_locked(void*, zwp_locked_pointer_v1*)
{
  g_lockActive = true;
  g_dx = 0.;
  g_dy = 0.;
}

void pointer_unlocked(void*, zwp_locked_pointer_v1*)
{
  g_lockActive = false;
}

const zwp_locked_pointer_v1_listener locked_listener{pointer_locked, pointer_unlocked};

WaylandGlobals* globals()
{
  if(!g_wl.tried)
  {
    g_wl.tried = true;

    if(!qGuiApp->platformName().startsWith(QStringLiteral("wayland")))
      return nullptr;

    auto* ni = qGuiApp->platformNativeInterface();
    if(!ni)
      return nullptr;

    g_wl.display
        = static_cast<wl_display*>(ni->nativeResourceForIntegration("wl_display"));
    if(!g_wl.display)
      return nullptr;

    auto* queue = wl_display_create_queue(g_wl.display);
    g_wl.registry = wl_display_get_registry(g_wl.display);
    wl_proxy_set_queue((wl_proxy*)g_wl.registry, queue);
    wl_registry_add_listener(g_wl.registry, &registry_listener, &g_wl);
    wl_display_roundtrip_queue(g_wl.display, queue);

    wl_proxy_set_queue((wl_proxy*)g_wl.registry, nullptr);
    if(g_wl.relative_manager)
      wl_proxy_set_queue((wl_proxy*)g_wl.relative_manager, nullptr);
    if(g_wl.constraints)
      wl_proxy_set_queue((wl_proxy*)g_wl.constraints, nullptr);
    wl_event_queue_destroy(queue);
  }

  return g_wl.constraints && g_wl.relative_manager ? &g_wl : nullptr;
}

wl_pointer* currentPointer(const WaylandGlobals& g) noexcept
{
  if(g.seats.empty())
    return nullptr;

  return static_cast<wl_pointer*>(
      qGuiApp->platformNativeInterface()->nativeResourceForIntegration("wl_pointer"));
}

bool begin(
    QWindow* window, PointerLock::MotionCallback onMotion,
    PointerLock::ReleaseCallback onRelease) noexcept
{
  if(g_relative)
    return true;
  if(!window)
    return false;

  auto* g = globals();
  if(!g)
    return false;

  auto* pointer = currentPointer(*g);
  if(!pointer)
    return false;

  auto* surface = static_cast<wl_surface*>(
      qGuiApp->platformNativeInterface()->nativeResourceForWindow("surface", window));
  if(!surface)
    return false;

  static const bool quitHandler = [] {
    QObject::connect(qGuiApp, &QCoreApplication::aboutToQuit, qGuiApp, [] {
      PointerLock::endRelative();
    });
    return true;
  }();
  Q_UNUSED(quitHandler);

  g_relative = zwp_relative_pointer_manager_v1_get_relative_pointer(
      g->relative_manager, pointer);
  if(!g_relative)
    return false;

  zwp_relative_pointer_v1_add_listener(g_relative, &relative_listener, nullptr);

  g_lockActive = false;
  g_locked = zwp_pointer_constraints_v1_lock_pointer(
      g->constraints, surface, pointer, nullptr,
      ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
  if(g_locked)
    zwp_locked_pointer_v1_add_listener(g_locked, &locked_listener, nullptr);

  g_callback = onMotion;
  g_release = onRelease;
  g_dx = 0.;
  g_dy = 0.;
  wl_display_flush(g->display);
  return true;
}

bool active() noexcept
{
  return g_relative && g_locked && g_lockActive;
}

QPointF takeDelta() noexcept
{
  const QPointF d{g_dx, g_dy};
  g_dx = 0.;
  g_dy = 0.;
  return d;
}

void end() noexcept
{
  if(g_locked)
  {
    zwp_locked_pointer_v1_destroy(g_locked);
    g_locked = nullptr;
  }
  if(g_relative)
  {
    zwp_relative_pointer_v1_destroy(g_relative);
    g_relative = nullptr;
  }
  if(g_wl.display)
    wl_display_flush(g_wl.display);

  g_lockActive = false;
  g_callback = nullptr;
  g_release = nullptr;
  g_dx = 0.;
  g_dy = 0.;
}
}
#endif


enum class Backend
{
  Unsupported,
  Wayland,
  X11
};

Backend currentBackend() noexcept
{
  // SCORE_NO_POINTER_LOCK=1 refuses every lock, so the controls go back to
  // reading pointer positions the way they did before there was a backend.
  static const bool disabled = qEnvironmentVariableIsSet("SCORE_NO_POINTER_LOCK");
  if(disabled)
    return Backend::Unsupported;

  const auto platform = qGuiApp->platformName();
#if defined(SCORE_HAS_WAYLAND_POINTER_LOCK)
  if(platform.startsWith(QStringLiteral("wayland")))
    return Backend::Wayland;
#endif
#if defined(SCORE_HAS_X11_POINTER_LOCK)
  if(platform == QStringLiteral("xcb"))
    return Backend::X11;
#endif
  return Backend::Unsupported;
}
}

bool PointerLock::beginRelative(
    QWindow* window, MotionCallback onMotion, ReleaseCallback onRelease) noexcept
{
  switch(currentBackend())
  {
#if defined(SCORE_HAS_WAYLAND_POINTER_LOCK)
    case Backend::Wayland:
      return wayland::begin(window, onMotion, onRelease);
#endif
#if defined(SCORE_HAS_X11_POINTER_LOCK)
    case Backend::X11:
      return x11::begin(window, onMotion, onRelease);
#endif
    default:
      return false;
  }
}

bool PointerLock::active() noexcept
{
  switch(currentBackend())
  {
#if defined(SCORE_HAS_WAYLAND_POINTER_LOCK)
    case Backend::Wayland:
      return wayland::active();
#endif
#if defined(SCORE_HAS_X11_POINTER_LOCK)
    case Backend::X11:
      return x11::active();
#endif
    default:
      return false;
  }
}

QPointF PointerLock::takeDelta() noexcept
{
  switch(currentBackend())
  {
#if defined(SCORE_HAS_WAYLAND_POINTER_LOCK)
    case Backend::Wayland:
      return wayland::takeDelta();
#endif
#if defined(SCORE_HAS_X11_POINTER_LOCK)
    case Backend::X11:
      return x11::takeDelta();
#endif
    default:
      return {};
  }
}

void PointerLock::endRelative() noexcept
{
#if defined(SCORE_HAS_WAYLAND_POINTER_LOCK)
  wayland::end();
#endif
#if defined(SCORE_HAS_X11_POINTER_LOCK)
  x11::end();
#endif
}
}

#else

namespace score
{
bool PointerLock::beginRelative(QWindow*, MotionCallback, ReleaseCallback) noexcept
{
  return false;
}

bool PointerLock::active() noexcept
{
  return false;
}

QPointF PointerLock::takeDelta() noexcept
{
  return {};
}

void PointerLock::endRelative() noexcept { }
}

#endif
#endif
