#include <score/graphics/DefaultGraphicsSliderImpl.hpp>
#include <score/graphics/widgets/QGraphicsPathGeneratorXY.hpp>
#include <score/serialization/StringConstants.hpp>
#include <score/tools/Debug.hpp>
#include <score/model/Skin.hpp>

#include <ossia/detail/math.hpp>
#include <ossia/network/domain/domain_functions.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <cmath>
#include <utility>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsPathGeneratorXY);

namespace score
{
namespace
{
constexpr double TWO_PI = ossia::two_pi;

//! Number of segments used to approximate a curved trajectory.
constexpr int trajectorySamples = 96;

constexpr double markerSize = 10.;

ossia::vec2f node_at(const std::vector<ossia::value>& nodes, std::size_t i) noexcept
{
  if(i < nodes.size())
    if(auto* v = nodes[i].target<ossia::vec2f>())
      return *v;
  return ossia::vec2f{0.5f, 0.5f};
}
}

score::QGraphicsPathGeneratorXY::QGraphicsPathGeneratorXY(QGraphicsItem* parent)
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
}

const std::vector<ossia::value>&
score::QGraphicsPathGeneratorXY::sources() const noexcept
{
  if(m_hasExec)
    if(auto* v = m_execValue.target<std::vector<ossia::value>>())
      return *v;
  return tab;
}

QPointF score::QGraphicsPathGeneratorXY::pathPoint(
    const std::vector<ossia::value>& nodes, double u) const noexcept
{
  const ossia::vec2f a = node_at(nodes, 0);
  const double rx = m_radiusX;
  const double ry = m_radiusY;
  const double phi = TWO_PI * m_phase;

  double x = a[0], y = a[1];
  switch(m_pathMode)
  {
    case Linear:
    {
      const ossia::vec2f b = nodes.size() > 1 ? node_at(nodes, 1) : a;
      x = a[0] + (b[0] - a[0]) * u;
      y = a[1] + (b[1] - a[1]) * u;
      break;
    }

    case Circle:
    {
      const double angle = TWO_PI * (1. - u) + phi;
      x = a[0] + rx * std::cos(angle);
      y = a[1] + ry * std::sin(angle);
      break;
    }

    case Spiral:
    {
      const double angle = 2. * TWO_PI * u + phi;
      x = a[0] + rx * u * std::cos(angle);
      y = a[1] + ry * u * std::sin(angle);
      break;
    }

    case Lissajous:
    {
      const double th = TWO_PI * u;
      x = a[0] + rx * std::sin(m_ratioX * th + phi);
      y = a[1] + ry * std::sin(m_ratioY * th);
      break;
    }

    case Rose:
    {
      const double th = TWO_PI * m_ratioY * u + phi;
      const double r = std::cos(m_ratioX * th);
      x = a[0] + rx * r * std::cos(th);
      y = a[1] + ry * r * std::sin(th);
      break;
    }

    case Polygon:
    {
      const int n = ossia::max(3, m_ratioX);
      const double s = u * n;
      const int k = ossia::min((int)s, n - 1);
      const double f = s - k;
      const double a0 = TWO_PI * k / n + phi;
      const double a1 = TWO_PI * (k + 1) / n + phi;
      const double x0 = std::cos(a0), y0 = std::sin(a0);
      const double x1 = std::cos(a1), y1 = std::sin(a1);
      x = a[0] + rx * (x0 + (x1 - x0) * f);
      y = a[1] + ry * (y0 + (y1 - y0) * f);
      break;
    }
  }

  return QPointF{x * width(), (1. - y) * height()};
}

void score::QGraphicsPathGeneratorXY::recomputePaths()
{
  const auto& src = sources();
  m_paths.clear();
  m_paths.reserve(src.size());

  for(const auto& s : src)
  {
    QPainterPath path;
    if(const auto* nodes = s.target<std::vector<ossia::value>>(); nodes && !nodes->empty())
    {
      path.moveTo(pathPoint(*nodes, 0.));
      if(m_pathMode == Linear)
      {
        path.lineTo(pathPoint(*nodes, 1.));
      }
      else
      {
        for(int i = 1; i <= trajectorySamples; i++)
          path.lineTo(pathPoint(*nodes, double(i) / trajectorySamples));
      }
    }
    m_paths.push_back(std::move(path));
  }
}

