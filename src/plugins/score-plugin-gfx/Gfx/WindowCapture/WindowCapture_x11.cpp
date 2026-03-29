#include <Gfx/WindowCapture/WindowCaptureBackend.hpp>

#include <ossia/detail/dylib_loader.hpp>

#include <QDebug>

#include <cstdlib>
#include <cstring>
#include <optional>
#include <sys/ipc.h>
#include <sys/shm.h>

// X11 types — forward declared to avoid including X11 headers
extern "C" {
typedef struct _XDisplay Display;
typedef unsigned long XID;
typedef XID Window;
typedef XID Pixmap;
typedef unsigned long Atom;
typedef int Bool;
typedef int Status;

struct XWindowAttributes
{
  int x, y;
  int width, height;
  int border_width;
  int depth;
  void* visual;
  Window root;
  int c_class;
  int bit_gravity, win_gravity;
  int backing_store;
  unsigned long backing_planes, backing_pixel;
  Bool save_under;
  long colormap;
  Bool map_installed;
  int map_state;
  long all_event_masks, your_event_mask, do_not_propagate_mask;
  Bool override_redirect;
  void* screen;
};
// map_state values
#define IsViewable 2

struct XImage
{
  int width, height;
  int xoffset;
  int format;
  char* data;
  int byte_order;
  int bitmap_unit, bitmap_bit_order, bitmap_pad;
  int depth;
  int bytes_per_line;
  int bits_per_pixel;
  unsigned long red_mask, green_mask, blue_mask;
  void* obdata;
  struct funcs
  {
    void* create_image;
    void* destroy_image;
    void* get_pixel;
    void* put_pixel;
    void* sub_image;
    void* add_pixel;
  } f;
};

struct XShmSegmentInfo
{
  long shmseg;
  int shmid;
  char* shmaddr;
  Bool readOnly;
};

// Constants
#define ZPixmap 2
#define AllPlanes (~0UL)
#define AnyPropertyType 0L
#define XA_WINDOW 33L

// Function types we need
using XOpenDisplay_t = Display* (*)(const char*);
using XCloseDisplay_t = int (*)(Display*);
using XDefaultRootWindow_t = Window (*)(Display*);
using XInternAtom_t = Atom (*)(Display*, const char*, Bool);
using XGetWindowProperty_t = int (*)(
    Display*, Window, Atom, long, long, Bool, Atom, Atom*, int*, unsigned long*,
    unsigned long*, unsigned char**);
using XFree_t = int (*)(void*);
using XFetchName_t = Status (*)(Display*, Window, char**);
using XGetWindowAttributes_t = Status (*)(Display*, Window, XWindowAttributes*);
using XShmQueryExtension_t = Bool (*)(Display*);
using XShmCreateImage_t = XImage* (*)(
    Display*, void*, unsigned int, int, char*, XShmSegmentInfo*, unsigned int,
    unsigned int);
using XShmAttach_t = Bool (*)(Display*, XShmSegmentInfo*);
using XShmDetach_t = Bool (*)(Display*, XShmSegmentInfo*);
using XShmGetImage_t = Bool (*)(Display*, Window, XImage*, int, int, unsigned long);
using XDestroyImage_t = int (*)(XImage*);
using XQueryTree_t = Status (*)(
    Display*, Window, Window*, Window*, Window**, unsigned int*);
using XFreePixmap_t = int (*)(Display*, Pixmap);
using XSync_t = int (*)(Display*, Bool);

// XComposite
using XCompositeQueryExtension_t = Bool (*)(Display*, int*, int*);
using XCompositeRedirectWindow_t = void (*)(Display*, Window, int);
using XCompositeUnredirectWindow_t = void (*)(Display*, Window, int);
using XCompositeNameWindowPixmap_t = Pixmap (*)(Display*, Window);

#define CompositeRedirectAutomatic 0
}

