// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SetIcons.hpp"

#include <score/application/ApplicationContext.hpp>
#include <score/widgets/Pixmap.hpp>

#include <core/application/ApplicationSettings.hpp>

#include <ossia/detail/hash_map.hpp>

#include <QDebug>
#include <QDirIterator>
#include <QFile>
#include <QGuiApplication>
#include <QScreen>
#include <QSvgRenderer>
#include <QtSvg>

namespace std
{
template <>
struct hash<std::pair<QString, QString>>
{
  std::size_t operator()(const std::pair<QString, QString>& p) const noexcept
  {
    return qHash(p);
  }
};

template <>
struct hash<std::tuple<QString, QString, QString, QString>>
{
  std::size_t
  operator()(const std::tuple<QString, QString, QString, QString>& p) const noexcept
  {
    return qHash(std::get<0>(p)) ^ qHash(std::get<1>(p)) ^ qHash(std::get<2>(p))
           ^ qHash(std::get<3>(p));
  }
};
}
static auto& iconMap()
{
  static ossia::hash_map<std::tuple<QString, QString, QString, QString>, QIcon> icons;
  return icons;
}

void setIcons(
    QAction* action, const QString& iconOn, const QString& iconOff,
    const QString& iconDisable, bool enableHover)
{
  auto& map = iconMap();
  auto pair = std::make_tuple(iconOn, iconOff, iconDisable, QString{});
  auto it = map.find(pair);
  if(it != map.end())
  {
    action->setIcon(it->second);
  }
  else
  {
    QIcon icon;
    QPixmap on = score::get_pixmap(iconOn);
    QPixmap off = score::get_pixmap(iconOff);
    QPixmap disable = score::get_pixmap(iconDisable);
    icon.addPixmap(on, QIcon::Mode::Selected);
    if(enableHover)
    {
      icon.addPixmap(on, QIcon::Mode::Active);
      icon.addPixmap(on, QIcon::Mode::Active, QIcon::State::On);
    }
    icon.addPixmap(on, QIcon::Mode::Normal, QIcon::State::On);
    icon.addPixmap(disable, QIcon::Mode::Disabled);
    icon.addPixmap(off, QIcon::Mode::Normal);
    action->setIcon(icon);

    map.insert(std::make_pair(std::move(pair), std::move(icon)));
  }
}
void setIcons(
    QAction* action, const QString& iconOn, const QString& iconHover,
    const QString& iconOff, const QString& iconDisable)
{
  auto& map = iconMap();
  auto pair = std::make_tuple(iconOn, iconOff, iconDisable, QString{});
  auto it = map.find(pair);
  if(it != map.end())
  {
    action->setIcon(it->second);
  }
  else
  {
    QIcon icon;
    QPixmap on = score::get_pixmap(iconOn);
    QPixmap off = score::get_pixmap(iconOff);
    QPixmap disable = score::get_pixmap(iconDisable);
    QPixmap hover = score::get_pixmap(iconHover);
    icon.addPixmap(on, QIcon::Mode::Selected);
    icon.addPixmap(hover, QIcon::Mode::Active);
    icon.addPixmap(hover, QIcon::Mode::Active, QIcon::State::On);
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

QIcon makeIcons(
    const QString& iconOn, const QString& iconOff, const QString& iconDisabled)
{
  return genIconFromPixmaps(iconOn, iconOff, iconDisabled);
}

QIcon makeIcons(
    const QString& iconOn, const QString& iconHover, const QString& iconOff,
    const QString& iconDisabled)
{
  return genIconFromPixmaps(iconOn, iconHover, iconOff, iconDisabled);
}

QIcon genIconFromPixmaps(
    const QString& iconOn, const QString& iconOff, const QString& iconDisabled)
{
  auto& map = iconMap();
  auto pair = std::make_tuple(iconOn, iconOff, iconDisabled, QString{});
  auto it = map.find(pair);
  if(it != map.end())
  {
    return it->second;
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

QIcon genIconFromPixmaps(
    const QString& iconOn, const QString& iconHover, const QString& iconOff,
    const QString& iconDisabled)
{
  auto& map = iconMap();
  auto pair = std::make_tuple(iconOn, iconOff, iconDisabled, iconHover);
  auto it = map.find(pair);
  if(it != map.end())
  {
    return it->second;
  }
  else
  {
    QIcon icon;
    QPixmap on(score::get_pixmap(iconOn));
    QPixmap off(score::get_pixmap(iconOff));
    QPixmap disabled(score::get_pixmap(iconDisabled));
    QPixmap hover(score::get_pixmap(iconHover));

    icon.addPixmap(on, QIcon::Mode::Selected);
    icon.addPixmap(hover, QIcon::Mode::Active);
    icon.addPixmap(hover, QIcon::Mode::Active, QIcon::State::On);
    icon.addPixmap(on, QIcon::Mode::Normal, QIcon::State::On);
    icon.addPixmap(on, QIcon::Mode::Selected, QIcon::State::On);
    icon.addPixmap(off, QIcon::Mode::Normal);
    icon.addPixmap(disabled, QIcon::Mode::Disabled);

    map.insert(std::make_pair(std::move(pair), icon));
    return icon;
  }
}
namespace score
{
static ossia::hash_map<QString, QString> svg_pixmap_map{};
static void init_svgmap()
{
  if(svg_pixmap_map.empty())
  {
    QDirIterator d{
        ":/",
        {"*.svg"},
        QDir::Files | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories};
    while(d.hasNext())
    {
      const auto& file = d.nextFileInfo();
      svg_pixmap_map[file.baseName()] = file.absoluteFilePath();
    }
  }
}

static QPixmap render_svg(const QString& svg, double scaleFactor)
{
  QSvgRenderer renderer{svg};

  QSize baseSize = renderer.defaultSize();
  QSize targetSize = baseSize * scaleFactor;
  QPixmap img{targetSize};
  img.fill(Qt::transparent);

  QPainter painter{&img};
  painter.setRenderHint(QPainter::Antialiasing);
  renderer.render(&painter);
  painter.end();

  img.setDevicePixelRatio(scaleFactor);

  return img;
}

static QImage render_svg_image(const QString& svg, double scaleFactor)
{
  QSvgRenderer renderer{svg};

  QSize baseSize = renderer.defaultSize();
  QSize targetSize = baseSize * scaleFactor;
  QImage img{targetSize, QImage::Format_ARGB32_Premultiplied};
  img.fill(Qt::transparent);

  QPainter painter{&img};
  painter.setRenderHint(QPainter::Antialiasing);
  renderer.render(&painter);
  painter.end();

  img.setDevicePixelRatio(scaleFactor);

  return img;
}

QPixmap get_pixmap(QString str, QString svg)
{
  init_svgmap();

  QPixmap img;
  static const auto& appSettings = score::AppContext().applicationSettings;
  if(!appSettings.gui)
    return img;

  if(appSettings.vector_gui)
  {
    if(QFile::exists(svg))
    {
      return render_svg(svg, 4.);
    }
    else
    {
      QFileInfo f{str};
      if(auto it = svg_pixmap_map.find(f.baseName()); it != svg_pixmap_map.end())
      {
        img.load(it->second);
        return render_svg(it->second, 4.);
      }
    }
  }

  if(appSettings.vector_gui || qGuiApp->devicePixelRatio() >= 1.5)
  {
    auto newstr = str;
    newstr.replace(".png", "@2x.png", Qt::CaseInsensitive);
    if(QFile::exists(newstr))
    {
      img.setDevicePixelRatio(2.);
      str = newstr;
    }
    else
    {
      qDebug() << "hidpi pixmap not found: " << newstr;
    }
  }
  img.load(str);
  return img;
}

QCursor get_cursor(QString str, double hotspot_x, double hotspot_y)
{
  QCursor cur;
  static const auto& appSettings = score::AppContext().applicationSettings;
  if(!appSettings.gui)
    return cur;

  // Here we don't care about the vector gui thing as cursors are never scaled
  QPixmap img;
  if(qGuiApp->devicePixelRatio() >= 1.5)
  {
    auto newstr = str;
    newstr.replace(".png", "@2x.png", Qt::CaseInsensitive);
    if(QFile::exists(newstr))
    {
      str = newstr;
      img.load(str);
      img.setDevicePixelRatio(2.);
    }
    else
    {
      qDebug() << "hidpi pixmap not found: " << newstr;
      img.load(str);
    }
  }
  else
  {
    img.load(str);
  }
  cur = QCursor{img, (int)hotspot_x, (int)hotspot_y};
  return cur;
}

QImage get_image(QString str, QString svg)
{
  init_svgmap();

  QImage img;
  static const auto& appSettings = score::AppContext().applicationSettings;
  if(!appSettings.gui)
    return img;

  if(appSettings.vector_gui)
  {
    if(QFile::exists(svg))
    {
      return render_svg_image(svg, 4.);
    }
    else
    {
      QFileInfo f{str};
      if(auto it = svg_pixmap_map.find(f.baseName()); it != svg_pixmap_map.end())
      {
        img.load(it->second);
        return render_svg_image(it->second, 4.);
      }
    }
  }

  if(appSettings.vector_gui || qGuiApp->devicePixelRatio() >= 2.0)
  {
    auto newstr = str;
    newstr.replace(".png", "@2x.png", Qt::CaseInsensitive);
    if(QFile::exists(newstr))
    {
      img.setDevicePixelRatio(2.);
      str = newstr;
    }
    else
    {
      qDebug() << "hidpi pixmap not found: " << newstr;
    }
  }
  img.load(str);
  return img;
}
}