QRectF score::QGraphicsPathGeneratorXY::progressRect() const noexcept
{
  if(!m_hasProgress)
    return {};

  QRectF r;
  for(const auto& s : sources())
  {
    const auto* nodes = s.target<std::vector<ossia::value>>();
    if(!nodes || nodes->empty())
      continue;

    const QPointF p = pathPoint(*nodes, m_progress);
    r = r.united(QRectF{
        p.x() - markerSize, p.y() - markerSize, 2 * markerSize, 2 * markerSize});
  }
  return r;
}

void score::QGraphicsPathGeneratorXY::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto& skin = score::Skin::instance();
  const auto& src = sources();

  // The centre and radii can put a trajectory outside of the item.
  painter->setClipRect(QRectF(0, 0, width(), height()));
  painter->fillRect(QRectF(0, 0, width(), height()), QColor(skin.Dark.color()));

  for(int64_t s = std::ssize(src); s-- > 0;)
  {
    const auto* nodes = src[s].target<std::vector<ossia::value>>();
    if(!nodes || nodes->empty())
      continue;

    const bool isSelectedSource = (s == selectedSource);

    const QColor startColor = isSelectedSource ? skin.Base3.color()
                                               : skin.Base3.color().darker(300);
    const QColor nodeColor = isSelectedSource ? skin.Warn3.color()
                                              : skin.Warn3.color().darker(250);

    if(s < std::ssize(m_paths))
    {
      painter->setPen(QPen{isSelectedSource ? skin.Base3.color()
                                            : skin.Base3.color().darker(250),
                           1.});
      painter->setBrush(Qt::NoBrush);
      painter->drawPath(m_paths[s]);
    }

    for(std::size_t c = 0; c < nodes->size(); c++)
    {
      const ossia::vec2f cursorXY = node_at(*nodes, c);

      QRectF cursorRect(
          (cursorXY[0] - cursorSize.x / 2) * width(),
          (1 - cursorXY[1] - cursorSize.y / 2) * height(), cursorSize.x * width(),
          cursorSize.y * height());

      painter->fillRect(cursorRect, c == 0 ? startColor : nodeColor);
    }

    if(m_hasProgress)
    {
      const QPointF p = pathPoint(*nodes, m_progress);
      painter->setPen(QPen{skin.Base1.color(), 1.});
      painter->setBrush(skin.Emphasis2.color());
      painter->drawEllipse(p, markerSize / 2., markerSize / 2.);
    }
  }
}

ossia::value score::QGraphicsPathGeneratorXY::value() const
{
  return m_value;
}

void score::QGraphicsPathGeneratorXY::setRange(const ossia::value& min, const ossia::value& max)
{
  this->min = ossia::convert<float>(min);
  this->max = ossia::convert<float>(max);
}

void score::QGraphicsPathGeneratorXY::setRange(const ossia::domain& dom)
{
  auto [min, max] = ossia::get_float_minmax(dom);

  if (min)
    this->min = *min;
  if (max)
    this->max = *max;
}

void score::QGraphicsPathGeneratorXY::setValue(ossia::value v)
{
  m_value = std::move(v);
  if(auto* t = m_value.target<std::vector<ossia::value>>())
    tab = *t;
  else
    tab.clear();

  if(!ossia::valid_index(selectedSource, tab))
    selectedSource = -1;

  recomputePaths();
  update();
}

void score::QGraphicsPathGeneratorXY::commitTab()
{
  m_value = std::vector<ossia::value>(tab.begin(), tab.end());
  recomputePaths();
}

void score::QGraphicsPathGeneratorXY::setExecutionValue(const ossia::value& v)
{
  if(m_hasExec && v == m_execValue)
    return;

  m_execValue = v;
  m_hasExec = true;
  recomputePaths();
  update();
}

void score::QGraphicsPathGeneratorXY::setExecutionProgress(double v)
{
  if(m_hasProgress && std::abs(v - m_progress) < 1e-9)
    return;

  const QRectF before = progressRect();
  m_progress = v;
  m_hasProgress = true;
  update(before.united(progressRect()));
}

void score::QGraphicsPathGeneratorXY::resetExecution()
{
  if(!m_hasExec && !m_hasProgress)
    return;

  m_hasProgress = false;
  if(m_hasExec)
  {
    m_hasExec = false;
    m_execValue = ossia::value{};
    recomputePaths();
  }
  update();
}

void score::QGraphicsPathGeneratorXY::setPathMode(int mode)
{
  if(mode == m_pathMode)
    return;
  m_pathMode = mode;
  recomputePaths();
  update();
}

void score::QGraphicsPathGeneratorXY::setRadii(float x, float y)
{
  if(x == m_radiusX && y == m_radiusY)
    return;
  m_radiusX = x;
  m_radiusY = y;
  recomputePaths();
  update();
}

