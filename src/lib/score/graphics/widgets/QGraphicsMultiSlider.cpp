#include <score/graphics/widgets/QGraphicsMultiSlider.hpp>
#include <score/graphics/DefaultGraphicsSliderImpl.hpp>

#include <ossia/detail/math.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsMultiSlider);

namespace score
{

struct apply_value {

  operator double() const noexcept
  {
    return 0.;
  }
  double& operator=(double other)
  {
    static double d;
    return d;
  }
};


struct SliderWrapper
{
  QGraphicsMultiSlider& parent;
  apply_value m_value{};
  double value{};
  QRectF rect{defaultSliderSize};

  RightClickImpl* impl{};

  double& min = parent.min;
  double& max = parent.max;

  double unmap(double v) const noexcept { return (v - min) / (max - min); }
  double map(double v) const noexcept { return (v * (max - min)) + min; }

  QGraphicsScene* scene() { return parent.scene(); }


  bool m_grab{false}; // TODO
  // TODO refactor with QGraphicsSliderBase
  double getHandleX() const noexcept
  {
    return sliderRect().width() * value;
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

  void update()
  {
    parent.update();
  }
  void valueChanged(double arg_1)
  {

  }
  void sliderMoved()
  {

  }
  void sliderReleased()
  {

  }

  QGraphicsMultiSlider* operator&() { return &parent; }
};

struct PaintVisitor
{
  QGraphicsMultiSlider& self;
  QPainter& painter;
  QWidget& widget;
  score::Skin& skin = score::Skin::instance();

  template<std::size_t N>
  void operator()(const std::array<float, N>& v) const noexcept
  {
    for(std::size_t i = 0; i < N; i++)
    {
      SliderWrapper slider{self};
      slider.rect.setY(i * (defaultSliderSize.height() + 4));
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

  }
  void operator()(float v) const noexcept
  {

  }
  void operator()(int v) const noexcept
  {

  }
  template<typename T>
  void operator()(const T&) const noexcept
  {

  }
  void operator()() const noexcept
  {

  }
};

template<auto Event>
struct EventVisitor
{
  QGraphicsMultiSlider& self;
  QGraphicsSceneMouseEvent& event;

  template<std::size_t N>
  void operator()(const std::array<float, N>& v) const noexcept
  {
    for(std::size_t i = 0; i < N; i++)
    {
      SliderWrapper slider{self};
      slider.rect.setY(i * (defaultSliderSize.height() + 4));
      Event(slider, &event);
    }
  }

  void operator()(const std::vector<ossia::value>& v) const noexcept
  {

  }
  void operator()(float v) const noexcept
  {

  }
  void operator()(int v) const noexcept
  {

  }
  template<typename U>
  void operator()(const U&) const noexcept
  {

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

void QGraphicsMultiSlider::setValue(ossia::value v)
{
  m_value = v;
  update();
}

void QGraphicsMultiSlider::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  m_value.apply(EventVisitor<&DefaultGraphicsSliderImpl::mousePressEvent<SliderWrapper>>{*this, *event});
}

void QGraphicsMultiSlider::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  m_value.apply(EventVisitor<&DefaultGraphicsSliderImpl::mouseMoveEvent<SliderWrapper>>{*this, *event});
}

void QGraphicsMultiSlider::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  m_value.apply(EventVisitor<&DefaultGraphicsSliderImpl::mouseReleaseEvent<SliderWrapper>>{*this, *event});
}

QRectF QGraphicsMultiSlider::boundingRect() const
{
  return QRectF{0, 0, 100, 100};
}
}
