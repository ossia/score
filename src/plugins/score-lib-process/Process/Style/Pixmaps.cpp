#include <Process/Style/Pixmaps.hpp>

#include <score/widgets/Pixmap.hpp>

#include <QApplication>
#include <QPainter>

namespace Process
{

Pixmaps::Pixmaps() noexcept
    : show_ui_off{score::get_pixmap(":/icons/undock_on.png")}
    , show_ui_on{score::get_pixmap(":/icons/undock_off.png")}

    , close_off{score::get_pixmap(":/icons/close_on.png")}
    , close_on{score::get_pixmap(":/icons/close_off.png")}

    , nodal_off{score::get_pixmap(":/icons/nodal_off.png")}
    , nodal_on{score::get_pixmap(":/icons/nodal_on.png")}

    , timeline_off{score::get_pixmap(":/icons/timeline_off.png")}
    , timeline_on{score::get_pixmap(":/icons/timeline_on.png")}

    , unmuted{score::get_pixmap(":/icons/process_on.png")}
    , muted{score::get_pixmap(":/icons/process_off.png")}

    , unroll{score::get_pixmap(":/icons/rack_button_off.png")}
    , unroll_selected{score::get_pixmap(":/icons/rack_button_off_selected.png")}
    , roll{score::get_pixmap(":/icons/rack_button_on.png")}
    , roll_selected{score::get_pixmap(":/icons/rack_button_on_selected.png")}

    , add{score::get_pixmap(":/icons/process_add_off.png")}

    , metricHandle{[] {
           double dpr = qApp->devicePixelRatio();
           QImage img(13 * dpr, 8 * dpr, QImage::Format_ARGB32_Premultiplied);
           img.fill(Qt::transparent);

           QPainter p(&img);
           QPainterPath path;
           path.lineTo(12 * dpr, 0);
           path.lineTo(6 * dpr, 7 * dpr);
           path.lineTo(0, 0);
           path.closeSubpath();

           p.setRenderHint(QPainter::Antialiasing, true);
           p.fillPath(path, Qt::blue);
           p.end();

           img.setDevicePixelRatio(dpr);
           return QPixmap::fromImage(img);
      }()}
{

}

Pixmaps::~Pixmaps() {}

const Pixmaps& Pixmaps::instance() noexcept
{
  static const Pixmaps p;
  return p;
}
}
