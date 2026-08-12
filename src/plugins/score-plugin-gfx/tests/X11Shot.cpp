// Minimal X11 screen grab, because the Jetson image ships no screenshot tool:
// xwd, import, scrot, xfce4-screenshooter, gnome-screenshot and ffmpeg are all
// absent, so there was no way to see what score was actually putting on the
// display -- which led to a rendering problem being inferred from a log line
// rather than observed.
//
// Writes a binary PPM (P6) to stdout or to a file: no libpng dependency, and
// anything on the other end can convert it.
//
// Build (cross, from the score recipe sysroot):
//   $CXX --sysroot=$SYSROOT -O1 X11Shot.cpp -o x11shot -lX11
// Run:
//   DISPLAY=:0 ./x11shot /tmp/shot.ppm

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char** argv)
{
  const char* out = argc > 1 ? argv[1] : "-";

  Display* dpy = XOpenDisplay(nullptr);
  if(!dpy)
  {
    std::fprintf(stderr, "x11shot: cannot open DISPLAY=%s\n",
                 getenv("DISPLAY") ? getenv("DISPLAY") : "(unset)");
    return 2;
  }

  const int screen = DefaultScreen(dpy);
  Window root = RootWindow(dpy, screen);

  XWindowAttributes wa{};
  if(!XGetWindowAttributes(dpy, root, &wa))
  {
    std::fprintf(stderr, "x11shot: XGetWindowAttributes failed\n");
    return 2;
  }
  const int W = wa.width, H = wa.height;

  XImage* img = XGetImage(dpy, root, 0, 0, W, H, AllPlanes, ZPixmap);
  if(!img)
  {
    std::fprintf(stderr, "x11shot: XGetImage failed\n");
    return 2;
  }

  std::FILE* f = (std::strcmp(out, "-") == 0) ? stdout : std::fopen(out, "wb");
  if(!f)
  {
    std::fprintf(stderr, "x11shot: cannot write %s\n", out);
    return 2;
  }
  std::fprintf(f, "P6\n%d %d\n255\n", W, H);

  // The visual is almost certainly 24/32-bit TrueColor here, but read the masks
  // rather than assume: a wrong channel order is exactly the kind of thing that
  // gets mistaken for a rendering bug.
  const unsigned long rm = img->red_mask, gm = img->green_mask, bm = img->blue_mask;
  const auto shiftOf = [](unsigned long m) {
    int s = 0;
    if(!m)
      return 0;
    while(!(m & 1))
    {
      m >>= 1;
      ++s;
    }
    return s;
  };
  const int rs = shiftOf(rm), gs = shiftOf(gm), bs = shiftOf(bm);

  std::fwrite(nullptr, 0, 0, f);
  for(int y = 0; y < H; ++y)
  {
    for(int x = 0; x < W; ++x)
    {
      const unsigned long px = XGetPixel(img, x, y);
      const unsigned char rgb[3]
          = {(unsigned char)((px & rm) >> rs), (unsigned char)((px & gm) >> gs),
             (unsigned char)((px & bm) >> bs)};
      std::fwrite(rgb, 1, 3, f);
    }
  }

  if(f != stdout)
    std::fclose(f);
  XDestroyImage(img);
  XCloseDisplay(dpy);
  std::fprintf(stderr, "x11shot: %dx%d written to %s\n", W, H, out);
  return 0;
}
