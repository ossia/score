// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <State/MessageListSerialization.hpp>

#include <score/graphics/GraphicsItem.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/network/common/destination_qualifiers.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>
#include <ossia/network/dataspace/value_with_unit.hpp>

#include <QColorDialog>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsView>
#include <QMimeData>
#include <QPainter>

#include <Color/GradientView.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Gradient::View)
namespace Gradient
{
View::View(QGraphicsItem* parent)
    : LayerView{parent}
{
  this->setFlags(QGraphicsItem::ItemIsFocusable | QGraphicsItem::ItemClipsToShape);
}

void View::setGradient(const View::gradient_colors& c)
{
  m_colors = c;
  m_origColors = c;
  update();
}

void View::setDataWidth(double x)
{
  m_dataWidth = x;
  update();
}

const constexpr double side = 7;
static const QPainterPath& triangle{[] {
  QPainterPath path;
  path.moveTo(0, 0);
  path.lineTo(side, 0);
  path.lineTo(side / 2., side / 1.5);
  path.lineTo(0, 0);
  return path;
}()};

void View::paint_impl(QPainter* p) const
{
  auto v = ::getView(*this);
  if(!v)
    return;

  const auto view_left = v->mapToScene(QPoint(0., 0.)).x();
  const auto view_right = v->mapToScene(QPoint(v->width(), 0.)).x();

  const auto rect = boundingRect();
  switch(m_colors.size())
  {
    case 0:
      p->fillRect(rect, Qt::black);
      return;
    case 1:
      p->fillRect(rect, m_colors.begin()->second);
      return;
    default:
      break;
  }

  auto it1 = m_colors.begin();
  double pos_1 = 0.;
  double pos_2 = it1->first * m_dataWidth;
  auto col_1 = it1->second;
  auto col_2 = it1->second;
  const auto h = boundingRect().height();

  // From 0. to it1->first: fill with solid color
  if(auto pos_2_scene = mapToScene(pos_2, 0.); !(pos_2_scene.x() < view_left))
  {
    p->fillRect(QRectF(pos_1, 0., pos_2, h), col_1);
  }
  pos_1 = it1->first;
  ++it1;

  // Fill all the gradients
  QLinearGradient g{QPointF{0, 0}, QPointF{1, 0}};
  g.setCoordinateMode(QGradient::CoordinateMode::ObjectMode);
  for(; it1 != m_colors.end(); ++it1)
  {
    pos_2 = it1->first * m_dataWidth;
    col_2 = it1->second;

    auto pos_1_scene = mapToScene(pos_1, 0.);
    auto pos_2_scene = mapToScene(pos_2, 0.);
    if(!(pos_2_scene.x() < view_left || pos_1_scene.x() > view_right))
    {
      g.setColorAt(0., col_1);
      g.setColorAt(1., col_2);

      p->fillRect(QRectF(pos_1, 0., pos_2 - pos_1, h), g);
    }
    pos_1 = pos_2;
    col_1 = col_2;
  }

  // Last solid color block
  if(auto pos_1_scene = mapToScene(pos_2, 0.); !(pos_1_scene.x() > view_right))
  {
    p->fillRect(QRectF(pos_2, 0., m_dataWidth, h), col_2);
  }

  const QPen pen(QColor::fromRgba(qRgba(200, 200, 200, 150)), 1);
  const QBrush br(Qt::gray);
  p->setPen(pen);
  p->setBrush(br);
  for(const auto& col : m_colors)
  {
    p->save();
    p->translate(col.first * m_dataWidth - side / 2., 0);
    p->setCompositionMode(QPainter::CompositionMode_Source);
    p->drawLine(QPointF{side / 2., 0}, QPointF{side / 2., height()});
    p->setCompositionMode(QPainter::CompositionMode_SourceOver);
    p->drawPath(triangle);
    p->restore();
  }
}

void View::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  m_clicked = std::nullopt;

