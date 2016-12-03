#include "SetIcons.hpp"

void setIcons(QAction* action, const QString& iconOn, const QString& iconOff)
{
  QIcon icon;
  icon.addFile(iconOn, QSize{}, QIcon::Mode::Selected);
  icon.addFile(iconOn, QSize{}, QIcon::Mode::Active);
  icon.addFile(iconOn, QSize{}, QIcon::Mode::Normal, QIcon::State::On);

  icon.addFile(iconOff, QSize{}, QIcon::Mode::Normal);
  action->setIcon(icon);
}

QIcon makeIcons(const QString& iconOn, const QString& iconOff)
{
  return genIconFromPixmaps(iconOn, iconOff);
}

QIcon genIconFromPixmaps(const QString& iconOn, const QString& iconOff)
{
  QIcon icon;
  icon.addFile(iconOn, QSize{}, QIcon::Mode::Selected);
  icon.addFile(iconOn, QSize{}, QIcon::Mode::Active);
  icon.addFile(iconOn, QSize{}, QIcon::Mode::Normal, QIcon::State::On);
  icon.addFile(iconOn, QSize{}, QIcon::Mode::Selected, QIcon::State::On);
  icon.addFile(iconOn, QSize{}, QIcon::Mode::Active, QIcon::State::On);

  icon.addFile(iconOff, QSize{}, QIcon::Mode::Normal);
  return icon;
}