namespace Gfx::WindowCapture
{

struct libx11
{
  XOpenDisplay_t OpenDisplay{};
  XCloseDisplay_t CloseDisplay{};
  XDefaultRootWindow_t DefaultRootWindow{};
  XInternAtom_t InternAtom{};
  XGetWindowProperty_t GetWindowProperty{};
  XFree_t Free{};
  XFetchName_t FetchName{};
  XGetWindowAttributes_t GetWindowAttributes{};
  XQueryTree_t QueryTree{};
  XDestroyImage_t DestroyImage{};
  XFreePixmap_t FreePixmap{};
  XSync_t Sync{};

  // XShm (from libXext)
  XShmQueryExtension_t ShmQueryExtension{};
  XShmCreateImage_t ShmCreateImage{};
  XShmAttach_t ShmAttach{};
  XShmDetach_t ShmDetach{};
  XShmGetImage_t ShmGetImage{};

  // XComposite (from libXcomposite)
  XCompositeQueryExtension_t CompositeQueryExtension{};
  XCompositeRedirectWindow_t CompositeRedirectWindow{};
  XCompositeUnredirectWindow_t CompositeUnredirectWindow{};
  XCompositeNameWindowPixmap_t CompositeNameWindowPixmap{};
  bool hasComposite{};

  bool available{};

  static const libx11& instance()
  {
    static const libx11 self;
    return self;
  }

private:
  ossia::dylib_loader m_x11;
  ossia::dylib_loader m_xext;
  std::optional<ossia::dylib_loader> m_xcomposite;

  template <typename T>
  T sym(ossia::dylib_loader& lib, const char* name)
  {
    return lib.symbol<T>(name);
  }

  libx11()
  try : m_x11{std::vector<std::string_view>{"libX11.so.6", "libX11.so"}}
      , m_xext{std::vector<std::string_view>{"libXext.so.6", "libXext.so"}}
  {
    OpenDisplay = sym<XOpenDisplay_t>(m_x11, "XOpenDisplay");
    CloseDisplay = sym<XCloseDisplay_t>(m_x11, "XCloseDisplay");
    DefaultRootWindow = sym<XDefaultRootWindow_t>(m_x11, "XDefaultRootWindow");
    InternAtom = sym<XInternAtom_t>(m_x11, "XInternAtom");
    GetWindowProperty = sym<XGetWindowProperty_t>(m_x11, "XGetWindowProperty");
    Free = sym<XFree_t>(m_x11, "XFree");
    FetchName = sym<XFetchName_t>(m_x11, "XFetchName");
    GetWindowAttributes = sym<XGetWindowAttributes_t>(m_x11, "XGetWindowAttributes");
    QueryTree = sym<XQueryTree_t>(m_x11, "XQueryTree");
    DestroyImage = sym<XDestroyImage_t>(m_x11, "XDestroyImage");
    FreePixmap = sym<XFreePixmap_t>(m_x11, "XFreePixmap");
    Sync = sym<XSync_t>(m_x11, "XSync");

    ShmQueryExtension = sym<XShmQueryExtension_t>(m_xext, "XShmQueryExtension");
    ShmCreateImage = sym<XShmCreateImage_t>(m_xext, "XShmCreateImage");
    ShmAttach = sym<XShmAttach_t>(m_xext, "XShmAttach");
    ShmDetach = sym<XShmDetach_t>(m_xext, "XShmDetach");
    ShmGetImage = sym<XShmGetImage_t>(m_xext, "XShmGetImage");

    // XComposite is optional — graceful fallback if not available
    try
    {
      m_xcomposite.emplace(
          std::vector<std::string_view>{"libXcomposite.so.1", "libXcomposite.so"});
      CompositeQueryExtension
          = sym<XCompositeQueryExtension_t>(*m_xcomposite, "XCompositeQueryExtension");
      CompositeRedirectWindow
          = sym<XCompositeRedirectWindow_t>(*m_xcomposite, "XCompositeRedirectWindow");
      CompositeUnredirectWindow = sym<XCompositeUnredirectWindow_t>(
          *m_xcomposite, "XCompositeUnredirectWindow");
      CompositeNameWindowPixmap = sym<XCompositeNameWindowPixmap_t>(
          *m_xcomposite, "XCompositeNameWindowPixmap");
      hasComposite = CompositeQueryExtension && CompositeRedirectWindow
                     && CompositeUnredirectWindow && CompositeNameWindowPixmap;
    }
    catch(...)
    {
      hasComposite = false;
    }

    available = OpenDisplay && CloseDisplay && DefaultRootWindow && InternAtom
                && GetWindowProperty && Free && FetchName && GetWindowAttributes
                && ShmQueryExtension && ShmCreateImage && ShmAttach && ShmDetach
                && ShmGetImage && DestroyImage && QueryTree && FreePixmap && Sync;
  }
  catch(...)
  {
  }
};

class X11WindowCaptureBackend final : public WindowCaptureBackend
{
public:
  bool available() const override { return libx11::instance().available; }

