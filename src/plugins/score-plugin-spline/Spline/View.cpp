// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/ProcessContext.hpp>
#include <Process/Style/ScenarioStyle.hpp>

#include <score/graphics/PainterPath.hpp>
#include <score/graphics/ZoomItem.hpp>

#include <ossia/editor/automation/tinyspline_util.hpp>

#include <QCursor>
#include <QMenu>
#include <QPainter>

#include <Spline/GeneratorDialog.hpp>
#include <Spline/View.hpp>

#include <cmath>
#include <wobjectimpl.h>

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

  QRectF boundingRect() const override { return QRectF(m_topLeft, m_bottomRight); }

  QPointF point(double pos) const noexcept
  {
    auto pt = m_spl.evaluate(pos);
    return mapToCanvas(ossia::spline_point{pt[0], pt[1]});
  }

  void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override
  {
    // TODO optimize painting here
    if(m_spline.points.empty())
      return;

    auto& skin = Process::Style::instance();
    QPainter& painter = *p;
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Draw the grid
    if(m_enabled)
    {
      auto squarePen = skin.LocalTimeRuler().main.pen3_dashed_flat_miter;
      squarePen.setWidthF(1.0 / m_zoom);
      squarePen.setStyle(Qt::SolidLine);

      painter.setBrush(skin.NoBrush());
      painter.setPen(squarePen);

      painter.drawLine(
          std::max(-1., m_topLeft.x() * m_zoom) + 30, 0.,
          std::min(1., m_bottomRight.x() * m_zoom) - 30, 0.);
      painter.drawLine(
          0., std::max(-1., m_topLeft.y() * m_zoom) + 30, 0.,
          std::min(1., m_bottomRight.y() * m_zoom) - 30);
    }

    painter.setRenderHint(QPainter::Antialiasing, true);

    // Draw the curve
    {
      QPen segmt = m_enabled ? m_selectedCurve ? skin.skin.Base2.main.pen2
                                               : skin.skin.Base4.main.pen2
                             : skin.skin.Gray.main.pen1;
      segmt.setWidthF(segmt.widthF() / m_zoom);

      painter.strokePath(m_curveShape, segmt);

      if(m_play > 0)
      {
        segmt.setColor(skin.IntervalPlayFill().color());
        painter.strokePath(m_playShape, segmt);
      }
    }

    // Draw the points
    if(m_enabled)
    {
      const auto pts = m_spline.points.size();
      if(pts == 0 || pts > 100)
        return;

      // Handle first point
      const auto pointSize = 3. / m_zoom;

      painter.setPen(skin.TransparentPen());
      if(m_selectedPoint && 0 != *m_selectedPoint)
        painter.setBrush(QColor(170, 220, 20));
      else
        painter.setBrush(QColor(170, 220, 220));

      {
        auto fp = mapToCanvas(m_spline.points[0]);
        painter.drawEllipse(QRectF{
            fp.x() - pointSize, fp.y() - pointSize, pointSize * 2., pointSize * 2.});
      }

      QPen purplePen = skin.skin.Emphasis3.darker.pen2_dotted_square_miter;
      purplePen.setWidthF(purplePen.widthF() / m_zoom);
      // Remaining points
      for(std::size_t i = 1U; i < pts; i++)
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
        painter.drawEllipse(QRectF{
            p.x() - pointSize, p.y() - pointSize, pointSize * 2., pointSize * 2.});
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
      m_topLeft.rx() = std::min(pt.x, m_topLeft.rx());
      m_topLeft.ry() = std::min(1. - pt.y, m_topLeft.ry());
      m_bottomRight.rx() = std::max(pt.x, m_bottomRight.rx());
      m_bottomRight.ry() = std::max(1. - pt.y, m_bottomRight.ry());
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

    if(w < 0.005 || h < 0.005)
      return;

    const double parent_w = parentRect.width();
    const double parent_h = parentRect.height();
    if(parent_w < 5 || parent_h < 5)
      return;

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

  ossia::spline_point evaluate(double pos) const noexcept
  {
    auto pt = m_spl.evaluate(pos);
    return ossia::spline_point{pt[0], pt[1]};
  }

  static const constexpr auto N = 500;
  void updateSpline()
  {
    m_spl.set_points(
        reinterpret_cast<const tsReal*>(m_spline.points.data()), m_spline.points.size());

    // Recompute the curve
    {
      m_points.clear();

      auto& path = m_curveShape;
      path.clear();

      auto pt = mapToCanvas(evaluate(0));
      m_points.push_back(pt);
      path.moveTo(pt);

      for(std::size_t i = 1U; i <= N; i++)
      {
        pt = mapToCanvas(evaluate(double(i) / N));
        path.lineTo(pt);
        m_points.push_back(pt);
      }

      updateRect();
      updateStroke();
      updatePlayPath();
    }

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
      path.clear();

      const double percentage = qBound(0.f, m_play, 1.f) * N;
      const double start = std::floor(percentage);
      const double end = std::ceil(percentage);
      const std::size_t max_start = std::min(std::size_t(start), m_points.size());
      const std::size_t max_end = std::min(std::size_t(end), m_points.size());
      if(max_start == 0)
        return;

      path.moveTo(m_points[0]);
      for(std::size_t i = 1U; i < max_end; i++)
      {
        path.lineTo(m_points[i]);
      }

      if(max_end < m_points.size())
      {
        const double rem = (percentage - max_start);
        const QPointF interp
            = ((1. - rem) * m_points[max_start] + rem * m_points[max_end]);
        path.lineTo(interp);
      }
    }
    else
    {
      m_playShape.clear();
    }
  }

  void setSpline(ossia::spline_data d)
  {
    if(d != m_spline)
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

  const ossia::spline_data& spline() const { return m_spline; }

  QPointF mapToCanvas(const ossia::spline_point& point) const
  {
    return QPointF(point.x, 1. - point.y);
  }

  ossia::spline_point mapFromCanvas(const QPointF& point) const
  {
    return ossia::spline_point{(double)point.x(), 1. - point.y()};
  }

  std::optional<std::size_t> findControlPoint(QPointF point) const
  {
    int pointIndex = -1;
    qreal distance = -1;

    const auto N = m_spline.points.size();
    for(std::size_t i = 0; i < N; ++i)
    {
      qreal d = QLineF{point, mapToCanvas(m_spline.points.at(i))}.length();
      if((distance < 0 && d < 10 / m_zoom) || d < distance)
      {
        distance = d;
        pointIndex = i;
      }
    }

    if(pointIndex != -1)
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
      if(cursor().shape() != Qt::CrossCursor)
        setCursor(Qt::CrossCursor);
    }
    else
    {
      if(cursor().shape() == Qt::CrossCursor)
        unsetCursor();
    }
  }

  void hoverLeaveEvent(QGraphicsSceneHoverEvent* e) override { unsetCursor(); }

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
    if(!m_enabled)
    {
      e->ignore();
      return;
    }

    auto btn = e->button();
    m_selectedCurve = false;
    if(btn == Qt::LeftButton)
    {
      if((m_selectedPoint = findControlPoint(e->pos())))
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
    if(btn & Qt::LeftButton)
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
    if(btn == Qt::LeftButton)
    {
      if(m_selectedPoint)
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
    std::optional<std::size_t> splitIndex;
    std::optional<std::size_t> eraseIndex;
    const std::size_t N = m_spline.points.size();
    if(N <= 3)
    {
      m_spline.points.push_back(newPos);
      updateSpline();
      update();
      event->accept();
      return;
    }

    constexpr auto epsilon = 0.000001;

    // A spline has a minimum of 4 control points
    const bool can_erase = N > 4;

    double dist_min = std::numeric_limits<double>::max();
    const QPointF p0{newPos.x, newPos.y};
    for(std::size_t i = 0; i < N - 1; ++i)
    {
      const QPointF p1{m_spline.points[i].x, m_spline.points[i].y};
      const QPointF p2{m_spline.points[i + 1].x, m_spline.points[i + 1].y};
      const QLineF l1{p0, p1};
      const auto l1l = l1.length();

      const QLineF l2{p0, p2};
      const auto l2l = l1.length();

      if(can_erase)
      {
        if(l1l < epsilon || l2l < epsilon)
        {
          if(l1l <= l2l)
          {
            eraseIndex = i;
          }
          else
          {
            eraseIndex = i + 1;
          }
          break;
        }
      }

      const double num = std::abs(
          (p2.x() - p1.x()) * (p1.y() - p0.y()) - (p1.x() - p0.x()) * (p2.y() - p1.y()));
      const double denom
          = std::sqrt(std::pow(p2.x() - p1.x(), 2) + std::pow(p2.y() - p1.y(), 2));
      const double point_to_line_distance = num / denom;
      const double dist = point_to_line_distance + l1l + l2l;

      if(dist < dist_min)
      {
        dist_min = dist;
        splitIndex = i;
      }
    }

    // Check if we must erase the last one
    if(can_erase)
    {
      if(QLineF{p0, QPointF{m_spline.points[N - 1].x, m_spline.points[N - 1].y}}.length()
         < epsilon)
      {
        eraseIndex = N - 1;
        splitIndex = std::nullopt;
      }
    }

    // Action
    if(eraseIndex)
    {
      m_spline.points.erase(m_spline.points.begin() + *eraseIndex);
    }
    else if(splitIndex)
    {
      m_spline.points.insert(m_spline.points.begin() + *splitIndex + 1, newPos);
    }

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
    if(mp < N)
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
      m_spline.points[i].x = m_origSpline.points[i].x + delta.x();
      m_spline.points[i].y = m_origSpline.points[i].y - delta.y();
    }
    updateSpline();
    update();
  }

  void enable()
  {
    m_enabled = true;

    setEnabled(true);
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);

    update();
  }

  void disable()
  {
    m_enabled = false;

    setEnabled(false);
    setAcceptHoverEvents(false);
    setAcceptedMouseButtons(Qt::NoButton);

    update();
  }
  const ProcessModel& m_model;
  const score::DocumentContext& m_context;

  std::vector<QPointF> m_points;
  QPainterPath m_curveShape, m_playShape;
  QPainterPath m_strokedShape;
  std::optional<std::size_t> m_selectedPoint;
  ossia::spline_data m_spline;
  ossia::spline_data m_origSpline;
  QPointF m_origClick;
  ts::spline<2> m_spl;

  double m_zoom{10.};
  QPointF m_topLeft, m_bottomRight;
  float m_play{0.};
  bool m_enabled{true};
  bool m_selectedCurve{};
};

