#include "diagnostics.hpp"

#include <unistd.h>

#include <cstdlib>

int draw_text_on_x11(std::string_view txt);
int draw_text_on_wayland(std::string_view txt);
int main(int argc, char** argv)
{
  if(argc < 2)
    return 1;

  auto text = linuxcheck::diagnostics(argv[1]);
  if(text.empty())
    return 0;
  else
    text
        = "Welcome to ossia score! All the required software are not installed, and "
          "thus the software cannot run.\n"
          "Make sure that the following software and libraries are installed on your "
          "system: \n \n"
          + text;

  fprintf(stderr, "%s\n", text.data());

  if(!system("command -v kdialog > /dev/null 2>&1"))
    if(!system(fmt::format("kdialog --msgbox \"{}\"", text).c_str()))
      return 1;
  if(!system("command -v zenity > /dev/null 2>&1"))
    if(!system(fmt::format("zenity --warning --text=\"{}\"", text).c_str()))
      return 1;
  if(!system("command -v Xdialog > /dev/null 2>&1"))
    if(!system(fmt::format("Xdialog --smooth  --msgbox \"{}\" 200 100", text).c_str()))
      return 1;

  if(getenv("WAYLAND_DISPLAY"))
  {
    if(draw_text_on_wayland(text) == 0)
      return 1;
  }

  if(getenv("DISPLAY"))
  {
    if(draw_text_on_x11(text) == 0)
      return 1;
  }
  return 1;
}
