// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Style/ScenarioStyle.hpp>

#include <Spline/View.hpp>
#include <Spline/GeneratorDialog.hpp>
#include <score/graphics/ZoomItem.hpp>
#include <ossia/editor/automation/tinyspline_util.hpp>
#include <Process/ProcessContext.hpp>
#include <score/graphics/PainterPath.hpp>

#include <QMenu>
#include <QPainter>
#include <wobjectimpl.h>
#include <cmath>
#include <numeric>

// Disclaimer:
// Part of the code comes from splineeditor.cpp from
// the Qt project:
// Copyright (C) 2016 The Qt Company Ltd.
// https://github.com/qt/qtdeclarative/blob/dev/tools/qmleasing/splineeditor.cpp

W_OBJECT_IMPL(Spline::View)

namespace Spline
{
class CurveItem : public QGraphicsItem
{
public:
  CurveItem(const ProcessModel& model, const score::DocumentContext& ctx, View& parent)
    : QGraphicsItem{&parent}
    , m_view{parent}
    , m_model{model}
    , m_context{ctx}
  {
    setFlag(ItemClipsToShape, false);
    setAcceptHoverEvents(true);
    setScale(m_zoom);
    updateRect();
  }

  View& m_view;

  QRectF boundingRect() const override
  {
    return QRectF(m_topLeft, m_bottomRight);
  }

  QPointF point(double pos) const noexcept
  {
    auto pt = m_spl.evaluate(pos);
    return mapToCanvas(ossia::nodes::spline_point{pt[0], pt[1]});
  }

  void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override
  {
    // TODO optimize painting here
    if (m_spline.points.empty())
      return;

    auto& skin = Process::Style::instance();
    QPainter& painter = *p;

    // Draw the grid
    {
      auto squarePen = skin.IntervalMuted().main.pen3_dashed_flat_miter;
      squarePen.setWidthF(1. / m_zoom);
      squarePen.setStyle(Qt::SolidLine);

      painter.setBrush(skin.NoBrush());
      painter.setPen(squarePen);

      const auto rect = boundingRect();

      double biggestDim = std::max(rect.width(), rect.height());
      painter.drawLine(-biggestDim * m_zoom, rect.height() / 2., biggestDim * m_zoom, rect.height() / 2.);
      painter.drawLine(rect.height() / 2., -biggestDim * m_zoom, rect.height() / 2., biggestDim * m_zoom);
    }

    painter.setRenderHint(QPainter::Antialiasing, true);

    // Draw the curve
    {
      QPen segmt = m_selectedCurve ? skin.skin.Base2.main.pen2 : skin.skin.Base4.main.pen2;
      segmt.setWidthF(segmt.widthF() / m_zoom);

      painter.strokePath(m_curveShape, segmt);

      if(m_play > 0)
      {
        segmt.setColor(skin.IntervalPlayFill().color());
        painter.strokePath(m_playShape, segmt);
      }
    }

    // Draw the points
    {
      const auto pts = m_spline.points.size();
      if(pts == 0 || pts > 100)
        return;

      // Handle first point
      const auto pointSize = 3. / m_zoom;

      painter.setPen(skin.TransparentPen());
      if (m_selectedPoint && 0 != *m_selectedPoint)
        painter.setBrush(QColor(170, 220, 20));
      else
        painter.setBrush(QColor(170, 220, 220));

      {
        auto fp = mapToCanvas(m_spline.points[0]);
        painter.drawEllipse(
              QRectF{fp.x() - pointSize, fp.y() - pointSize, pointSize * 2., pointSize * 2.});
      }

      QPen purplePen = skin.skin.Emphasis3.darker.pen2_dotted_square_miter;
      purplePen.setWidthF(purplePen.widthF() / m_zoom);
      // Remaining points
      for (std::size_t i = 1U; i < pts; i++)
      {
        QPointF p = mapToCanvas(m_spline.points[i]);

        // Draw the points
        if(m_selectedPoint == i)
        {
          // Draw the purple lines
          painter.setPen(purplePen);
          if(i > 0)
          {
            QPointF p0 = mapToCanvas(m_spline.points[i - 1]);
            painter.drawLine(p0, p);
          }
          if(i < pts - 1)
          {
            QPointF p2 = mapToCanvas(m_spline.points[i + 1]);
            painter.drawLine(p, p2);
          }
          painter.setBrush(QColor(170, 220, 220));
        }
        else
        {
          painter.setBrush(QColor(170, 220, 20));
        }

        painter.setPen(skin.TransparentPen());
        painter.drawEllipse(
              QRectF{p.x() - pointSize, p.y() - pointSize, pointSize * 2., pointSize * 2.});
      }
    }

    painter.setRenderHint(QPainter::Antialiasing, false);
  }

