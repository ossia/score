#include <Process/Style/Pixmaps.hpp>

#include <score/widgets/Pixmap.hpp>

namespace Process
{

Pixmaps::Pixmaps()
    : show_ui_off{score::get_pixmap(":/icons/undock_on.png")}
    , show_ui_on{score::get_pixmap(":/icons/undock_off.png")}
    , close_off{score::get_pixmap(":/icons/close_on.png")}
    , close_on{score::get_pixmap(":/icons/close_off.png")}
    , unmuted{score::get_pixmap(":/icons/process_on.png")}
    , muted{score::get_pixmap(":/icons/process_off.png")}
    , unroll{score::get_pixmap(":/icons/rack_button_off.png")}
    , unroll_selected{score::get_pixmap(":/icons/rack_button_off_selected.png")}
    , roll{score::get_pixmap(":/icons/rack_button_on.png")}
    , roll_selected{score::get_pixmap(":/icons/rack_button_on_selected.png")}

    , add{score::get_pixmap(":/icons/process_add_off.png")}
{
}

Pixmaps::~Pixmaps() {}

const Pixmaps& Pixmaps::instance() noexcept
{
  static const Pixmaps p;
  return p;
}
}
