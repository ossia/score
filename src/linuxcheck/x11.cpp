#include <boost/algorithm/string.hpp>

#include <X11/Xlib.h>

#include <dlfcn.h>

#include <cstdio>
#include <string_view>
#define LOAD_SYM(name)                                                          \
  decltype(::name)* name = reinterpret_cast<decltype(name)>(dlsym(lib, #name)); \
  if(!(name))                                                                   \
  {                                                                             \
    fprintf(stderr, "Could not find symbol " #name "\n");                       \
    dlclose(lib);                                                               \
    return 1;                                                                   \
  }

int draw_text_on_x11(std::string_view txt)
{
  void* lib = dlopen("libX11.so.6", RTLD_LAZY);
  if(!lib)
  {
    fprintf(stderr, "Failed to load libX11.so: %s\n", dlerror());
    return 1;
  }

  LOAD_SYM(XOpenDisplay)
  LOAD_SYM(XDefaultScreen)
  LOAD_SYM(XRootWindow)
  LOAD_SYM(XCreateSimpleWindow)
  LOAD_SYM(XSelectInput)
  LOAD_SYM(XCreateGC)
  LOAD_SYM(XMapWindow)
  LOAD_SYM(XNextEvent)
  LOAD_SYM(XDrawString)
  LOAD_SYM(XFreeGC)
  LOAD_SYM(XDestroyWindow)
  LOAD_SYM(XCloseDisplay)
  LOAD_SYM(XBlackPixel)
  LOAD_SYM(XWhitePixel)

  Display* display{};
  Window window{};
  int screen{};
  GC gc{};
  XEvent event{};
  unsigned int width = 640, height = 480;

  display = XOpenDisplay(nullptr);
  if(!display)
  {
    dlclose(lib);
    return 1;
  }
  screen = XDefaultScreen(display);
  window = XCreateSimpleWindow(
      display, XRootWindow(display, screen), 10, 10, width, height, 1,
      XBlackPixel(display, screen), XWhitePixel(display, screen));
  XSelectInput(display, window, (1L << 15) | (1L << 0)); // ExposureMask | KeyPressMask
  gc = XCreateGC(display, window, 0, nullptr);
  XMapWindow(display, window);

  while(true)
  {
    XNextEvent(display, &event);
    if(event.type == 12)
    {
      std::vector<std::string> lines;
      auto str = boost::algorithm::split(lines, txt, boost::is_any_of("\n\r"));
      int n = 20;
      for(auto s : str)
      {
        XDrawString(display, window, gc, 20, n, s.data(), s.size());
        n += 15;
      }
    }
    if(event.type == 2) // KeyPress
      break;
  }
  XFreeGC(display, gc);
  XDestroyWindow(display, window);
  XCloseDisplay(display);
  dlclose(lib);
  return 0;
}