  // Read a UTF-8 window name via _NET_WM_NAME, falling back to WM_NAME (XFetchName).
  static std::string getWindowName(const libx11& x11, Display* dpy, Window win)
  {
    Atom utf8Atom = x11.InternAtom(dpy, "UTF8_STRING", false);
    Atom netWmName = x11.InternAtom(dpy, "_NET_WM_NAME", false);

    Atom actualType{};
    int actualFormat{};
    unsigned long nItems{}, bytesAfter{};
    unsigned char* prop{};

    if(x11.GetWindowProperty(
           dpy, win, netWmName, 0, 1024, false, utf8Atom, &actualType,
           &actualFormat, &nItems, &bytesAfter, &prop)
           == 0
       && prop && nItems > 0)
    {
      std::string name(reinterpret_cast<char*>(prop), nItems);
      x11.Free(prop);
      return name;
    }
    if(prop)
      x11.Free(prop);

    // Fallback to WM_NAME (Latin-1)
    char* name{};
    if(x11.FetchName(dpy, win, &name) && name)
    {
      std::string s(name);
      x11.Free(name);
      return s;
    }
    return {};
  }

  std::vector<CapturableWindow> enumerate() override
  {
    std::vector<CapturableWindow> result;
    auto& x11 = libx11::instance();
    if(!x11.available)
    {
      qDebug() << "WindowCapture X11: libx11 not available";
      return result;
    }

    Display* dpy = x11.OpenDisplay(nullptr);
    if(!dpy)
    {
      qDebug() << "WindowCapture X11: XOpenDisplay failed";
      return result;
    }

    Window root = x11.DefaultRootWindow(dpy);

    // Try _NET_CLIENT_LIST_STACKING first (includes all desktops, stacking order),
    // then _NET_CLIENT_LIST (all desktops, creation order).
    // Some WMs only populate one of these.
    const char* atomNames[] = {"_NET_CLIENT_LIST_STACKING", "_NET_CLIENT_LIST"};
    bool found = false;

    for(const char* atomName : atomNames)
    {
      if(found)
        break;

      Atom clientListAtom = x11.InternAtom(dpy, atomName, true);
      if(!clientListAtom)
        continue;

      Atom actualType{};
      int actualFormat{};
      unsigned long nItems{}, bytesAfter{};
      unsigned char* data{};

      int ret = x11.GetWindowProperty(
          dpy, root, clientListAtom, 0, 4096, false, AnyPropertyType, &actualType,
          &actualFormat, &nItems, &bytesAfter, &data);

      if(ret == 0 && data && actualFormat == 32 && nItems > 0)
      {
        found = true;
        auto* windowIds = reinterpret_cast<long*>(data);
        for(unsigned long i = 0; i < nItems; i++)
        {
          Window win = static_cast<Window>(windowIds[i]);
          // Don't filter on attributes at all — windows on other virtual
          // desktops may be unmapped with zero-size attributes on some WMs.
          // The _NET_CLIENT_LIST guarantees these are real toplevel windows.
          std::string name = getWindowName(x11, dpy, win);
          if(name.empty())
            name = "(untitled) " + std::to_string(win);
          result.push_back({std::move(name), win});
        }
        x11.Free(data);
      }
      else
      {
        if(data)
          x11.Free(data);
      }
    }
    x11.CloseDisplay(dpy);
    return result;
  }