void score::QGraphicsPathGeneratorXY::setRatioX(int r)
{
  if(r == m_ratioX)
    return;
  m_ratioX = r;
  recomputePaths();
  update();
}

void score::QGraphicsPathGeneratorXY::setRatioY(int r)
{
  if(r == m_ratioY)
    return;
  m_ratioY = r;
  recomputePaths();
  update();
}

void score::QGraphicsPathGeneratorXY::setPhase(float p)
{
  if(p == m_phase)
    return;
  m_phase = p;
  recomputePaths();
  update();
}

void score::QGraphicsPathGeneratorXY::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  if (m_grab && ossia::valid_index(selectedSource, tab)) // The cursor’s position is updated as the mouse moves, constrained within the widget’s bounds.
  {
    tab[selectedSource].get<std::vector<ossia::value>>()[selectedCursor] =
                                  ossia::vec2f{
                                       (float)std::clamp((event->pos().x() / width()), 0., 1.),
                                       (float)std::clamp(1-(event->pos().y() / height()), 0., 1.)};

    commitTab();
    sliderMoved();
    update();
  }
}

void score::QGraphicsPathGeneratorXY::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  float x = event->pos().x();
  float y = event->pos().y();

  // If the left mouse button is pressed on an existing cursor, that cursor can be moved.
  if (event->button() & Qt::LeftButton)
  {
    for (int sourceIndex = 0; sourceIndex < std::ssize(tab); sourceIndex++){
      for (int cursorIndex = 0; cursorIndex < std::ssize(tab[sourceIndex].get<std::vector<ossia::value>>()); cursorIndex++)
    {
      const auto& CursorXY = tab[sourceIndex].get<std::vector<ossia::value>>()[cursorIndex].get<ossia::vec2f>();
      if ((CursorXY[0] - 0.02f) * width() <= (float)x &&
         (float)x <= (CursorXY[0] + 0.02f) * width() &&
         (1-CursorXY[1] - 0.02f) * height() <= (float)y &&
         (float)y <= (1-CursorXY[1] + 0.02f) * height())
      {
        m_grab = true;
        selectedSource = sourceIndex;
        selectedCursor = cursorIndex;
        mouseMoveEvent(event);
        return;
      }
    }
    }
    // Else if the press occurs at an empty area, a new cursor will be created at that position.
    tab.push_back(std::vector<ossia::value>{ossia::vec2f{(float)(x / width()), 1-(float)(y / width())}, ossia::vec2f{(float)(x / width()), 1-(float)(y / width())}});

    commitTab();
    m_grab = true;
    selectedSource = tab.size() - 1;
    selectedCursor = 1;
    mouseMoveEvent(event);
    return;
  }
  else if (event->button() & Qt::RightButton) //If the right mouse button is pressed over a cursor, the cursor is deleted.
  {
    m_grab = false;
    for (int v = 0; v < std::ssize(tab); v++){
      for (int c = 0; c < std::ssize(tab[v].get<std::vector<ossia::value>>()); c++)
    {
      const auto& CursorXY = tab[v].get<std::vector<ossia::value>>()[c].get<ossia::vec2f>();
      if ((CursorXY[0] - 0.02f) * width() <= (float)x &&
         (float)x <= (CursorXY[0] + 0.02f) * width() &&
         (1-CursorXY[1] - 0.02f) * height() <= (float)y &&
         (float)y <= (1-CursorXY[1] + 0.02f) * height())
      {
        tab.erase(tab.begin() + v);
        if(selectedSource == v)
          selectedSource = -1;
        else if(selectedSource > v)
          selectedSource--;

        commitTab();
        sliderMoved();
        sliderReleased();
        update();
        return;
      }}

    }
    return;
  }
  update();
}

void score::QGraphicsPathGeneratorXY::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if ((event->button() & Qt::LeftButton) && m_grab)
  {
    m_grab = false;
    mouseMoveEvent(event);
  }

  update();
  sliderReleased();
}

//! QEvent::UngrabMouse: the scene took the implicit grab away and there will be
//! no release to end the drag on. See DefaultGraphicsSliderImpl for what goes
//! wrong if the edit is left open.
bool score::QGraphicsPathGeneratorXY::sceneEvent(QEvent* event)
{
  if(event->type() == QEvent::UngrabMouse)
  {
    if(m_grab)
    {
      m_grab = false;
      update();
      sliderReleased();
    }
  }
  return QGraphicsItem::sceneEvent(event);
}

QRectF score::QGraphicsPathGeneratorXY::boundingRect() const
{
  return QRectF(0, 0, width(), height());
}
}
