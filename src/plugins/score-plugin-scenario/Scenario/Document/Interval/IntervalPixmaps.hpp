#pragma once
#include <Process/Style/ScenarioStyle.hpp>

#include <score/model/Skin.hpp>

#include <QPainter>
#include <QPixmap>

#include <array>
namespace Scenario
{

struct IntervalPixmaps
{
  void update(const Process::Style& style);

  QColor oldBase, oldSelected;
  QPixmap dashed;
  QPixmap dashedSelected;
  std::array<QPixmap, 25> playDashed;

  static void
  drawDashes(qreal from, qreal to, QPainter& p, const QRectF& visibleRect, const QPixmap& pixmap);
};

IntervalPixmaps& intervalPixmaps(const Process::Style& style);
}
