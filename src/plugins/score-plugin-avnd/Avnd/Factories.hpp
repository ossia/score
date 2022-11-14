#pragma once
#include <Process/Dataflow/ControlWidgets.hpp>

#include <Crousti/Executor.hpp>
#include <Crousti/Layer.hpp>
#include <Crousti/ProcessModel.hpp>
#include <Dataflow/WidgetInletFactory.hpp>

#include <score/graphics/DefaultGraphicsKnobImpl.hpp>
#include <score/graphics/DefaultGraphicsSliderImpl.hpp>
#include <score/graphics/GraphicsSliderBaseImpl.hpp>
#include <score/graphics/widgets/QGraphicsRangeSlider.hpp>
#include <score/graphics/widgets/QGraphicsSlider.hpp>

#include <ossia/detail/for_each.hpp>

namespace oscr
{
template <typename Node>
struct ProcessFactory final : public Process::ProcessFactory_T<oscr::ProcessModel<Node>>
{
  using Process::ProcessFactory_T<oscr::ProcessModel<Node>>::ProcessFactory_T;
};

template <typename Node>
struct ExecutorFactory final
    : public Execution::ProcessComponentFactory_T<oscr::Executor<Node>>
{
  using Execution::ProcessComponentFactory_T<
      oscr::Executor<Node>>::ProcessComponentFactory_T;
};

template <typename N>
using reflect_mapped_controls =
    typename avnd::mapped_control_input_introspection<N>::field_reflections_type;

template <typename Field>
struct NormalizerFromMapper
{
  static inline constexpr auto mapper = avnd::get_mapper<Field>();
  static float map(float val) noexcept { return mapper.map(val); }
  static float unmap(float val) noexcept { return mapper.unmap(val); }

  template <typename T>
  static float to01(const T& slider, float val) noexcept
  {
    const auto min = slider.getMin();
    const auto max = slider.getMax();

    // [a; b] -> [0; 1] in linear space
    const auto in_01 = (val - min) / (max - min);

    // [0; 1] -> [0; 1] in e.g. log space
    return map(in_01);
  }

  template <typename T>
  static float from01(const T& slider, float in_01) noexcept
  {
    const auto min = slider.getMin();
    const auto max = slider.getMax();

    // [0; 1] in e.g. log space -> [0; 1] in linear space
    const auto unmapped = unmap(in_01);

    // [0; 1] in linear space -> [a; b]
    return min + unmapped * (max - min);
  }
};

template <typename Field>
class CustomTextGraphicsSlider final : public score::QGraphicsSlider
{
public:
  using score::QGraphicsSlider::QGraphicsSlider;

  double getMin() const noexcept { return this->min; }
  double getMax() const noexcept { return this->max; }
  double unmap(double v) const noexcept
  {
    return NormalizerFromMapper<Field>::to01(*this, v);
  }
  double map(double v) const noexcept
  {
    return NormalizerFromMapper<Field>::from01(*this, v);
  }

private:
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override
  {
    score::DefaultGraphicsSliderImpl::paint(
        *this, score::Skin::instance(), QString::number(map(m_value), 'f', 3), painter,
        widget);
  }

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override
  {
    score::DefaultGraphicsSliderImpl::mousePressEvent(*this, event);
  }

  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override
  {
    score::DefaultGraphicsSliderImpl::mouseMoveEvent(*this, event);
  }

  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override
  {
    score::DefaultGraphicsSliderImpl::mouseReleaseEvent(*this, event);
  }

  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override
  {
    event->accept();
  }
};

template <typename Field>
class CustomTextGraphicsKnob final : public score::QGraphicsKnob
{
public:
  using score::QGraphicsKnob::QGraphicsKnob;

  double getMin() const noexcept { return this->min; }
  double getMax() const noexcept { return this->max; }
  double unmap(double v) const noexcept
  {
    return NormalizerFromMapper<Field>::to01(*this, v);
  }
  double map(double v) const noexcept
  {
    return NormalizerFromMapper<Field>::from01(*this, v);
  }

private:
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override
  {
    const double val = map(m_value);
    const double abs = std::abs(val);
    int pres = abs < 10. ? 3 : abs < 100. ? 2 : abs < 1000. ? 1 : 0;
    score::DefaultGraphicsKnobImpl::paint(
        *this, score::Skin::instance(), QString::number(val, 'f', pres), painter,
        widget);
  }

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override
  {
    score::DefaultGraphicsKnobImpl::mousePressEvent(*this, event);
  }

  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override
  {
    score::DefaultGraphicsKnobImpl::mouseMoveEvent(*this, event);
  }

  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override
  {
    score::DefaultGraphicsKnobImpl::mouseReleaseEvent(*this, event);
  }

  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override
  {
    event->accept();
  }
};

template <typename Field>
struct MatchingWidget
{
  using type = CustomTextGraphicsSlider<Field>;
};
template <typename Field>
requires requires
{
  Field::widget::knob;
}
struct MatchingWidget<Field>
{
  using type = CustomTextGraphicsKnob<Field>;
};

template <typename Node, typename Refl>
struct CustomControlFactory;
template <typename Node, std::size_t N, typename Field>
struct CustomControlFactory<Node, avnd::field_reflection<N, Field>>
    : public Dataflow::WidgetInletFactory<
          oscr::CustomFloatControl<Node, avnd::field_index<N>>,
          WidgetFactory::FloatControl<
              typename MatchingWidget<Field>::type, NormalizerFromMapper<Field>, true>>
{
};

template <typename... Nodes>
std::vector<std::unique_ptr<score::InterfaceBase>>
instantiate_fx(const score::ApplicationContext& ctx, const score::InterfaceKey& key)
{
  std::vector<std::unique_ptr<score::InterfaceBase>> v;

  if(key == Execution::ProcessComponentFactory::static_interfaceKey())
  {
    //static_assert((requires { std::declval<Nodes>().run({}, {}); } && ...));
    (v.emplace_back(static_cast<Execution::ProcessComponentFactory*>(
         new oscr::ExecutorFactory<Nodes>())),
     ...);
  }
  else if(key == Process::ProcessModelFactory::static_interfaceKey())
  {
    (v.emplace_back(
         static_cast<Process::ProcessModelFactory*>(new oscr::ProcessFactory<Nodes>())),
     ...);
  }
  else if(key == Process::LayerFactory::static_interfaceKey())
  {
    auto fun = [&]<typename type>() {
      if constexpr(avnd::has_ui<type>)
      {
        v.emplace_back(
            static_cast<Process::LayerFactory*>(new oscr::LayerFactory<type>()));
      }
    };
    (fun.template operator()<Nodes>(), ...);
  }
  else if(key == Process::PortFactory::static_interfaceKey())
  {
    // Go through all the process's control inputs with a mapper
    // Generate the matching WidgetInletFactory
    using namespace boost::mp11;

    auto fun = [&]<typename N, typename... Fields>(avnd::typelist<Fields...>) {
      (v.emplace_back(
           static_cast<Process::PortFactory*>(new CustomControlFactory<N, Fields>{})),
       ...);
    };
    (fun.template operator()<Nodes>(reflect_mapped_controls<Nodes>{}), ...);
  }

  return v;
}

template <typename T>
void custom_factories(
    std::vector<std::unique_ptr<score::InterfaceBase>>& fx,
    const score::ApplicationContext& ctx, const score::InterfaceKey& key);

}

namespace Avnd = oscr;
