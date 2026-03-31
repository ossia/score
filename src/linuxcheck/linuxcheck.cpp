#include "diagnostics.hpp"

#include <unistd.h>

#include <cstdlib>

int draw_text_on_x11(std::string_view txt);
int draw_text_on_wayland(std::string_view txt);
int main(int argc, char** argv)
{
  using namespace std::literals;
  if(argc < 2)
    return 1;

  bool flag_no_gui{};
  for(int i = 2; i < argc; i++)
  {
    if(argv[i] == "--no-gui"sv)
    {
      flag_no_gui = true;
      break;
    }
  }

  auto text = linuxcheck::diagnostics(argv[1]);
  text += " ";
  if(text.empty())
    return 0;

  std::string_view app_name = "ossia score";
  if(auto env_app_name = getenv("SCORE_CUSTOM_APP_APPLICATION_NAME"))
    app_name = env_app_name;

  text = fmt::format(
      "Welcome to {}!\n\n"
      "All the required software or system features are not installed, and thus the "
      "software cannot run properly.\n"
      "Make sure that the following software, features, configuration and libraries "
      "are available on your system: \n \n"
      "{}"
      "\n\nTo install relevant packages, you can look for the relevant scripts (for "
      "instance, ubuntu.plucky.deps.sh),\n"
      "at the following URL:\n"
      "https://github.com/ossia/score/tree/master/ci\n\n"
      "If you do not want to see this message, run the app with SCORE_SKIP_LINUXCHECK=1 "
      "environment variable set.",
      app_name, text);

  fprintf(stderr, "%s\n", text.data());

  if(flag_no_gui || getenv("SCORE_SKIP_LINUXCHECK"))
    return 0;

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
