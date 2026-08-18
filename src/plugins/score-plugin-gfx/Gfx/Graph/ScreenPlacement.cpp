#include "ScreenPlacement.hpp"

#include <QGuiApplication>
#include <QScreen>
#include <QWindow>

namespace score::gfx
{
QSet<QScreen*> occupiedScreens()
{
  QSet<QScreen*> taken;
  for(auto* w : QGuiApplication::topLevelWindows())
  {
    // handle(): a QWindow that was never shown has no platform surface and so
    // holds no screen.
    if(w && w->handle())
      if(auto* s = w->screen())
        taken.insert(s);
  }
  return taken;
}

QScreen* freeScreen(
    QScreen* preferred, const QList<QScreen*>& all, const QSet<QScreen*>& taken) noexcept
{
  if(preferred && !taken.contains(preferred))
    return preferred;

  for(auto* s : all)
    if(s && !taken.contains(s))
      return s;

  return nullptr;
}
}
