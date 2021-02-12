#include <score/graphics/widgets/QGraphicsHSVChooser.hpp>
#include <score/model/Skin.hpp>
#include <score/tools/Debug.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsHSVChooser);

namespace score
{
QGraphicsHSVChooser::QGraphicsHSVChooser(QGraphicsItem* parent) { }

void QGraphicsHSVChooser::setRect(const QRectF& r)
{
  SCORE_TODO;
}

namespace
{
static QRgb hsvColors[100][100];
static QRgb valueColors[100];
static auto initHsvColors = [] {
  for (int j = 0; j < 100; j++)
  {
    for (int i = 0; i < 100; i++)
    {
      const QRgb col = QColor::fromHsvF(double(i) / 100., double(j) / 100., 1.).rgba();
      hsvColors[i][j] = col;
    }
  }

  for (int j = 0; j < 100; j++)
  {
    const QRgb col = QColor::fromHsvF(-1., 1., double(j) / 100.).rgba();
    valueColors[j] = col;
  }
  return 0;
}();
}
void QGraphicsHSVChooser::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  static QImage hs_zone{100, 100, QImage::Format_ARGB32};
  {
    auto img_data = hs_zone.bits();
    for (int j = 0; j < 100; j++)
    {
      for (int i = 0; i < 100; i++)
      {
        const QRgb col = hsvColors[i][j];
        img_data[0] = qBlue(col) * this->v;
        img_data[1] = qGreen(col) * this->v;
        img_data[2] = qRed(col) * this->v;
        img_data[3] = 255;
        img_data += 4;
      }
    }
  }

  static QImage v_zone{20, 100, QImage::Format_ARGB32};
  {
    auto img_data = v_zone.bits();
    for (int j = 0; j < 100; j++)
    {
      const QRgb col = valueColors[j];
      for (int i = 0; i < 20; i++)
      {
        img_data[0] = qBlue(col);
        img_data[1] = qGreen(col);
        img_data[2] = qRed(col);
        img_data[3] = 255;
        img_data += 4;
      }
    }
  }

  painter->drawImage(QPointF{0, 0}, hs_zone);
  painter->drawImage(QPointF{110, 0}, v_zone);

  const auto color = QColor::fromRgbF(m_value[0], m_value[1], m_value[2]).toHsv();
  auto x = color.hsvHueF() * 100.;
  auto y = color.hsvSaturationF() * 100.;
  auto val_y = color.valueF() * 100.;

  painter->setPen(score::Skin::instance().DarkGray.main.pen0);
  painter->drawLine(QPointF{x, 0.}, QPointF{x, 100.});
  painter->drawLine(QPointF{0, y}, QPointF{100., y});

  painter->drawLine(QPointF{111., val_y}, QPointF{130., val_y});
}

std::array<float, 4> QGraphicsHSVChooser::value() const
{
  return m_value;
}

void QGraphicsHSVChooser::setValue(std::array<float, 4> v)
{
  m_value = v;
  auto hsv = QColor::fromRgbF(v[0], v[1], v[2], v[3]).toHsv();

  this->h = hsv.hueF();
  this->s = hsv.saturationF();
  this->v = hsv.valueF();
  update();
}

void QGraphicsHSVChooser::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  const auto p = event->pos();
  if (p.x() < 100.)
  {
    h = qBound(0., p.x() / 100., 1.);
    s = qBound(0., p.y() / 100., 1.);
    m_grab = true;
  }
  else if (p.x() >= 110 && p.x() < 130)
  {
    v = qBound(0., p.y() / 100., 1.);
    m_grab = true;
  }

  const auto rgba = QColor::fromHsvF(h, s, v, 1.);
  auto new_v = m_value;
  auto& [r, g, b, a] = new_v;
  r = rgba.redF();
  g = rgba.greenF();
  b = rgba.blueF();
  a = 1.;
  if (new_v != m_value)
  {
    m_value = new_v;
    sliderMoved();
    update();
  }
  event->accept();
}

void QGraphicsHSVChooser::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  const auto p = event->pos();
  if (m_grab)
  {
    if (p.x() < 100.)
    {
      h = qBound(0., p.x() / 100., 1.);
      s = qBound(0., p.y() / 100., 1.);
    }
    else if (p.x() >= 110 && p.x() < 130)
    {
      v = qBound(0., p.y() / 100., 1.);
    }

    const auto rgba = QColor::fromHsvF(h, s, v, 1.);
    auto new_v = m_value;
    auto& [r, g, b, a] = new_v;
    r = rgba.redF();
    g = rgba.greenF();
    b = rgba.blueF();
    a = 1.;
    if (new_v != m_value)
    {
      m_value = new_v;
      sliderMoved();
      update();
    }
  }
  event->accept();
}

void QGraphicsHSVChooser::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if (m_grab)
  {
    const auto p = event->pos();
    if (p.x() < 100.)
    {
      h = qBound(0., p.x() / 100., 1.);
      s = qBound(0., p.y() / 100., 1.);
    }
    else if (p.x() >= 110 && p.x() < 130)
    {
      v = qBound(0., p.y() / 100., 1.);
    }
    const auto rgba = QColor::fromHsvF(h, s, v, 1.);
    auto new_v = m_value;
    auto& [r, g, b, a] = new_v;
    r = rgba.redF();
    g = rgba.greenF();
    b = rgba.blueF();
    a = 1.;
    if (new_v != m_value)
    {
      m_value = new_v;
      update();
    }
    sliderReleased();
    m_grab = false;
  }
  event->accept();
}

QRectF QGraphicsHSVChooser::boundingRect() const
{
  return QRectF{0, 0, 140, 100};
}
}
