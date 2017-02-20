#include "TimeRulerGraphicsView.hpp"
#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>

namespace Scenario
{

TimeRulerGraphicsView::TimeRulerGraphicsView(QGraphicsScene* scene)
    : QQuickWidget{scene}
{
  setRenderHints(
      QPainter::Antialiasing | QPainter::SmoothPixmapTransform
      | QPainter::TextAntialiasing);
#if !defined(ISCORE_OPENGL)
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAttribute(Qt::WA_PaintOnScreen, true);
#endif
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setFocusPolicy(Qt::NoFocus);
  setSceneRect(ScenarioLeftSpace, -70, 800, 35);
  setFixedHeight(40);
  setViewportUpdateMode(QQuickWidget::NoViewportUpdate);
  setAlignment(Qt::AlignTop | Qt::AlignLeft);

  setBackgroundBrush(ScenarioStyle::instance().TimeRulerBackground.getBrush());

  setOptimizationFlag(QQuickWidget::DontSavePainterState, true);
#if defined(__APPLE__)
  setRenderHints(0);
  setOptimizationFlag(QQuickWidget::IndirectPainting, true);
#endif
}
}
