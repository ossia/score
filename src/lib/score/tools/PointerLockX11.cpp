#include <score/tools/PointerLockX11.hpp>

#include <QAbstractNativeEventFilter>
#include <QByteArray>
#include <QGuiApplication>
#include <QWindow>
#include <qpa/qplatformnativeinterface.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <vector>

#include <xcb/xcb.h>
#include <xcb/xinput.h>

namespace score::x11
{
namespace
{
// Raw XInput2 motion is the only usable delta source here: it keeps coming once
// the pointer is pinned, and it survives an input source that reports positions
// instead of movements. Some setups never deliver it, so the lock only reports
// itself usable once one has actually arrived -- until then the scroller reads
// pointer positions and wraps them at the screen edges, as it always did.
xcb_connection_t* g_conn{};
xcb_window_t g_root{};
uint8_t g_opcode{};
bool g_active{};
int16_t g_anchorX{};
int16_t g_anchorY{};
double g_pixelScale{1.};
PointerLock::MotionCallback g_callback{};
PointerLock::ReleaseCallback g_release{};
double g_dx{};
double g_dy{};
uint32_t g_addedBits{};
bool g_sawRaw{};

xcb_window_t g_confine{XCB_NONE};
xcb_cursor_t g_blank{XCB_NONE};
bool g_grabbed{};
bool g_ungrabNeeded{};

constexpr uint32_t rawEventBits
    = (1u << XCB_INPUT_RAW_MOTION) | (1u << XCB_INPUT_RAW_BUTTON_RELEASE);

constexpr double fixedToDouble(xcb_input_fp3232_t v) noexcept
{
  return double(v.integral) + double(v.frac) / 4294967296.;
}

// Tablets and the pointing devices virtual machines expose report positions
// rather than movements: difference the samples instead of dropping them.
struct DeviceState
{
  xcb_input_device_id_t id{};
  bool absolute{};
  bool classified{};
  bool have{};
  double last[2]{};
  double scale[2]{1., 1.};
};
// deque: deviceState() hands out references that must survive a later insert.
std::deque<DeviceState> g_devices;

DeviceState queryDevice(xcb_input_device_id_t id)
{
  DeviceState state{.id = id};

  auto* reply = xcb_input_xi_query_device_reply(
      g_conn, xcb_input_xi_query_device(g_conn, id), nullptr);
  if(!reply)
    return state;

  const auto* screen = xcb_setup_roots_iterator(xcb_get_setup(g_conn)).data;
  const double extent[2]{
      screen ? double(screen->width_in_pixels) : 1.,
      screen ? double(screen->height_in_pixels) : 1.};

  for(auto devices = xcb_input_xi_query_device_infos_iterator(reply); devices.rem;
      xcb_input_xi_device_info_next(&devices))
  {
    for(auto classes = xcb_input_xi_device_info_classes_iterator(devices.data);
        classes.rem; xcb_input_device_class_next(&classes))
    {
      if(classes.data->type != XCB_INPUT_DEVICE_CLASS_TYPE_VALUATOR)
        continue;

      const auto& valuator
          = *reinterpret_cast<const xcb_input_valuator_class_t*>(classes.data);
      if(valuator.number > 1 || valuator.mode != XCB_INPUT_VALUATOR_MODE_ABSOLUTE)
        continue;

      state.absolute = true;
      state.classified = true;
      const double min = fixedToDouble(valuator.min);
      const double max = fixedToDouble(valuator.max);
      if(max > min)
        state.scale[valuator.number] = extent[valuator.number] / (max - min);
    }
  }

  free(reply);
  return state;
}

// Injected motion -- what remote-desktop tools produce by driving XTest with
// absolute coordinates -- reports screen positions through a device that
// declares itself relative, so the device class alone does not say how to read
// a sample. Compare the first one against where the pointer actually is.
void classify(DeviceState& dev, const double (&axis)[2], const bool (&have)[2])
{
  dev.classified = true;
  if(!have[0] || !have[1])
    return;
  if(std::abs(axis[0]) <= 64. && std::abs(axis[1]) <= 64.)
    return;

  auto* pointer
      = xcb_query_pointer_reply(g_conn, xcb_query_pointer(g_conn, g_root), nullptr);
  if(!pointer)
    return;

  if(std::abs(axis[0] - pointer->root_x) <= 64.
     && std::abs(axis[1] - pointer->root_y) <= 64.)
  {
    dev.absolute = true;
    dev.scale[0] = 1.;
    dev.scale[1] = 1.;
  }
  free(pointer);
}

DeviceState& deviceState(xcb_input_device_id_t id)
{
  for(auto& d : g_devices)
    if(d.id == id)
      return d;

  g_devices.push_back(queryDevice(id));
  return g_devices.back();
}

void warpToAnchor()
{
  xcb_warp_pointer(g_conn, XCB_NONE, g_root, 0, 0, 0, 0, g_anchorX, g_anchorY);
  xcb_flush(g_conn);
}

xcb_grab_status_t grabPointer(xcb_window_t confineTo)
{
  auto* reply = xcb_grab_pointer_reply(
      g_conn,
      xcb_grab_pointer(
          g_conn, 1, g_root,
          XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_BUTTON_RELEASE
              | XCB_EVENT_MASK_BUTTON_PRESS,
          XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, confineTo, g_blank,
          XCB_CURRENT_TIME),
      nullptr);
  if(!reply)
    return XCB_GRAB_STATUS_NOT_VIEWABLE;

  const auto status = xcb_grab_status_t(reply->status);
  free(reply);
  return status;
}

// Warping is only advisory -- a device that reports absolute positions (a
// tablet, a remote desktop driving XTest) overrides it on its very next sample.
// Confining the pointer to a single pixel is enforced by the server instead,
// whatever the device reports, and is the counterpart of ClipCursor on Windows.
void pin()
{
  if(g_grabbed)
    return;

  if(g_blank == XCB_NONE)
  {
    const xcb_pixmap_t pixmap = xcb_generate_id(g_conn);
    xcb_create_pixmap(g_conn, 1, pixmap, g_root, 1, 1);
    const xcb_gcontext_t gc = xcb_generate_id(g_conn);
    const uint32_t black = 0;
    xcb_create_gc(g_conn, gc, pixmap, XCB_GC_FOREGROUND, &black);
    const xcb_rectangle_t rect{0, 0, 1, 1};
    xcb_poly_fill_rectangle(g_conn, pixmap, gc, 1, &rect);
    xcb_free_gc(g_conn, gc);

    g_blank = xcb_generate_id(g_conn);
    xcb_create_cursor(g_conn, g_blank, pixmap, pixmap, 0, 0, 0, 0, 0, 0, 0, 0);
    xcb_free_pixmap(g_conn, pixmap);
  }

  g_confine = xcb_generate_id(g_conn);
  const uint32_t overrideRedirect = 1;
  xcb_create_window(
      g_conn, XCB_COPY_FROM_PARENT, g_confine, g_root, g_anchorX, g_anchorY, 1, 1, 0,
      XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, XCB_CW_OVERRIDE_REDIRECT,
      &overrideRedirect);
  xcb_map_window(g_conn, g_confine);

  // owner_events keeps the widgets receiving their events through Qt as usual.
  auto status = grabPointer(g_confine);
  if(status == XCB_GRAB_STATUS_ALREADY_GRABBED)
  {
    // The one the button press left behind, which is ours: the server refuses a
    // second grab while it holds, so drop it and take one that confines.
    xcb_ungrab_pointer(g_conn, XCB_CURRENT_TIME);
    g_ungrabNeeded = true;
    status = grabPointer(g_confine);

    // Nothing holds the pointer now that the implicit grab is gone: take an
    // unconfined one back, or the rest of the drag lands in other windows and
    // the widget never sees its release.
    if(status != XCB_GRAB_STATUS_SUCCESS)
      grabPointer(XCB_NONE);
  }

  g_grabbed = status == XCB_GRAB_STATUS_SUCCESS;

  if(!g_grabbed)
  {
    xcb_destroy_window(g_conn, g_confine);
    g_confine = XCB_NONE;
  }
  xcb_flush(g_conn);
}

void unpin()
{
  if(g_grabbed || g_ungrabNeeded)
  {
    xcb_ungrab_pointer(g_conn, XCB_CURRENT_TIME);
    g_grabbed = false;
    g_ungrabNeeded = false;
  }
  if(g_confine != XCB_NONE)
  {
    xcb_destroy_window(g_conn, g_confine);
    g_confine = XCB_NONE;
  }
}

void report(QPointF delta)
{
  g_dx += delta.x();
  g_dy += delta.y();
  if(g_callback)
    g_callback(delta);
}

void onRawMotion(const xcb_input_raw_motion_event_t& ev)
{
  if(xcb_input_raw_button_press_valuator_mask_length(&ev) < 1)
    return;

  const uint32_t mask = *xcb_input_raw_button_press_valuator_mask(&ev);
  const xcb_input_fp3232_t* values = xcb_input_raw_button_press_axisvalues(&ev);

  // Axes 0 and 1 are the lowest bits, so their samples are the leading entries.
  double axis[2]{};
  bool have[2]{};
  int index = 0;
  for(int i = 0; i < 2; i++)
  {
    if(!(mask & (1u << i)))
      continue;
    axis[i] = fixedToDouble(values[index++]);
    have[i] = true;
  }

  if(!have[0] && !have[1])
    return;

  auto& dev = deviceState(ev.sourceid);
  if(!dev.classified)
    classify(dev, axis, have);

  QPointF delta;
  if(dev.absolute)
  {
    const bool first = !dev.have;
    for(int i = 0; i < 2; i++)
    {
      if(!have[i])
        continue;
      if(!first)
        (i == 0 ? delta.rx() : delta.ry()) = (axis[i] - dev.last[i]) * dev.scale[i];
      dev.last[i] = axis[i];
    }
    dev.have = true;
    if(first)
      return;
  }
  else
  {
    delta = {axis[0], axis[1]};
  }

  if(delta.isNull())
    return;

  // Proof that a delta source exists: only now is it safe to hold the pointer,
  // and only now does active() report the lock as usable.
  if(!g_sawRaw)
  {
    g_sawRaw = true;
    pin();
  }

  // Warping raises no raw event, so this cannot feed back on itself. Kept for
  // when the confine could not be taken; a device reporting absolute positions
  // overrides it, which is exactly what the confine is there to cover.
  if(!g_grabbed)
    warpToAnchor();

  report(delta * g_pixelScale);
}

struct RawFilter final : public QAbstractNativeEventFilter
{
  bool nativeEventFilter(const QByteArray& type, void* message, qintptr*) override
  {
    if(!g_active || type != "xcb_generic_event_t")
      return false;

    auto* ev = static_cast<xcb_generic_event_t*>(message);
    if((ev->response_type & 0x7f) != XCB_GE_GENERIC)
      return false;

    const auto* generic = reinterpret_cast<const xcb_ge_generic_event_t*>(ev);
    if(generic->extension != g_opcode)
      return false;

    if(generic->event_type == XCB_INPUT_RAW_MOTION)
    {
      onRawMotion(*reinterpret_cast<const xcb_input_raw_motion_event_t*>(ev));
    }
    else if(generic->event_type == XCB_INPUT_RAW_BUTTON_RELEASE)
    {
      const auto* button
          = reinterpret_cast<const xcb_input_raw_button_release_event_t*>(ev);
      if(button->detail == 1)
        if(auto* release = g_release)
          release();
    }
    return false;
  }
};

RawFilter g_filter;

std::vector<uint32_t> selectedMask()
{
  std::vector<uint32_t> out;
  auto* reply = xcb_input_xi_get_selected_events_reply(
      g_conn, xcb_input_xi_get_selected_events(g_conn, g_root), nullptr);
  if(!reply)
    return out;

  for(auto it = xcb_input_xi_get_selected_events_masks_iterator(reply); it.rem;
      xcb_input_event_mask_next(&it))
  {
    if(it.data->deviceid != XCB_INPUT_DEVICE_ALL_MASTER)
      continue;

    const uint32_t* mask = xcb_input_event_mask_mask(it.data);
    out.assign(mask, mask + it.data->mask_len);
    break;
  }

  free(reply);
  return out;
}

// Merged into whatever the xcb plugin already selected for itself: this is the
// same X11 client, and a bare selection would drop Qt's own.
bool applyMask(std::vector<uint32_t> words)
{
  if(words.empty())
    words.push_back(0);

  std::vector<uint32_t> request(1 + words.size(), 0);
  auto* header = reinterpret_cast<xcb_input_event_mask_t*>(request.data());
  header->deviceid = XCB_INPUT_DEVICE_ALL_MASTER;
  header->mask_len = words.size();
  std::memcpy(request.data() + 1, words.data(), words.size() * sizeof(uint32_t));

  auto* error = xcb_request_check(
      g_conn, xcb_input_xi_select_events_checked(g_conn, g_root, 1, header));
  if(!error)
    return true;

  free(error);
  return false;
}
}

bool begin(
    QWindow* window, PointerLock::MotionCallback onMotion,
    PointerLock::ReleaseCallback onRelease) noexcept
{
  if(g_active)
    return true;
  if(!window)
    return false;

  auto* ni = qGuiApp->platformNativeInterface();
  if(!ni)
    return false;

  g_conn = static_cast<xcb_connection_t*>(ni->nativeResourceForIntegration("connection"));
  if(!g_conn || xcb_connection_has_error(g_conn))
    return false;

  const auto* extension = xcb_get_extension_data(g_conn, &xcb_input_id);
  if(!extension || !extension->present)
    return false;
  g_opcode = extension->major_opcode;

  const auto* screen = xcb_setup_roots_iterator(xcb_get_setup(g_conn)).data;
  if(!screen)
    return false;
  g_root = screen->root;

  auto* pointer
      = xcb_query_pointer_reply(g_conn, xcb_query_pointer(g_conn, g_root), nullptr);
  if(!pointer)
    return false;
  g_anchorX = pointer->root_x;
  g_anchorY = pointer->root_y;
  free(pointer);

  // SCORE_X11_NO_RAW_POINTER=1 leaves only the warp source, which is what a
  // setup that does not deliver raw events falls back to anyway.
  static const bool wantRaw = !qEnvironmentVariableIsSet("SCORE_X11_NO_RAW_POINTER");
  g_addedBits = 0;
  if(wantRaw)
  {
    auto merged = selectedMask();
    if(merged.empty())
      merged.push_back(0);
    g_addedBits = rawEventBits & ~merged[0];
    merged[0] |= rawEventBits;
    applyMask(merged);
  }

  static const bool filter = [] {
    qGuiApp->installNativeEventFilter(&g_filter);
    QObject::connect(qGuiApp, &QCoreApplication::aboutToQuit, qGuiApp, [] {
      PointerLock::endRelative();
    });
    return true;
  }();
  Q_UNUSED(filter);

  const double dpr = window->devicePixelRatio();
  g_pixelScale = dpr > 0. ? 1. / dpr : 1.;
  g_devices.clear();
  g_dx = 0.;
  g_dy = 0.;
  g_sawRaw = false;
  g_callback = onMotion;
  g_release = onRelease;
  g_active = true;
  return true;
}

// Not merely armed: a lock that cannot hold the pointer is worse than none,
// because the scroller stops wrapping at the screen edges on the strength of
// it. Raw motion having actually arrived is the only proof there is one.
bool active() noexcept
{
  return g_active && g_sawRaw;
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
  if(!g_active)
    return;

  unpin();
  g_active = false;
  g_callback = nullptr;
  g_release = nullptr;
  g_dx = 0.;
  g_dy = 0.;
  g_devices.clear();

  // Drop only the bits this backend added: the plugin may have selected more
  // of its own since, and a snapshot taken at begin() would undo that.
  if(g_addedBits != 0)
  {
    auto mask = selectedMask();
    if(!mask.empty())
    {
      mask[0] &= ~g_addedBits;
      applyMask(mask);
    }
    g_addedBits = 0;
  }
  xcb_flush(g_conn);
}
}
