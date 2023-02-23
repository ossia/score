#pragma once
#include <QString>
#include <QSize>

namespace Gfx
{
struct CameraSettings
{
  QString input;
  QString device;
  QSize size{};
  double fps{};

  int codec{0};        // an AVCodecID, we just use int to not leak the header...
  int pixelformat{-1}; // an AVPixelFormat ; same
};
}
