#include <Process/Style/Pixmaps.hpp>

namespace Process
{

static auto mini_pixmap(QString str)
{
  auto img = QImage(str).scaled(10, 10, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  return QPixmap::fromImage(std::move(img));
}

Pixmaps::Pixmaps()
  : show_ui_off   {mini_pixmap(":/icons/undock_off.png")}
  , show_ui_on    {mini_pixmap(":/icons/undock_on.png")}
  , rm_process_off{mini_pixmap(":/icons/close_off.png")}
  , rm_process_on {mini_pixmap(":/icons/close_on.png")}
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