  void updateRect()
  {
    prepareGeometryChange();
    m_topLeft = QPointF{INT_MAX, INT_MAX};
    m_bottomRight = QPointF{INT_MIN, INT_MIN};
    for(auto& pt : m_spline.points)
    {
      m_topLeft.rx() = std::min(pt.x(), m_topLeft.rx());
      m_topLeft.ry() = std::min(1. - pt.y(), m_topLeft.ry());
      m_bottomRight.rx() = std::max(pt.x(), m_bottomRight.rx());
      m_bottomRight.ry() = std::max(1. - pt.y(), m_bottomRight.ry());
    }

    const double w = m_bottomRight.x() - m_topLeft.x();
    const double h = m_bottomRight.y() - m_topLeft.y();
    m_topLeft -= QPointF{0.1 * w, 0.1 * h};
    m_bottomRight += QPointF{0.1 * w, 0.1 * h};
    update();
  }

  void setZoomToFitRect(QRectF parentRect)
  {
    // We want to find the zoom ratio that makes the largest dimension
    // of the child to fit in the parent.
    // e.g. if the child fits in a [10; 30] rect,
    // and the parent is a [400, 500] rect,
    // 400 / 10 = 40
    // 500 / 30 = 16.6
    // we take 16.6 as zoom ratio
    const double w = m_bottomRight.x() - m_topLeft.x();
    const double h = m_bottomRight.y() - m_topLeft.y();
    const double parent_w = parentRect.width();
    const double parent_h = parentRect.height();
    const double w_ratio = parent_w / w;
    const double h_ratio = parent_h / h;

    setZoom(std::min(w_ratio, h_ratio) * 0.9);
  }

  void setZoom(double zoom)
  {
    m_zoom = zoom;
    updateStroke();
    setScale(m_zoom);
  }

  ossia::nodes::spline_point evaluate(double pos) const noexcept
  {
    auto pt = m_spl.evaluate(pos);
    return ossia::nodes::spline_point{pt[0], pt[1]};
  }

  static const constexpr auto N = 500;
  void updateSpline()
  {
    m_spl.set_points(
          reinterpret_cast<const tsReal*>(m_spline.points.data()),
          m_spline.points.size());

    // Recompute the curve
    {
      m_points.clear();

      auto& path = m_curveShape;
      clearPainterPath(path);

      auto pt = mapToCanvas(evaluate(0));
      m_points.push_back(pt);
      path.moveTo(pt);

      for (std::size_t i = 1U; i < N; i++)
      {
        pt = mapToCanvas(evaluate(double(i) / N));
        path.lineTo(pt);
        m_points.push_back(pt);
      }

      updateStroke();
      updatePlayPath();
    }

    m_view.changed();
    update();
  }

  void updateStroke()
  {
    QPainterPathStroker stk;
    stk.setWidth(5. / m_zoom);
    m_strokedShape = stk.createStroke(m_curveShape);
  }

  void updatePlayPath()
  {
    if(m_play > 0)
    {
      auto& path = m_playShape;
      clearPainterPath(path);

      std::size_t max = std::min(std::size_t(qBound(0.f, m_play, 1.f) * N), m_points.size());
      if(max == 0)
        return;

      path.moveTo(m_points[0]);
      for (std::size_t i = 1U; i < max; i++)
      {
        path.lineTo(m_points[i]);
      }
    }
    else
    {
      clearPainterPath(m_playShape);
    }
  }

  void setSpline(ossia::nodes::spline_data d)
  {
    if (d != m_spline)
    {
      m_spline = std::move(d);
      updateSpline();
      updateRect();
    }
    update();
  }

