#include <score/tools/X11CursorScale.hpp>

#include <QByteArray>
#include <QString>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string_view>

#if defined(SCORE_HAS_X11_CURSOR_SCALE)
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#endif

namespace score
{
namespace
{
double g_cursorScale = 0.;

//! The pointer size the artwork of score's cursors is drawn next to.
constexpr double reference_cursor_size = 24.;

#if defined(SCORE_HAS_X11_CURSOR_SCALE)
int resourceValue(std::string_view resources, std::string_view key)
{
  std::size_t pos = 0;
  while(pos < resources.size())
  {
    auto end = resources.find('\n', pos);
    if(end == resources.npos)
      end = resources.size();
    const auto line = resources.substr(pos, end - pos);
    pos = end + 1;

    if(!line.starts_with(key))
      continue;
    auto value = line.substr(key.size());
    while(!value.empty() && (value.front() == ' ' || value.front() == '\t'))
      value.remove_prefix(1);
    return std::atoi(std::string{value}.c_str());
  }
  return 0;
}

//! Same order as libxcb-cursor, which Qt's xcb plug-in sizes theme cursors
//! with: Xcursor.size, then Xft.dpi, then the size of the screen.
int desktopCursorSize()
{
  int screen_index = 0;
  xcb_connection_t* conn = xcb_connect(nullptr, &screen_index);
  if(!conn || xcb_connection_has_error(conn))
  {
    if(conn)
      xcb_disconnect(conn);
    return 0;
  }

  int size = 0;
  auto it = xcb_setup_roots_iterator(xcb_get_setup(conn));
  for(int i = 0; i < screen_index && it.rem > 0; i++)
    xcb_screen_next(&it);
  if(xcb_screen_t* screen = it.data)
  {
    auto cookie = xcb_get_property(
        conn, 0, screen->root, XCB_ATOM_RESOURCE_MANAGER, XCB_ATOM_STRING, 0,
        16 * 1024);
    if(auto* reply = xcb_get_property_reply(conn, cookie, nullptr))
    {
      const std::string_view resources{
          static_cast<const char*>(xcb_get_property_value(reply)),
          std::size_t(xcb_get_property_value_length(reply))};
      size = resourceValue(resources, "Xcursor.size:");
      if(size <= 0)
        if(const int dpi = resourceValue(resources, "Xft.dpi:"); dpi > 0)
          size = dpi * 16 / 72;
      std::free(reply);
    }
    if(size <= 0)
      size = std::min(screen->width_in_pixels, screen->height_in_pixels) / 48;
  }
  xcb_disconnect(conn);
  return size;
}
#endif
}

void setupX11CursorScale(double zoom)
{
#if defined(SCORE_HAS_X11_CURSOR_SCALE)
  // Wayland compositors size the pointer themselves; so does an X server
  // the application was not going to talk to.
  if(qEnvironmentVariableIsSet("WAYLAND_DISPLAY")
     && qEnvironmentVariable("QT_QPA_PLATFORM") != QLatin1String("xcb"))
    return;
  if(!qEnvironmentVariableIsSet("DISPLAY"))
    return;
  if(const auto platform = qEnvironmentVariable("QT_QPA_PLATFORM");
     !platform.isEmpty() && !platform.startsWith(QLatin1String("xcb")))
    return;

  int size = qEnvironmentVariableIntValue("XCURSOR_SIZE");
  if(size <= 0)
    size = desktopCursorSize();
  if(size <= 0)
    return;

  const int scaled = std::max(1, int(std::lround(size * zoom)));
  if(scaled != size)
    qputenv("XCURSOR_SIZE", QByteArray::number(scaled));
  g_cursorScale = scaled / reference_cursor_size;
#endif
}

double x11CursorScale() noexcept
{
  return g_cursorScale;
}
}
