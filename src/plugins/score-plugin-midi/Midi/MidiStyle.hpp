#pragma once
#include <QBrush>
#include <QPen>

namespace Midi
{
struct MidiStyle
{
  MidiStyle()
  {
    auto baseOrange = noteBaseBrush.color();
    float hue = baseOrange.hslHueF();
    float saturation = baseOrange.hslSaturationF();
    float lightness = baseOrange.lightnessF();
    for (std::size_t i = 0; i < std::size(paintedNoteBrush); i++)
    {
      auto orange = baseOrange;
      orange.setHslF(hue, 0.2 + 1.5 *  i / 256., lightness);
      paintedNoteBrush[i].setColor(orange);
      paintedNoteBrush[i].setStyle(Qt::SolidPattern);
    }
  }

  const QBrush lightBrush{QColor{"#3085bcbd"}};
  const QBrush darkerBrush{QColor{"#B0B0B0"}};
  const QBrush transparentBrush{Qt::transparent};
  const QPen darkPen = [] {
    QPen p{QColor{"#604C4C4C"}};
    p.setCosmetic(true);
    p.setWidthF(1);
    return p;
  }();

  const QPen selectionPen = [] {
    QPen pen{QColor{0, 0, 0, 127}, 2, Qt::DashLine, Qt::SquareCap, Qt::BevelJoin};
    pen.setCosmetic(true);
    return pen;
  }();

  const QBrush noteBaseBrush{QColor{"#ff9900"}};
  const QPen noteSelectedBasePen{QColor{"#F6A019"}, 2};
  const QPen noteBasePen{QColor{"#e0b01e"}};

  QBrush paintedNoteBrush[128];
};
}
