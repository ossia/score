// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "TimeRulerGraphicsView.hpp"

#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>

#include <QGLWidget>
#include <QWheelEvent>

namespace Scenario
{

TimeRulerGraphicsView::TimeRulerGraphicsView(QGraphicsScene* scene)
    : QGraphicsView{scene}
{
  setRenderHints(
      QPainter::Antialiasing | QPainter::SmoothPixmapTransform
      | QPainter::TextAntialiasing);
  //#if !defined(SCORE_OPENGL)

  setViewport(new QGLWidget);
  setAlignment(Qt::AlignTop | Qt::AlignLeft);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setFocusPolicy(Qt::NoFocus);
  setSceneRect(ScenarioLeftSpace, -70, 800, 35);
  setFixedHeight(40);
  setViewportUpdateMode(QGraphicsView::NoViewportUpdate);
  setBackgroundBrush(Process::Style::instance().MinimapBackground());
  setOptimizationFlag(QGraphicsView::DontSavePainterState, true);


//#if !defined(__EMSCRIPTEN__)
//  setAttribute(Qt::WA_OpaquePaintEvent, true);
//  setAttribute(Qt::WA_PaintOnScreen, true);
//#endif
  //#endif


#if defined(__APPLE__)
  setRenderHints(0);
  setOptimizationFlag(QGraphicsView::IndirectPainting, true);
#endif
}

MinimapGraphicsView::MinimapGraphicsView(QGraphicsScene* s)
    : TimeRulerGraphicsView{s}
{
  setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
  setSceneRect({0, 0, 2000, 100});

  setDragMode(DragMode::NoDrag);
}

void MinimapGraphicsView::scrollContentsBy(int dx, int dy) {}

void MinimapGraphicsView::wheelEvent(QWheelEvent* event)
{
  event->accept();
}
}
