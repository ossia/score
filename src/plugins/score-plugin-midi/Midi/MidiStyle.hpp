#pragma once
#include <score/model/Skin.hpp>

#include <QBrush>
#include <QGuiApplication>
#include <QPen>

namespace Midi
{
struct MidiStyle
{
  static const MidiStyle& instance() noexcept
  {
    static MidiStyle s;
    return s;
  }

  MidiStyle()
  {
    reload();
    QObject::connect(
        &score::Skin::instance(), &score::Skin::changed, qApp, [this] { reload(); });
  }

  void reload()
  {
    auto& skin = score::Skin::instance();

    noteBaseBrush = skin.Base4.main.brush;
    noteSelectedBasePen = QPen{skin.Base2.main.brush, 2};
    noteBasePen = QPen{skin.Base4.darker300.brush, 1};

    lightBrush = skin.Transparent3.main.brush;
    darkerBrush = skin.LightGray.main.brush;

    darkPen = QPen{skin.Transparent2.main.brush, 1};
    darkPen.setCosmetic(true);

    selectionPen
        = QPen{skin.Transparent1.main.brush, 2, Qt::DashLine, Qt::SquareCap,
               Qt::BevelJoin};
    selectionPen.setCosmetic(true);

    // The velocity ramp: the note colour at a saturation that follows the
    // velocity, so a quiet note is washed out and a loud one is the full
    // accent.
    const QColor base = noteBaseBrush.color();
    const double hue = base.hslHueF();
    const double lightness = base.lightnessF();
    for(std::size_t i = 0; i < std::size(paintedNoteBrush); i++)
    {
      QColor c = base;
      c.setHslF(hue, 0.2 + 1.5 * i / 256., lightness);
      paintedNoteBrush[i].setColor(c);
      paintedNoteBrush[i].setStyle(Qt::SolidPattern);
    }
  }

  QBrush lightBrush;
  QBrush darkerBrush;
  const QBrush transparentBrush{Qt::transparent};
  QPen darkPen;
  QPen selectionPen;

  QBrush noteBaseBrush;
  QPen noteSelectedBasePen;
  QPen noteBasePen;

  QBrush paintedNoteBrush[128];
};
}
