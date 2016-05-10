#include "seticons.hpp"

void setIcons(QAction *action, const QString &iconOn, const QString &iconOff) {
    QIcon icon;
    icon.addPixmap(iconOn, QIcon::Mode::Selected);
    icon.addPixmap(iconOn, QIcon::Mode::Active);
    icon.addPixmap(iconOn, QIcon::Mode::Normal, QIcon::State::On);

    icon.addPixmap(iconOff, QIcon::Mode::Normal);
    action->setIcon(icon);
}
