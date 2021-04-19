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

  QPixmap dashed;
  QPixmap dashedSelected;
  QPixmap dashedDropTarget;
  QPixmap dashedWarning;
  QPixmap dashedInvalid;
  QPixmap dashedMuted;
  std::array<QPixmap, 25> playDashed;

  int loadIndex{};
  static void
  drawDashes(qreal from, qreal to, QPainter& p, const QRectF& visibleRect, const QPixmap& pixmap);
};

IntervalPixmaps& intervalPixmaps(const Process::Style& style);
}
