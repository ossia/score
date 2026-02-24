#pragma once
#include <Process/Dataflow/ControlWidgets.hpp>

#include <Scenario/Commands/SetControllerControlValue.hpp>

#include <Crousti/Executor.hpp>
#include <Crousti/Layer.hpp>
#include <Crousti/ProcessModel.hpp>
#include <Crousti/ScoreLayer.hpp>
#include <Dataflow/WidgetInletFactory.hpp>

#include <score/graphics/DefaultGraphicsKnobImpl.hpp>
#include <score/graphics/DefaultGraphicsSliderImpl.hpp>
#include <score/graphics/GraphicsSliderBaseImpl.hpp>
#include <score/graphics/widgets/QGraphicsRangeSlider.hpp>
#include <score/graphics/widgets/QGraphicsSlider.hpp>

#include <ossia/detail/for_each.hpp>

namespace oscr
{

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
  requires requires { Field::widget::knob; }
struct MatchingWidget<Field>
{
  using type = CustomTextGraphicsKnob<Field>;
};

template <typename Node, typename Refl>
struct CustomControlFactory;

template <typename Node, std::size_t N, typename Field>
  requires(!avnd::controller_interaction_port<Field>)
struct CustomControlFactory<Node, avnd::field_reflection<N, Field>>
    : public Dataflow::WidgetInletFactory<
          oscr::CustomFloatControl<Node, avnd::field_index<N>>,
          WidgetFactory::FloatControl<
              typename MatchingWidget<Field>::type, NormalizerFromMapper<Field>, true>>
{
};

struct ControllerIntSpinBox
{
  static Process::PortItemLayout layout() noexcept
  {
    return Process::DefaultControlLayouts::spinbox();
  }

  template <typename T>
  static auto make_widget(
      T& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    auto sl = new QSpinBox{parent};
    WidgetFactory::bindIntDomain(inlet, inlet, *sl);
    sl->setValue(ossia::convert<int>(inlet.value()));
    sl->setContentsMargins(0, 0, 0, 0);

    QObject::connect(
        sl, SignalUtils::QSpinBox_valueChanged_int(), context, [&inlet, &ctx](int val) {
      CommandDispatcher<>{ctx.commandStack}.submit<Scenario::SetControllerControlValue>(
          inlet, val, ctx);
    });

    QObject::connect(&inlet, &T::valueChanged, sl, [sl](const ossia::value& val) {
      sl->setValue(ossia::convert<int>(val));
    });

    return sl;
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& slider, Control_T& inlet, const score::DocumentContext& ctx,
      QGraphicsItem* parent, QObject* context)
  {
    auto sl = new score::QGraphicsIntSpinbox{nullptr};
    WidgetFactory::initWidgetProperties(inlet, *sl);
    WidgetFactory::bindIntDomain(slider, inlet, *sl);
    sl->setValue(ossia::convert<int>(inlet.value()));

    QObject::connect(
        sl, &score::QGraphicsIntSpinbox::sliderMoved, context, [=, &inlet, &ctx] {
      sl->moving = true;
      ctx.dispatcher.submit<Scenario::SetControllerControlValue>(
          inlet, sl->value(), ctx);
    });
    QObject::connect(
        sl, &score::QGraphicsIntSpinbox::sliderReleased, context, [&ctx, sl]() {
      ctx.dispatcher.commit();
      sl->moving = false;
    });

    QObject::connect(&inlet, &Control_T::valueChanged, sl, [=](const ossia::value& val) {
      if(!sl->moving)
        sl->setValue(ossia::convert<int>(val));
    });

    QObject::connect(
        &inlet, &Control_T::executionValueChanged, sl, [=](const ossia::value& val) {
      if(!sl->moving)
        sl->setExecutionValue(ossia::convert<int>(val));
    });
    QObject::connect(
        &inlet, &Control_T::executionReset, sl,
        &score::QGraphicsIntSpinbox::resetExecution);

    return sl;
  }
};

struct ControllerLineEdit
{
  static Process::PortItemLayout layout() noexcept
  {
    return Process::DefaultControlLayouts::lineedit();
  }

  template <typename T>
  static auto make_widget(
      T& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    auto sl = new QLineEdit{parent};
    sl->setText(QString::fromStdString(ossia::convert<std::string>(inlet.value())));
    sl->setContentsMargins(0, 0, 0, 0);
    sl->setMaximumWidth(70);
    QObject::connect(sl, &QLineEdit::editingFinished, context, [sl, &inlet, &ctx]() {
      CommandDispatcher<>{ctx.commandStack}.submit<Scenario::SetControllerControlValue>(
          inlet, sl->text().toStdString(), ctx);
    });

    QObject::connect(&inlet, &T::valueChanged, sl, [sl](const ossia::value& val) {
      sl->setText(QString::fromStdString(ossia::convert<std::string>(val)));
    });

    return sl;
  }

  template <typename T, typename Control_T>
  static score::QGraphicsLineEdit* make_item(
      const T& slider, Control_T& inlet, const score::DocumentContext& ctx,
      QGraphicsItem* parent, QObject* context)
  {
    auto sl = new score::QGraphicsLineEdit{parent};
    WidgetFactory::initWidgetProperties(inlet, *sl);
    sl->setTextWidth(180.);
    sl->setDefaultTextColor(QColor{"#E0B01E"});
    sl->setCursor(Qt::IBeamCursor);

    sl->setPlainText(QString::fromStdString(ossia::convert<std::string>(inlet.value())));

    auto doc = sl->document();
    auto on_edit = [=, &inlet, &ctx] {
      auto cur_str = ossia::convert<std::string>(inlet.value());
      if(cur_str != doc->toPlainText().toStdString())
      {
        CommandDispatcher<>{ctx.commandStack}
            .submit<Scenario::SetControllerControlValue>(
                inlet, doc->toPlainText().toStdString(), ctx);
      }
    };

    if(!inlet.noValueChangeOnMove)
    {
      QObject::connect(doc, &QTextDocument::contentsChanged, context, on_edit);
    }
    else
    {
      QObject::connect(sl, &score::QGraphicsLineEdit::editingFinished, context, on_edit);
    }

    if(auto obj = dynamic_cast<score::ResizeableItem*>(context))
    {
      QObject::connect(
          sl, &score::QGraphicsLineEdit::sizeChanged, obj,
          &score::ResizeableItem::childrenSizeChanged);
    }

    QObject::connect(&inlet, &Control_T::valueChanged, sl, [=](const ossia::value& val) {
      auto str = QString::fromStdString(ossia::convert<std::string>(val));
      if(str != doc->toPlainText())
        doc->setPlainText(str);
    });

    return sl;
  }
};

template <typename Node, std::size_t N, typename Field>
  requires(avnd::controller_interaction_port<Field> && avnd::int_parameter<Field>)
struct CustomControlFactory<Node, avnd::field_reflection<N, Field>>
    : public Dataflow::WidgetInletFactory<
          CustomGenericControl<Process::IntSpinBox, Node, Field, avnd::field_index<N>>,
          ControllerIntSpinBox>
{
};

template <typename Node, std::size_t N, typename Field>
  requires(avnd::controller_interaction_port<Field> && avnd::string_parameter<Field>)
struct CustomControlFactory<Node, avnd::field_reflection<N, Field>>
    : public Dataflow::WidgetInletFactory<
          CustomGenericControl<Process::LineEdit, Node, Field, avnd::field_index<N>>,
          ControllerLineEdit>
{
};

template <typename N>
using reflect_controller_controls =
    typename avnd::controller_interaction_port_input_introspection<
        N>::field_reflections_type;

}