  const auto pos = event->pos();
  for(auto& e : m_colors)
  {
    if(std::abs(e.first * m_dataWidth - pos.x()) < 2.)
    {
      if(event->button() == Qt::LeftButton)
      {
        if(pos.y() < (side / 1.5))
        {
          auto w
              = QColorDialog::getColor(e.second, getView(*this), tr("Gradient color"));
          if(w.isValid())
            setColor(e.first, w);
        }
        else
        {
          m_clicked = e.first;
        }
      }

      else if(event->button() == Qt::RightButton)
      {
        removePoint(e.first);
      }
      break;
    }
  }
}

void View::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  if(m_clicked)
  {
    m_colors = m_origColors;
    auto cur_it = m_colors.find(*m_clicked);
    if(cur_it != m_colors.end())
    {
      auto col = cur_it->second;
      m_colors.erase(*m_clicked);
      auto np = std::max(0., event->pos().x()) / m_dataWidth;

      m_colors.insert(std::make_pair(np, col));
      update();
    }
  }
}

void View::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if(m_clicked)
  {
    m_colors = m_origColors;
    auto cur_it = m_colors.find(*m_clicked);
    if(cur_it != m_colors.end())
    {
      auto col = cur_it->second;
      m_colors.erase(*m_clicked);
      auto np = event->pos().x() / m_dataWidth;

      m_colors.insert(std::make_pair(np, col));
      movePoint(*m_clicked, np);
      update();
    }
  }

  m_clicked = std::nullopt;
}

void View::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  doubleClicked(event->pos());
}

void View::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
  event->accept();
}
void View::dragMoveEvent(QGraphicsSceneDragDropEvent* event)
{
  event->accept();
}
void View::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
  event->accept();
}

static constexpr bool array_is_between(const auto& arr, float min, float max) noexcept
{
  return ossia::all_of(arr, [=](float v) { return v >= min && v <= max; });
}

void View::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  for(auto m : event->mimeData()->formats())
  {
    if(m == score::mime::messagelist())
    {
      Mime<State::MessageList>::Deserializer des{*event->mimeData()};

      // 1. Try to look for one which has a color unit
      const auto& msgs = des.deserialize();
      const State::Message* m{};
      ossia::value_with_unit res;
      for(const auto& mess : msgs)
      {
        auto& u = mess.address.qualifiers.get().unit;
        if(u.v.target<ossia::color_u>())
        {
          m = &mess;
          res = ossia::make_value(mess.value, u);
          break;
        }
      }

      auto vec3_to_color = [&](auto vec, auto& mess) {
        if(array_is_between(vec, 0.f, 1.f))
        {
          m = &mess;
          res = ossia::rgb{vec[0], vec[1], vec[2]};
        }
        else if(array_is_between(vec, 0.f, 255.f))
        {
          m = &mess;
          res = ossia::rgb{vec[0] / 255.f, vec[1] / 255.f, vec[2] / 255.f};
        }
      };

      // 2. Try to look for a vecnf
      if(!m)
      {
        for(const auto& mess : msgs)
        {
          auto t = mess.value.get_type();

          switch(t)
          {
            case ossia::val_type::VEC4F: {
              vec3_to_color(*mess.value.target<ossia::vec4f>(), mess);
              break;
            }
            case ossia::val_type::VEC3F: {
              vec3_to_color(*mess.value.target<ossia::vec3f>(), mess);
              break;
            }
            default:
              break;
          }
          if(m)
            break;
        }
      }

      // 3. Try to look for a vec with the right number of components
      if(!m)
      {
        for(const auto& mess : msgs)
        {
          if(auto t = mess.value.target<std::vector<ossia::value>>();
             t && (t->size() == 3 || t->size() == 4))
          {
            if(ossia::all_of(*t, [](const ossia::value& v) {
                 return v.get_type() == ossia::val_type::FLOAT
                        || v.get_type() == ossia::val_type::INT;
               }))
            {
              vec3_to_color(ossia::convert<ossia::vec3f>(mess.value), mess);
              break;
            }
          }
        }
      }

      // If we found something then we inform the presenter
      if(m)
      {
        auto [r, g, b] = ossia::convert<ossia::vec3f>(
            ossia::to_value(ossia::convert(res, ossia::rgb_u{})));

        dropPoint(event->pos().x(), QColor::fromRgbF(r, g, b));
        break;
      }
    }
  }

  event->accept();
}
}