  bool start(uint64_t windowId) override
  {
    stop();

    auto& x11 = libx11::instance();
    if(!x11.available)
      return false;

    m_display = x11.OpenDisplay(nullptr);
    if(!m_display)
      return false;

    m_windowId = windowId;

    // Check that the window exists and get its size
    XWindowAttributes attrs{};
    if(!x11.GetWindowAttributes(m_display, m_windowId, &attrs))
    {
      qDebug() << "WindowCapture X11: window" << m_windowId << "not found";
      x11.CloseDisplay(m_display);
      m_display = nullptr;
      return false;
    }

    m_width = attrs.width;
    m_height = attrs.height;

    // Check XShm support
    if(!x11.ShmQueryExtension(m_display))
    {
      qDebug() << "WindowCapture X11: MIT-SHM extension not available";
      x11.CloseDisplay(m_display);
      m_display = nullptr;
      return false;
    }

    // Try XComposite for occlusion-free capture of offscreen windows
    m_useComposite = false;
    m_pixmap = 0;
    if(x11.hasComposite)
    {
      int eventBase{}, errorBase{};
      if(x11.CompositeQueryExtension(m_display, &eventBase, &errorBase))
      {
        // Redirect in Automatic mode — the compositor (if any) still
        // displays the window normally. If no compositor is running,
        // the X server allocates a backing pixmap for us.
        x11.CompositeRedirectWindow(
            m_display, m_windowId, CompositeRedirectAutomatic);
        m_pixmap = x11.CompositeNameWindowPixmap(m_display, m_windowId);
        if(m_pixmap)
        {
          m_useComposite = true;
        }
        else
        {
          // Undo redirect if pixmap failed
          x11.CompositeUnredirectWindow(
              m_display, m_windowId, CompositeRedirectAutomatic);
          qDebug() << "WindowCapture X11: XCompositeNameWindowPixmap failed,"
                      " falling back to direct capture";
        }
      }
    }

    if(!allocateShmImage())
      return false;

    m_running = true;
    return true;
  }

  void stop() override
  {
    m_running = false;
    m_lastFrame = {};
    freeShmImage();
    auto& x11 = libx11::instance();
    if(m_display)
    {
      if(m_useComposite)
      {
        if(m_pixmap)
        {
          x11.FreePixmap(m_display, m_pixmap);
          m_pixmap = 0;
        }
        x11.CompositeUnredirectWindow(
            m_display, m_windowId, CompositeRedirectAutomatic);
        m_useComposite = false;
      }
      x11.CloseDisplay(m_display);
      m_display = nullptr;
    }
  }