  void setPlayPercentage(float f)
  {
    m_play = f;
    updatePlayPath();
    update();
  }

  const ossia::nodes::spline_data& spline() const { return m_spline; }

  template <typename T>
  QPointF mapToCanvas(const T& point) const
  {
    return QPointF(point.x(), 1. - point.y());
  }

  ossia::nodes::spline_point mapFromCanvas(const QPointF& point) const
  {
    return ossia::nodes::spline_point{(double)point.x(), 1. - point.y()};
  }

  std::optional<std::size_t> findControlPoint(QPointF point) const
  {
    int pointIndex = -1;
    qreal distance = -1;

    const auto N = m_spline.points.size();
    for (std::size_t i = 0; i < N; ++i)
    {
      qreal d = QLineF{point, mapToCanvas(m_spline.points.at(i))}.length();
      if ((distance < 0 && d < 10 / m_zoom) || d < distance)
      {
        distance = d;
        pointIndex = i;
      }
    }

    if (pointIndex != -1)
      return pointIndex;
    return {};
  }

  void hoverEnterEvent(QGraphicsSceneHoverEvent* e) override
  {
    if(m_strokedShape.contains(e->pos()))
      setCursor(Qt::CrossCursor);
  }
  void hoverMoveEvent(QGraphicsSceneHoverEvent* e) override
  {
    if(m_strokedShape.contains(e->pos()))
    {
      if(cursor() != Qt::CrossCursor)
        setCursor(Qt::CrossCursor);
    }
    else
    {
      if(cursor() == Qt::CrossCursor)
        unsetCursor();
    }
  }

  void hoverLeaveEvent(QGraphicsSceneHoverEvent* e) override
  {
    unsetCursor();
  }

  void contextMenuEvent(QGraphicsSceneContextMenuEvent* e) override
  {
    auto menu = new QMenu{e->widget()};
    auto setCurveAct = menu->addAction(QObject::tr("Generate curve"));
    auto res = menu->exec(e->screenPos());
    if(res == setCurveAct)
    {
      auto dial = new GeneratorDialog{this->m_model, this->m_context, e->widget()};
      dial->exec();
    }
    menu->deleteLater();
  }

  void mousePressEvent(QGraphicsSceneMouseEvent* e) override
  {
    auto btn = e->button();
    m_selectedCurve = false;
    if (btn == Qt::LeftButton)
    {
      if ((m_selectedPoint = findControlPoint(e->pos())))
      {
        moveControlPoint(e->pos());
        e->accept();
        update();
      }
      else if(m_strokedShape.contains(e->pos()))
      {
        m_origSpline = m_spline;
        m_origClick = e->pos();
        m_selectedCurve = true;
        e->accept();
        update();
      }
      else
      {
        e->ignore();
      }
    }
    else
    {
      QGraphicsItem::mousePressEvent(e);
    }
  }

  void mouseMoveEvent(QGraphicsSceneMouseEvent* e) override
  {
    auto btn = e->buttons();
    if (btn & Qt::LeftButton)
    {
      if(m_selectedPoint)
      {
        moveControlPoint(e->pos());
        e->accept();
      }
      else if(m_selectedCurve)
      {
        moveCurve(e->pos() - m_origClick);
        e->accept();
      }
      else
      {
        e->ignore();
      }
    }
    else
    {
      QGraphicsItem::mouseMoveEvent(e);
    }
  }

  void mouseReleaseEvent(QGraphicsSceneMouseEvent* e) override
  {
    auto btn = e->button();
    if (btn == Qt::LeftButton)
    {
      if (m_selectedPoint)
      {
        moveControlPoint(e->pos());
        updateRect();
        m_view.changed();
        m_selectedPoint = std::nullopt;
        e->accept();
        m_view.released(e->pos());
        update();
      }
      else if(m_selectedCurve)
      {
        moveCurve(e->pos() - m_origClick);
        updateRect();
        m_view.changed();
        m_selectedCurve = false;
        e->accept();
        m_view.released(e->pos());
        m_origSpline.points.clear();
      }
      else
      {
        e->ignore();
      }
    }
    else
    {
      QGraphicsItem::mouseReleaseEvent(e);
    }
  }

  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override
  {
    const auto newPos = mapFromCanvas(event->pos());
    std::size_t splitIndex = 0;
    const std::size_t N = m_spline.points.size();
    for (std::size_t i = 0; i < N - 1; ++i)
    {
      if (m_spline.points[i].x() <= newPos.x())
      {
        splitIndex = i;
      }
      else
      {
        break;
      }
    }

    m_spline.points.insert(m_spline.points.begin() + splitIndex + 1, newPos);

    updateSpline();
    update();
    event->accept();
  }

