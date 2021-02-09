#include <score/graphics/widgets/QGraphicsMultiSlider.hpp>
#include <score/graphics/DefaultGraphicsSliderImpl.hpp>
#include <score/model/Skin.hpp>

#include <ossia/detail/math.hpp>
#include <ossia/network/value/value_conversion.hpp>
#include <ossia/network/domain/domain_functions.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsMultiSlider);

namespace score
{
template<typename T>
struct ValueAssigner;

template<>
struct ValueAssigner<float>
{
  float& value;
  operator double() { return value; }
  float& operator=(double d) { return value = d; }
};
template<>
struct ValueAssigner<int>
{
  int& value;
  operator double() { return value; }
  int& operator=(double d) { return value = d; }
};
template<>
struct ValueAssigner<ossia::value>
{
  ossia::value& value;
  operator double() { return ossia::convert<float>(value); }
  ossia::value& operator=(double d) { return value = float(d); }
};
template<typename T>
double operator*(double lhs, const ValueAssigner<T>& rhs) noexcept { return lhs * rhs.value; }

template<typename T>
struct SliderWrapper
{
  QGraphicsMultiSlider& parent;
  ValueAssigner<T> m_value{};
  QRectF rect{defaultSliderSize};

  RightClickImpl* impl{};

  double& min = parent.min;
  double& max = parent.max;

  double unmap(double v) const noexcept { return (v - min) / (max - min); }
  double map(double v) const noexcept { return (v * (max - min)) + min; }

  QGraphicsScene* scene() { return parent.scene(); }

  bool m_grab{};
  // TODO refactor with QGraphicsSliderBase
  double getHandleX() const noexcept
  {
    return sliderRect().width() * m_value;
  }
  QRectF sliderRect() const noexcept {
    return QRectF{rect.x(), rect.y(), rect.width(), 8};
  }

  bool isInHandle(QPointF p) const noexcept {
    return rect.contains(p);
  }

  QRectF handleRect() const noexcept {
    auto r = sliderRect();
    r.setWidth(std::max(0., getHandleX()));
    return r;
  }
  QRectF boundingRect() const noexcept {
    return rect;
  }

  void update() const noexcept
  {
    parent.update();
  }

  void sliderMoved() const noexcept
  {
    parent.sliderMoved();
  }

  void sliderReleased() const noexcept
  {
    parent.sliderReleased();
  }

  QGraphicsMultiSlider* operator&() const noexcept { return &parent; }
};

struct PaintVisitor
{
  QGraphicsMultiSlider& self;
  QPainter& painter;
  QWidget& widget;
  score::Skin& skin = score::Skin::instance();

  template<std::size_t N>
  void operator()(std::array<float, N>& v) const noexcept
  {
    for(std::size_t i = 0; i < N; i++)
    {
      SliderWrapper<float> slider{self, {v[i]}};
      slider.rect.moveTop(i * (defaultSliderSize.height() + 4));
      DefaultGraphicsSliderImpl::paint(
            slider,
            skin,
            QString::number(self.min + v[i] * (self.max - self.min), 'f', 3),
            &painter,
            &widget);
    }
  }

  void operator()(const std::vector<ossia::value>& v) const noexcept
  {
    SCORE_TODO;
  }
  void operator()(float v) const noexcept
  {
    SCORE_TODO;
  }
  void operator()(int v) const noexcept
  {
    SCORE_TODO;
  }
  template<typename T>
  void operator()(const T&) const noexcept
  {
    SCORE_TODO;
  }
  void operator()() const noexcept
  {

  }
};


struct SizeVisitor
{
  const QGraphicsMultiSlider& self;
  template<std::size_t N>
  QRectF operator()(const std::array<float, N>& v) const noexcept
  {
    return {0, 0, 100, N * (defaultSliderSize.height() + 4)};
  }

  QRectF operator()(const std::vector<ossia::value>& v) const noexcept
  {
    SCORE_TODO;
    return defaultSliderSize;
  }
  QRectF operator()(float v) const noexcept
  {
    return defaultSliderSize;
  }
  QRectF operator()(int v) const noexcept
  {
    return defaultSliderSize;
  }
  template<typename T>
  QRectF operator()(const T&) const noexcept
  {
    return defaultSliderSize;
  }
  QRectF operator()() const noexcept
  {
    return defaultSliderSize;
  }
};

template<auto Event>
struct EventVisitor
{
  QGraphicsMultiSlider& self;
  QGraphicsSceneMouseEvent& event;

  template<std::size_t N>
  void operator()(std::array<float, N>& v) const noexcept
  {
    for(std::size_t i = 0; i < N; i++)
    {
      SliderWrapper<float> slider{self, {v[i]}};
      slider.m_grab = (self.m_grab == i);
      bool had_grab{slider.m_grab};
      slider.rect.moveTop(i * (defaultSliderSize.height() + 4));
      Event(slider, &event);

      if(slider.m_grab)
      {
        self.m_grab = i;
      }
      else
      {
        if(had_grab)
          self.m_grab = -1;
      }
    }
  }

  void operator()(std::vector<ossia::value>& v) const noexcept
  {
    SCORE_TODO;
  }
  void operator()(float v) const noexcept
  {
    SCORE_TODO;
  }
  void operator()(int v) const noexcept
  {
    SCORE_TODO;
  }
  template<typename U>
  void operator()(const U&) const noexcept
  {
    SCORE_TODO;
  }
  void operator()() const noexcept
  {

  }
};

QGraphicsMultiSlider::QGraphicsMultiSlider(QGraphicsItem* parent)
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
}

void QGraphicsMultiSlider::setPoint(const QPointF& r)
{
  SCORE_TODO;
}

void QGraphicsMultiSlider::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  m_value.apply(PaintVisitor{*this, *painter, *widget});
}

ossia::value QGraphicsMultiSlider::value() const
{
  return m_value;
}

void QGraphicsMultiSlider::setRange(const ossia::value& min, const ossia::value& max)
{
  this->min = ossia::convert<float>(min);
  this->max = ossia::convert<float>(max);
}

void QGraphicsMultiSlider::setRange(const ossia::domain& dom)
{
  auto [min, max] = ossia::get_float_minmax(dom);

  if(min)
    this->min = *min;
  if(max)
    this->max = *max;
}

void QGraphicsMultiSlider::setValue(ossia::value v)
{
  prepareGeometryChange();
  m_value = v;
  update();
}

void QGraphicsMultiSlider::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  m_value.apply(EventVisitor<&DefaultGraphicsSliderImpl::mousePressEvent<SliderWrapper<float>>>{*this, *event});
}

void QGraphicsMultiSlider::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  m_value.apply(EventVisitor<&DefaultGraphicsSliderImpl::mouseMoveEvent<SliderWrapper<float>>>{*this, *event});
}

void QGraphicsMultiSlider::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  m_value.apply(EventVisitor<&DefaultGraphicsSliderImpl::mouseReleaseEvent<SliderWrapper<float>>>{*this, *event});
}

QRectF QGraphicsMultiSlider::boundingRect() const
{
  return m_value.apply(SizeVisitor{*this});
}
}