  CapturedFrame grab() override
  {
    if(!m_running || !m_display || !m_ximage)
      return m_lastFrame;

    auto& x11 = libx11::instance();

    // Check if window still exists and get current size
    XWindowAttributes attrs{};
    if(!x11.GetWindowAttributes(m_display, m_windowId, &attrs))
      return m_lastFrame;

    // Without composite, the window must be mapped
    if(!m_useComposite && attrs.map_state != IsViewable)
      return m_lastFrame;

    // Handle window resize — only when the window is actually viewable
    // (unmapped windows may report stale or zero dimensions)
    if(attrs.map_state == IsViewable
       && attrs.width > 0 && attrs.height > 0
       && (attrs.width != m_width || attrs.height != m_height))
    {
      m_width = attrs.width;
      m_height = attrs.height;

      // XComposite pixmap becomes invalid on resize — re-acquire
      if(m_useComposite)
      {
        if(m_pixmap)
          x11.FreePixmap(m_display, m_pixmap);
        m_pixmap = x11.CompositeNameWindowPixmap(m_display, m_windowId);
        if(!m_pixmap)
          return m_lastFrame;
      }

      freeShmImage();
      if(!allocateShmImage())
        return m_lastFrame;
    }

    // Read from the composite backing pixmap (occlusion-free) or
    // directly from the window (legacy fallback).
    // If the window is unmapped (other desktop), XShmGetImage will fail —
    // we return the last successfully captured frame (frozen but not black).
    XID target = m_useComposite ? m_pixmap : m_windowId;
    if(!x11.ShmGetImage(m_display, target, m_ximage, 0, 0, AllPlanes))
      return m_lastFrame;

    m_lastFrame.type = CapturedFrame::CPU_BGRA;
    m_lastFrame.data = reinterpret_cast<const uint8_t*>(m_ximage->data);
    m_lastFrame.stride = m_ximage->bytes_per_line;
    m_lastFrame.width = m_width;
    m_lastFrame.height = m_height;
    return m_lastFrame;
  }

private:
  bool allocateShmImage()
  {
    auto& x11 = libx11::instance();

    XWindowAttributes attrs{};
    x11.GetWindowAttributes(m_display, m_windowId, &attrs);

    std::memset(&m_shmInfo, 0, sizeof(m_shmInfo));

    m_ximage = x11.ShmCreateImage(
        m_display, attrs.visual, attrs.depth, ZPixmap, nullptr, &m_shmInfo, m_width,
        m_height);
    if(!m_ximage)
    {
      qDebug() << "WindowCapture X11: XShmCreateImage failed";
      return false;
    }

    m_shmInfo.shmid
        = shmget(IPC_PRIVATE, m_ximage->bytes_per_line * m_height, IPC_CREAT | 0777);
    if(m_shmInfo.shmid < 0)
    {
      qDebug() << "WindowCapture X11: shmget failed";
      m_ximage->data = nullptr;
      x11.DestroyImage(m_ximage);
      m_ximage = nullptr;
      return false;
    }

    m_shmInfo.shmaddr = static_cast<char*>(shmat(m_shmInfo.shmid, nullptr, 0));
    if(m_shmInfo.shmaddr == reinterpret_cast<char*>(-1))
    {
      qDebug() << "WindowCapture X11: shmat failed";
      shmctl(m_shmInfo.shmid, IPC_RMID, nullptr);
      m_ximage->data = nullptr;
      x11.DestroyImage(m_ximage);
      m_ximage = nullptr;
      return false;
    }

    m_ximage->data = m_shmInfo.shmaddr;
    m_shmInfo.readOnly = false;

    if(!x11.ShmAttach(m_display, &m_shmInfo))
    {
      qDebug() << "WindowCapture X11: XShmAttach failed";
      shmdt(m_shmInfo.shmaddr);
      shmctl(m_shmInfo.shmid, IPC_RMID, nullptr);
      m_ximage->data = nullptr;
      x11.DestroyImage(m_ximage);
      m_ximage = nullptr;
      return false;
    }

    // Mark for deletion — will be freed when last detach happens
    shmctl(m_shmInfo.shmid, IPC_RMID, nullptr);
    return true;
  }

  void freeShmImage()
  {
    auto& x11 = libx11::instance();
    if(m_ximage && m_display)
    {
      x11.ShmDetach(m_display, &m_shmInfo);
      shmdt(m_shmInfo.shmaddr);
      m_ximage->data = nullptr;
      x11.DestroyImage(m_ximage);
      m_ximage = nullptr;
    }
  }

  Display* m_display{};
  Window m_windowId{};
  Pixmap m_pixmap{};
  XImage* m_ximage{};
  XShmSegmentInfo m_shmInfo{};
  CapturedFrame m_lastFrame{};
  int m_width{};
  int m_height{};
  bool m_running{};
  bool m_useComposite{};
};

// Platform factory function for Linux — X11 backend
// (PipeWire backend is in WindowCapture_pipewire.cpp; selection happens
// in createWindowCaptureBackend())

std::unique_ptr<WindowCaptureBackend> createX11Backend()
{
  // Don't trust XDG_SESSION_TYPE — some users have it set to "tty" even when
  // running X11. Instead, check if we can actually connect to an X display.
  // Skip only if we're certain there's no X11 (Wayland-only session).
  auto& x11 = libx11::instance();
  if(!x11.available)
    return nullptr;

  // Quick probe: try to open the display. If it fails, X11 isn't running.
  Display* dpy = x11.OpenDisplay(nullptr);
  if(!dpy)
    return nullptr;
  x11.CloseDisplay(dpy);

  auto backend = std::make_unique<X11WindowCaptureBackend>();
  return backend;
}

}