static_assert(std::is_same<tsReal, qreal>::value, "");
View::View(const ProcessModel& m, const Process::Context& ctx, QGraphicsItem* parent)
    : LayerView{parent}
{
  m_impl = new CurveItem{m, ctx, *this};

  auto item = new score::ZoomItem{this};
  item->setPos(10, 10);

  // TODO proper zooming is done in log space
  connect(item, &score::ZoomItem::zoom, this, [this] {
    auto zoom = m_impl->m_zoom;
    m_impl->setZoom(qBound(0.001, zoom * 1.5, 1000.));
  });
  connect(item, &score::ZoomItem::dezoom, this, [this] {
    auto zoom = m_impl->m_zoom;
    m_impl->setZoom(qBound(0.001, zoom / 1.5, 1000.));
  });

  connect(item, &score::ZoomItem::recenter, this, &View::recenter);
  recenter();

  this->setFlags(QGraphicsItem::ItemIsFocusable | QGraphicsItem::ItemClipsToShape);
}

void View::setSpline(ossia::spline_data d)
{
  m_impl->setSpline(std::move(d));
  if(!m_impl->m_selectedPoint)
  {
    recenter();
  }
}

const ossia::spline_data& View::spline() const noexcept
{
  return m_impl->spline();
}

void View::setPlayPercentage(float p)
{
  m_impl->setPlayPercentage(p);
}

void View::enable()
{
  m_impl->enable();
}

void View::disable()
{
  m_impl->disable();
}

void View::recenter()
{
  m_impl->updateRect();
  m_impl->setZoomToFitRect(boundingRect());
  auto childCenter
      = m_impl->mapRectToParent(m_impl->boundingRect()).center() - m_impl->pos();
  auto ourCenter = boundingRect().center();
  auto delta = ourCenter - childCenter;

  m_impl->setPos(delta);
}

void View::paint_impl(QPainter* p) const { }

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
  update();
  this->parentItem()->update();
}

void View::mouseReleaseEvent(QGraphicsSceneMouseEvent* e)
{
  mouseMoveEvent(e);
}

void View::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e) { }

}