  void moveControlPoint(QPointF mouse)
  {
    SCORE_ASSERT(m_selectedPoint);

    auto p = mapFromCanvas(mouse);
    const auto mp = *m_selectedPoint;
    const auto N = m_spline.points.size();
    if (mp < N)
    {
      m_spline.points[mp] = p;

      updateSpline();
      update();
    }
  }

  void moveCurve(QPointF delta)
  {
    for(std::size_t i = 0; i < m_spline.points.size(); i++)
    {
      m_spline.points[i].m_x = m_origSpline.points[i].m_x + delta.x();
      m_spline.points[i].m_y = m_origSpline.points[i].m_y - delta.y();
    }
    updateSpline();
  }
  const ProcessModel& m_model;
  const score::DocumentContext& m_context;

  std::vector<QPointF> m_points;
  QPainterPath m_curveShape, m_playShape;
  QPainterPath m_strokedShape;
  std::optional<std::size_t> m_selectedPoint;
  ossia::nodes::spline_data m_spline;
  ossia::nodes::spline_data m_origSpline;
  QPointF m_origClick;
  ts::spline<2> m_spl;


  double m_zoom{10.};
  QPointF m_topLeft, m_bottomRight;
  float m_play{0.};
  bool m_selectedCurve{};
};

static_assert(std::is_same<tsReal, qreal>::value, "");
View::View(const ProcessModel& m, const Process::Context& ctx, QGraphicsItem* parent) : LayerView{parent}
{
  m_impl = new CurveItem{m, ctx, *this};

  auto item = new score::ZoomItem{this};
  item->setPos(10, 10);

  // TODO proper zooming is done in log space
  connect(item, &score::ZoomItem::zoom, this, [this] {
    auto zoom = m_impl->m_zoom;
    m_impl->setZoom(qBound(0.001, zoom * 1.5, 1000.));
  });
  connect(item, &score::ZoomItem::dezoom,
          this, [this] {
    auto zoom = m_impl->m_zoom;
    m_impl->setZoom(qBound(0.001, zoom / 1.5, 1000.));
  });

  connect(item, &score::ZoomItem::recenter,
          this, &View::recenter);
  recenter();

  this->setFlags(QGraphicsItem::ItemIsFocusable | QGraphicsItem::ItemClipsToShape);
}


void View::setSpline(ossia::nodes::spline_data d)
{
  m_impl->setSpline(std::move(d));
  if(!m_impl->m_selectedPoint)
  {
    recenter();
  }
}

const ossia::nodes::spline_data& View::spline() const noexcept
{
  return m_impl->spline();
}

void View::setPlayPercentage(float p)
{
  m_impl->setPlayPercentage(p);
}

void View::recenter()
{
  m_impl->updateRect();
  m_impl->setZoomToFitRect(boundingRect());
  auto childCenter = m_impl->mapRectToParent(m_impl->boundingRect()).center() - m_impl->pos();
  auto ourCenter = boundingRect().center();
  auto delta = ourCenter - childCenter;

  m_impl->setPos(delta);
}

void View::paint_impl(QPainter* p) const
{
}

void View::mousePressEvent(QGraphicsSceneMouseEvent* e)
{
  m_pressedPos = e->scenePos();
  e->accept();
}

void View::mouseMoveEvent(QGraphicsSceneMouseEvent* e)
{
  const auto delta = e->scenePos() - m_pressedPos;
  m_impl->setPos(m_impl->pos() + delta);
  m_pressedPos = e->scenePos();
  e->accept();
}

void View::mouseReleaseEvent(QGraphicsSceneMouseEvent* e)
{
  mouseMoveEvent(e);
}

void View::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e)
{

}

}
