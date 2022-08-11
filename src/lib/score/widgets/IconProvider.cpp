#include "IconProvider.hpp"

#include <score/widgets/IconProvider.hpp>
#include <score/widgets/Pixmap.hpp>

#include <QFileInfo>
#include <QIcon>
#include <QString>

namespace score
{
IconProvider::~IconProvider() = default;
IconProvider& IconProvider::instance() noexcept
{
  static IconProvider self;
  return self;
}

const QIcon& IconProvider::folderIcon() noexcept
{
  static const QIcon dir_icon = [] {
    QPixmap on = score::get_pixmap(":/icons/load_off.png");
    QIcon icon;
    icon.addPixmap(on, QIcon::Mode::Normal);
    return icon;
  }();
  return dir_icon;
}

QIcon IconProvider::icon(IconType type) const
{
  if(type == IconType::Folder)
    return folderIcon();
  return {};
}

QIcon IconProvider::icon(const QFileInfo& info) const
{
  if(info.isDir())
    return folderIcon();
  return {};
}

QString IconProvider::type(const QFileInfo&) const
{
  return QStringLiteral("Unknown");
}

}
