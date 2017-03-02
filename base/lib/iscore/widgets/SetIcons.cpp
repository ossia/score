#include "SetIcons.hpp"
#include <hopscotch_map.h>
#include <QHash>

namespace std
{
template<>
struct hash<std::pair<QString, QString>>
{
  std::size_t
  operator()(const std::pair<QString, QString>& p) const noexcept
  {
    return qHash(p);
  }
};
}
static auto& iconMap()
{
  static tsl::hopscotch_map<std::pair<QString, QString>, QIcon> icons;
  return icons;
}

void setIcons(QAction* action, const QString& iconOn, const QString& iconOff)
{
  auto& map = iconMap();
  auto pair = std::make_pair(iconOn, iconOff);
  auto it = map.find(pair);
  if(it != map.end())
  {
    action->setIcon(it.value());
  }
  else
  {
    QIcon icon;
    icon.addFile(iconOn, QSize{}, QIcon::Mode::Selected);
    icon.addFile(iconOn, QSize{}, QIcon::Mode::Active);
    icon.addFile(iconOn, QSize{}, QIcon::Mode::Normal, QIcon::State::On);
    icon.addFile(iconOff, QSize{}, QIcon::Mode::Normal);
    action->setIcon(icon);

    map.insert(std::make_pair(std::move(pair), std::move(icon)));
  }
}

QIcon makeIcons(const QString& iconOn, const QString& iconOff)
{
  return genIconFromPixmaps(iconOn, iconOff);
}

QIcon genIconFromPixmaps(const QString& iconOn, const QString& iconOff)
{
  auto& map = iconMap();
  auto pair = std::make_pair(iconOn, iconOff);
  auto it = map.find(pair);
  if(it != map.end())
  {
    return it.value();
  }
  else
  {
    QIcon icon;
    icon.addFile(iconOn, QSize{}, QIcon::Mode::Selected);
    icon.addFile(iconOn, QSize{}, QIcon::Mode::Active);
    icon.addFile(iconOn, QSize{}, QIcon::Mode::Normal, QIcon::State::On);
    icon.addFile(iconOn, QSize{}, QIcon::Mode::Selected, QIcon::State::On);
    icon.addFile(iconOn, QSize{}, QIcon::Mode::Active, QIcon::State::On);
    icon.addFile(iconOff, QSize{}, QIcon::Mode::Normal);

    map.insert(std::make_pair(std::move(pair), icon));
    return icon;
  }
}
