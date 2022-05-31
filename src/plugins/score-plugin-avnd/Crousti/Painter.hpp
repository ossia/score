#pragma once
#include <score/model/Skin.hpp>

#include <QGradient>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPolygon>

#include <avnd/concepts/painter.hpp>
#include <cmath>
#include <halp/texture.hpp>

namespace oscr
{
struct QPainterAdapter
{
  QPainter& painter;
  QGraphicsItem& item;
  QPainterPath path;

  void begin_path() { path = QPainterPath{}; }
  void close_path() { path.closeSubpath(); }
  void stroke() { painter.strokePath(path, painter.pen()); }
  void fill() { painter.fillPath(path, painter.brush()); }
  void update() { item.update(); }

  void move_to(double x, double y) { path.moveTo(x, y); }
  void line_to(double x, double y) { path.lineTo(x, y); }
  void
  arc_to(double x, double y, double w, double h, double start, double length)
  {
    path.arcTo(x, y, w, h, start, length);
  }

  void cubic_to(
      double c1x,
      double c1y,
      double c2x,
      double c2y,
      double endx,
      double endy)
  {
    path.cubicTo(c1x, c1y, c2x, c2y, endx, endy);
  }

  void quad_to(double x1, double y1, double x2, double y2)
  {
    path.quadTo(x1, y1, x2, y2);
  }

  void translate(double x, double y) { painter.translate(x, y); }
  void scale(double x, double y) { painter.scale(x, y); }
  void rotate(double a) { painter.rotate(a); }
  void reset_transform() { painter.resetTransform(); }

  // Colors:
  void set_stroke_color(halp::rgba_color c)
  {
    QPen p = painter.pen();
    p.setColor(qRgba(c.r, c.g, c.b, c.a));
    painter.setPen(p);
  }

  void set_stroke_width(double w)
  {
    QPen p = painter.pen();
    p.setWidth(w);
    painter.setPen(p);
  }

  void set_fill_color(halp::rgba_color c)
  {
    painter.setBrush(QColor(qRgba(c.r, c.g, c.b, c.a)));
  }

  void set_linear_gradient(
      double x1,
      double y1,
      double x2,
      double y2,
      halp::rgba_color c1,
      halp::rgba_color c2)
  {
    QLinearGradient gradient(QPointF(x1, y1), QPointF(x2, y2));
    gradient.setColorAt(0, QColor(qRgba(c1.r, c1.g, c1.b, c1.a)));
    gradient.setColorAt(1, QColor(qRgba(c2.r, c2.g, c2.b, c2.a)));
    painter.setBrush(gradient);
  }

  void set_radial_gradient(
      double cx,
      double cy,
      double cr,
      halp::rgba_color c1,
      halp::rgba_color c2)
  {
    QRadialGradient gradient(cx, cy, cr);
    gradient.setColorAt(0, QColor(qRgba(c1.r, c1.g, c1.b, c1.a)));
    gradient.setColorAt(1, QColor(qRgba(c2.r, c2.g, c2.b, c2.a)));
    painter.setBrush(gradient);
  }

  void set_conical_gradient(
      double x,
      double y,
      double a,
      halp::rgba_color c1,
      halp::rgba_color c2)
  {
    QConicalGradient gradient(x, y, a);
    gradient.setColorAt(0, QColor(qRgba(c1.r, c1.g, c1.b, c1.a)));
    gradient.setColorAt(1, QColor(qRgba(c2.r, c2.g, c2.b, c2.a)));
    painter.setBrush(gradient);
  }

  // Text:
  void set_font(std::string_view f)
  {
    auto font = painter.font();
    font.setFamily(QString::fromUtf8(f.data(), f.size()));
    painter.setFont(font);
  }

  void set_font_size(double f)
  {
    auto font = painter.font();
    font.setPointSize(f);
    painter.setFont(font);
  }

  void draw_text(double x, double y, std::string_view str)
  {
    path.addText(
        x, y, painter.font(), QString::fromUtf8(str.data(), str.size()));
  }

  // Drawing
  void draw_line(double x1, double y1, double x2, double y2)
  {
    path.moveTo(x1, y1);
    path.lineTo(x2, y2);
  }

  //          x1, y1, x2 , y2, x3, y3
  void draw_triangle(
      double x1,
      double y1,
      double x2,
      double y2,
      double x3,
      double y3)
  {
    path.moveTo(x1, y1);
    path.lineTo(x2, y2);
    path.lineTo(x3, y3);
    path.lineTo(x1, y1);
    painter.drawPath(path);
  }

  //          x , y , w  , h
  void draw_rect(double x, double y, double w, double h)
  {
    path.addRect(x, y, w, h);
  }

  //                  x , y , w  , h
  void draw_rounded_rect(double x, double y, double w, double h, double r)
  {
    path.addRoundedRect(x, y, w, h, r, r);
  }

