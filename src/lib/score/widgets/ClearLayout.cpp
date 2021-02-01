// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ClearLayout.hpp"

#include <QApplication>
#include <QWidget>

void score::setCursor(Qt::CursorShape c)
{
  if (qApp)
  {
    if (auto w = qApp->activeWindow())
    {
      w->setCursor(c);
    }
  }
}
