#pragma once
#include <QApplication>
#include <QPixmap>
#include <QScreen>

namespace score
{
inline QImage get_image(QString str)
{
  QImage img;
  if (auto screen = qApp->primaryScreen())
  {
    if (screen->devicePixelRatio() >= 2.0)
    {
      str.replace(".png", "@2x.png", Qt::CaseInsensitive);
    }
  }
  img.load(str);
  return img;
}
}
