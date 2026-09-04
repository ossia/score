#include <score/graphics/widgets/QGraphicsHSVChooser.hpp>
#include <score/model/Skin.hpp>
#include <score/tools/Debug.hpp>

#include <QColorDialog>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QPainter>
#include <QPointer>
#include <QTimer>

#include <wobjectimpl.h>

#include <algorithm>

W_OBJECT_IMPL(score::QGraphicsHSVChooser);

namespace score
{

QGraphicsHSVChooser::QGraphicsHSVChooser(QGraphicsItem* parent)
    : hs_zone{100, 100, QImage::Format_ARGB32}

{
}

void QGraphicsHSVChooser::setRect(const QRectF& r)
{
  SCORE_TODO;
}

namespace
{
static QRgb hsvColors[100][100];
static QRgb valueColors[100];
static auto initHsvColors = [] {
  for(int j = 0; j < 100; j++)
  {
    for(int i = 0; i < 100; i++)
    {
      const QRgb col = QColor::fromHsvF(double(i) / 100., double(j) / 100., 1.).rgba();
      hsvColors[i][j] = col;
    }
  }

  for(int j = 0; j < 100; j++)
  {
    const QRgb col = QColor::fromHsvF(-1., 1., double(j) / 100.).rgba();
    valueColors[j] = col;
  }
  return 0;
}();

static QImage& v_zone()
{
  static QImage v_zone = [] {
    QImage v_zone{20, 100, QImage::Format_ARGB32};

    {
      auto img_data = v_zone.bits();
      for(int j = 0; j < 100; j++)
      {
        const QRgb col = valueColors[j];
        for(int i = 0; i < 20; i++)
        {
          img_data[0] = qBlue(col);
          img_data[1] = qGreen(col);
          img_data[2] = qRed(col);
          img_data[3] = 255;
          img_data += 4;
        }
      }
    }

    return v_zone;
  }();
  return v_zone;
}
}
void QGraphicsHSVChooser::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  if(prev_v != v)
  {
    // Redraw the hue chooser with the correct light intensity
    auto img_data = hs_zone.bits();
    for(int j = 0; j < 100; j++)
    {
      for(int i = 0; i < 100; i++)
      {
        const QRgb col = hsvColors[i][j];
        img_data[0] = qBlue(col) * this->v;
        img_data[1] = qGreen(col) * this->v;
        img_data[2] = qRed(col) * this->v;
        img_data[3] = 255;
        img_data += 4;
      }
    }
    prev_v = v;
  }

  painter->drawImage(QPointF{0, 0}, hs_zone);
  painter->drawImage(QPointF{110, 0}, v_zone());

  const auto color = QColor::fromRgbF(m_value[0], m_value[1], m_value[2]).toHsv();
  auto x = color.hsvHueF() * 100.;
  auto y = color.hsvSaturationF() * 100.;
  if(x < 0)
    x = 0;
  auto val_y = color.valueF() * 100.;

  painter->setPen(score::Skin::instance().DarkGray.main.pen0);
  painter->drawLine(QPointF{x, 0.}, QPointF{x, 100.});
  painter->drawLine(QPointF{0, y}, QPointF{100., y});

  painter->drawLine(QPointF{111., val_y}, QPointF{130., val_y});
}

std::array<float, 4> QGraphicsHSVChooser::rgbaValue() const
{
  return m_value;
}
std::array<float, 4> QGraphicsHSVChooser::hsvValue() const
{
  return std::array<float, 4>{float(h), float(s), float(v), 1.0};
}

void QGraphicsHSVChooser::setRgbaValue(std::array<float, 4> v)
{
  m_value = v;
  auto hsv = QColor::fromRgbF(v[0], v[1], v[2], v[3]).toHsv();

  this->h = hsv.hueF();
  this->s = hsv.saturationF();
  this->v = hsv.valueF();
  update();
}
void QGraphicsHSVChooser::setHsvValue(std::array<float, 4> v)
{
  this->h = v[0];
  this->s = v[1];
  this->v = v[2];

  auto rgb = QColor::fromHsvF(this->h, this->s, this->v);
  m_value[0] = rgb.redF();
  m_value[1] = rgb.greenF();
  m_value[2] = rgb.blueF();
  m_value[3] = 1.0f;

  update();
}

void QGraphicsHSVChooser::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  // Left button only: the right one opens the colour dialog on release, and
  // must not repaint the colour on the way there.
  if(event->button() != Qt::LeftButton)
  {
    event->accept();
    return;
  }

  const auto p = event->pos();
  if(p.x() < 100.)
  {
    h = qBound(0., p.x() / 100., 1.);
    s = qBound(0., p.y() / 100., 1.);
    m_grab = true;
  }
  else if(p.x() >= 110 && p.x() < 130)
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
  if(new_v != m_value)
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
  if(m_grab)
  {
    if(p.x() < 100.)
    {
      h = qBound(0., p.x() / 100., 1.);
      s = qBound(0., p.y() / 100., 1.);
    }
    else if(p.x() >= 110 && p.x() < 130)
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
    if(new_v != m_value)
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
  if(m_grab)
  {
    const auto p = event->pos();
    if(p.x() < 100.)
    {
      h = qBound(0., p.x() / 100., 1.);
      s = qBound(0., p.y() / 100., 1.);
    }
    else if(p.x() >= 110 && p.x() < 130)
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
    if(new_v != m_value)
    {
      m_value = new_v;
      update();
    }
    sliderReleased();
    m_grab = false;
  }
  else if(event->button() == Qt::RightButton)
  {
    // Out of the event handler: the dialog runs its own event loop, and the
    // scene is still in the middle of delivering this release.
    QTimer::singleShot(0, this, [this] { showColorDialog(); });
  }
  event->accept();
}

void QGraphicsHSVChooser::showColorDialog()
{
  const auto current = QColor::fromRgbF(
      std::clamp(m_value[0], 0.f, 1.f), std::clamp(m_value[1], 0.f, 1.f),
      std::clamp(m_value[2], 0.f, 1.f));

  // Parented to the view so the dialog is modal to the window the pad is in.
  auto* sc = scene();
  const auto views = sc ? sc->views() : QList<QGraphicsView*>{};
  QWidget* parent = views.isEmpty() ? nullptr : views.front();

  QPointer<QGraphicsHSVChooser> self{this};
  const QColor picked
      = QColorDialog::getColor(current, parent, QObject::tr("Pick a colour"));

  // The dialog ran an event loop: the control may have gone away under it,
  // the way a process can be deleted while one of its controls holds a dialog.
  if(!self || !picked.isValid() || picked == current)
    return;

  setRgbaValue(
      ossia::vec4f{
          {float(picked.redF()), float(picked.greenF()), float(picked.blueF()), 1.f}});

  // Moved then released, as a drag would be: one command on the undo stack.
  sliderMoved();
  sliderReleased();
}

//! QEvent::UngrabMouse: the scene took the implicit grab away and there will be
//! no release to end the drag on. See DefaultGraphicsSliderImpl for what goes
//! wrong if the edit is left open.
bool QGraphicsHSVChooser::sceneEvent(QEvent* event)
{
  if(event->type() == QEvent::UngrabMouse)
  {
    if(m_grab)
    {
      m_grab = false;
      sliderReleased();
    }
  }
  return QGraphicsItem::sceneEvent(event);
}

QRectF QGraphicsHSVChooser::boundingRect() const
{
  return QRectF{0, 0, 140, 100};
}
}
