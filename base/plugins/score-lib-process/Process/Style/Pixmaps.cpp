#include <Process/Style/Pixmaps.hpp>
#include <score/widgets/Pixmap.hpp>

namespace Process
{

Pixmaps::Pixmaps()
  : show_ui_off   {score::get_pixmap(":/icons/undock_on.png")}
  , show_ui_on    {score::get_pixmap(":/icons/undock_off.png")}
  , rm_process_off{score::get_pixmap(":/icons/close_on.png")}
  , rm_process_on {score::get_pixmap(":/icons/close_off.png")}
{

}

Pixmaps::~Pixmaps()
{

}

const Pixmaps& Pixmaps::instance() noexcept
{
  static const Pixmaps p;
  return p;
}

}
