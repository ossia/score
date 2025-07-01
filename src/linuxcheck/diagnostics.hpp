#pragma once
#include <ossia/detail/fmt.hpp>

#include <boost/algorithm/string.hpp>

#include <dlfcn.h>

#include <string>
#include <vector>

namespace linuxcheck
{
inline std::string diagnostics()
{
  static constexpr std::string_view libraries[]
      = {"libz.so.1",
         "libudev.so.1",
         "libm.so.6",
         "libxcb-glx.so.0",
#if defined(__x86_64) || defined(__x86_64__)
         "libGLX.so.0",
#else
         "libGLESv2.so.2",
#endif
         "libOpenGL.so.0",
         "libX11.so.6",
         "libX11-xcb.so.1",
         "libxkbcommon.so.0",
         "libxkbcommon-x11.so.0",
         "libxcb-cursor.so.0",
         "libxcb-icccm.so.4",
         "libxcb-image.so.0",
         "libxcb-keysyms.so.1",
         "libxcb-randr.so.0",
         "libxcb-render-util.so.0",
         "libxcb-shm.so.0",
         "libxcb-sync.so.1",
         "libxcb-xfixes.so.0",
         "libxcb-render.so.0",
         "libxcb-shape.so.0",
         "libxcb-xkb.so.1",
         "libxcb.so.1",
         "libEGL.so.1",
         "libdrm.so.2",
         "libgbm.so.1",
         "libwayland-cursor.so.0",
         "libwayland-egl.so.1",
         "libwayland-client.so.0",
         "libresolv.so.2",
         "libgcc_s.so.1",
         "libc.so.6",
         "libasound.so.2|libjack.so.0|libjack.so.1|libjack.so.2|libpipewire-0.3.so.0",
         "libxcb.so.1",
         "libdbus-1.so.3",
         "libbluetooth.so.3",
         "libv4l2.so.0",
         "libavahi-client.so.3"};

  std::string ret;
  for(auto dylibs : libraries)
  {
    std::vector<std::string> split_on_pipe;
    boost::split(split_on_pipe, dylibs, boost::is_any_of("|"));
    bool missing = true;
    for(std::string dylib : split_on_pipe)
    {
      if(auto lib = dlopen(dylib.data(), RTLD_LAZY))
      {
        dlclose(lib);
        missing = false;
        break;
      }
    }

    if(missing)
    {
      ret += fmt::format("{}: MISSING LIBRARY!\n", dylibs);
    }
  }

  static constexpr std::string_view binaries[]
      = {"avahi-daemon", "dbus-broker|dbus-daemon", "jackd|jackd2|pipewire"};
  for(auto programs : binaries)
  {
    std::vector<std::string> split_program_on_pipe;
    boost::split(split_program_on_pipe, programs, boost::is_any_of("|"));
    bool missing = true;
    for(std::string p : split_program_on_pipe)
    {
      if(!system(fmt::format("command -v {} > /dev/null 2>&1", p).c_str()))
      {
        missing = false;
        break;
      }
    }
    if(missing)
    {
      ret += fmt::format("{}: MISSING PROGRAM!\n", programs);
    }
  }

  return ret;
}
}
