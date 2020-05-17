#pragma once
#include <QBrush>
#include <QPen>

namespace Midi
{
struct MidiStyle
{
  MidiStyle()
  {
    for (std::size_t i = 0; i < std::size(paintedNotePen); i++)
    {
      auto orange = noteBaseBrush.color();
      orange.setHslF(orange.hslHueF() - 0.02, 1., 0.25 + (127. - i) / 256.);
      paintedNotePen[i].setColor(orange);
      paintedNotePen[i].setWidth(2);
      paintedNotePen[i].setStyle(Qt::DashLine);
      paintedNotePen[i].setCapStyle(Qt::SquareCap);
      paintedNotePen[i].setJoinStyle(Qt::BevelJoin);
      paintedNotePen[i].setCosmetic(true);
    }
  }

  const QBrush lightBrush = QColor::fromRgb(200, 200, 200);
  const QBrush darkBrush = QColor::fromRgb(170, 170, 170);
  const QBrush darkerBrush = QColor::fromRgb(70, 70, 70);
  const QBrush transparentBrush{Qt::transparent};
  const QPen darkPen = [] {
    QPen p = QColor::fromRgb(150, 150, 150);
    p.setCosmetic(true);
    return p;
  }();

  const QPen selectionPen = [] {
    QPen pen{QColor{0, 0, 0, 127}, 2, Qt::DashLine, Qt::SquareCap, Qt::BevelJoin};
    pen.setCosmetic(true);
    return pen;
  }();

  const QBrush noteBaseBrush{QColor::fromRgb(200, 120, 20)};
  const QBrush noteSelectedBaseBrush{QColor::fromRgb(200, 120, 20).darker()};
  const QPen noteBasePen = [] {
    QPen p{QColor::fromRgb(200, 120, 20).darker()};
    // p.setWidthF(0.8);
    return p;
  }();

  QPen paintedNotePen[128];
};
}
