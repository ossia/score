// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SetIcons.hpp"

#include <score/widgets/Pixmap.hpp>

#include <QDebug>
#include <QFile>
#include <QGuiApplication>
#include <QScreen>

#include <tsl/hopscotch_map.h>

namespace std
{
template <>
struct hash<std::pair<QString, QString>>
{
  std::size_t operator()(const std::pair<QString, QString>& p) const noexcept { return qHash(p); }
};
}
static auto& iconMap()
{
  static tsl::hopscotch_map<std::pair<QString, QString>, QIcon> icons;
  return icons;
}

void setIcons(
    QAction* action,
    const QString& iconOn,
    const QString& iconOff,
    const QString& iconDisable,
    bool enableHover)
{
  auto& map = iconMap();
  auto pair = std::make_pair(iconOn, iconOff);
  auto it = map.find(pair);
  if (it != map.end())
  {
    action->setIcon(it.value());
  }
  else
  {
    QIcon icon;
    QPixmap on = score::get_pixmap(iconOn);
    QPixmap off = score::get_pixmap(iconOff);
    QPixmap disable = score::get_pixmap(iconDisable);
    icon.addPixmap(on, QIcon::Mode::Selected);
    if (enableHover)
      icon.addPixmap(on, QIcon::Mode::Active);
    icon.addPixmap(on, QIcon::Mode::Normal, QIcon::State::On);
    icon.addPixmap(disable, QIcon::Mode::Disabled);
    icon.addPixmap(off, QIcon::Mode::Normal);
    action->setIcon(icon);

    map.insert(std::make_pair(std::move(pair), std::move(icon)));
  }
}

QIcon makeIcon(const QString& icon)
{
  return genIconFromPixmaps(icon, icon, icon);
}

QIcon makeIcons(const QString& iconOn, const QString& iconOff, const QString& iconDisabled)
{
  return genIconFromPixmaps(iconOn, iconOff, iconDisabled);
}

QIcon genIconFromPixmaps(
    const QString& iconOn,
    const QString& iconOff,
    const QString& iconDisabled)
{
  auto& map = iconMap();
  auto pair = std::make_pair(iconOn, iconOff);
  auto it = map.find(pair);
  if (it != map.end())
  {
    return it.value();
  }
  else
  {
    QIcon icon;
    QPixmap on(score::get_pixmap(iconOn));
    QPixmap off(score::get_pixmap(iconOff));
    QPixmap disabled(score::get_pixmap(iconDisabled));

    icon.addPixmap(on, QIcon::Mode::Selected);
    icon.addPixmap(on, QIcon::Mode::Active);
    icon.addPixmap(on, QIcon::Mode::Normal, QIcon::State::On);
    icon.addPixmap(on, QIcon::Mode::Selected, QIcon::State::On);
    icon.addPixmap(on, QIcon::Mode::Active, QIcon::State::On);
    icon.addPixmap(off, QIcon::Mode::Normal);
    icon.addPixmap(disabled, QIcon::Mode::Disabled);

    map.insert(std::make_pair(std::move(pair), icon));
    return icon;
  }
}

namespace score
{

QPixmap get_pixmap(QString str)
{
  QPixmap img;
  if (auto screen = qApp->primaryScreen())
  {
    if (screen->devicePixelRatio() >= 2.0)
    {
      auto newstr = str;
      newstr.replace(".png", "@2x.png", Qt::CaseInsensitive);
      if (QFile::exists(newstr))
      {
        img.setDevicePixelRatio(2.);
        str = newstr;
      }
      else
      {
        qDebug() << "hidpi pixmap not found: " << newstr;
      }
    }
  }
  img.load(str);
  return img;
}

QImage get_image(QString str)
{
  QImage img;
  if (auto screen = qApp->primaryScreen())
  {
    if (screen->devicePixelRatio() >= 2.0)
    {
      auto newstr = str;
      newstr.replace(".png", "@2x.png", Qt::CaseInsensitive);
      if (QFile::exists(newstr))
      {
        img.setDevicePixelRatio(2.);
        str = newstr;
      }
      else
      {
        qDebug() << "hidpi pixmap not found: " << newstr;
      }
    }
  }
  img.load(str);
  return img;
}
}