  //            x , y , filename
  void draw_pixmap(double x, double y, const QString& str)
  {
    painter.drawPixmap(x, y, str);
  }

  //             x , y , w  , h
  void draw_ellipse(double x, double y, double w, double h)
  {
    path.addEllipse(x, y, w, h);
  }

  //            cx, cy, radius
  void draw_circle(double cx, double cy, double cr)
  {
    path.addEllipse(QPointF{cx, cy}, cr, cr);
  }

  //          tab, count
  void draw_polygon(double* tab, int count)
  {
    QPolygonF poly;
    double x, y;
    for (int i = 0; i < count * 2; i += 2)
    {
      x = tab[i];
      y = tab[i + 1];
      poly << QPointF(x, y);
    }
    path.addPolygon(poly);
  }
};
static_assert(avnd::painter<QPainterAdapter>);

template <typename Item, typename Control = void>
class CustomItem : public QGraphicsItem
{
public:
  // Item may be T::item_type or T&
  using item_type = std::decay_t<Item>;

  // Case T::item_type
  explicit CustomItem()
  {
    this->setFlag(ItemClipsToShape);
    this->setFlag(ItemClipsChildrenToShape);
    if constexpr (requires { impl.transaction; })
    {
      impl.transaction.start = [] {};
      impl.transaction.update = [this](const auto& value)
      {
        impl.value = value; //impl.value_to_control(Control{}
        update();
      };
      impl.transaction.commit = [] {};
      impl.transaction.rollback = [] {};
    }
  }

  // Case T&
  explicit CustomItem(Item item_init)
      : impl{item_init}
  {
    this->setFlag(ItemClipsToShape);
    this->setFlag(ItemClipsChildrenToShape);
  }

  QRectF boundingRect() const override
  {
    return {0., 0., item_type::width(), item_type::height()};
  }

  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override
  {
    auto& skin = score::Skin::instance();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(skin.Dark.main.pen1);
    impl.paint(QPainterAdapter{*painter, *this, {}});
    painter->setRenderHint(QPainter::Antialiasing, false);
  }

  struct custom_mouse_event
  {
    enum button
    {
      no_button,
      left = (1 << 1),
      right = (1 << 2),
      middle = (1 << 3)
    };
    enum modifier
    {
      no_modifier,
      shift = (1 << 1),
      ctrl = (1 << 2),
      alt = (1 << 3),
      meta = (1 << 4)
    };
    float x, y;

    enum button button
    {
    };
    enum button held_buttons
    {
    };
    enum modifier modifiers
    {
    };
  };

protected:
  static custom_mouse_event
  make_event(QGraphicsSceneMouseEvent* event) noexcept
  {
    custom_mouse_event p;

    p.x = event->pos().x();
    p.y = event->pos().y();

    if (event->button() == Qt::LeftButton)
      p.button = custom_mouse_event::left;
    else if (event->button() == Qt::RightButton)
      p.button = custom_mouse_event::right;
    else if (event->button() == Qt::MiddleButton)
      p.button = custom_mouse_event::middle;

    if (event->buttons() & Qt::LeftButton)
      p.held_buttons |= p.left;
    if (event->buttons() & Qt::RightButton)
      p.held_buttons |= p.right;
    if (event->buttons() & Qt::MiddleButton)
      p.held_buttons |= p.middle;

    if (event->modifiers() & Qt::ShiftModifier)
      p.modifiers |= p.shift;
    if (event->modifiers() & Qt::AltModifier)
      p.modifiers |= p.alt;
    if (event->modifiers() & Qt::ControlModifier)
      p.modifiers |= p.ctrl;
    if (event->modifiers() & Qt::MetaModifier)
      p.modifiers |= p.meta;

    return p;
  }

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override
  {
    if constexpr (requires { impl.mouse_press(0, 0); })
    {
      if (impl.mouse_press(event->pos().x(), event->pos().y()))
        event->accept();
    }
    else if constexpr (requires { impl.mouse_press(custom_mouse_event{}); })
    {
      if (impl.mouse_press(make_event(event)))
        event->accept();
    }
    update();
  }

  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override
  {
    if constexpr (requires { impl.mouse_move(0, 0); })
    {
      if (impl.mouse_move(event->pos().x(), event->pos().y()))
        event->accept();
    }
    else if constexpr (requires { impl.mouse_move(custom_mouse_event{}); })
    {
      if (impl.mouse_move(make_event(event)))
        event->accept();
    }
    update();
  }

  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override
  {
    if constexpr (requires { impl.mouse_release(0, 0); })
    {
      if (impl.mouse_release(event->pos().x(), event->pos().y()))
        event->accept();
    }
    else if constexpr (requires { impl.mouse_release(custom_mouse_event{}); })
    {
      if (impl.mouse_release(make_event(event)))
        event->accept();
    }
    update();
  }

private:
  Item impl;
};
}
