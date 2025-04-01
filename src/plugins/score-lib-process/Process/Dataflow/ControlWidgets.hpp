#pragma once
#include <Process/Commands/SetControlValue.hpp>
#include <Process/Dataflow/ControlWidgetDomains.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/TimeSignature.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/Script/ScriptEditor.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/graphics/GraphicWidgets.hpp>
#include <score/graphics/GraphicsItem.hpp>
#include <score/graphics/RectItem.hpp>
#include <score/graphics/TextItem.hpp>
#include <score/tools/Unused.hpp>
#include <score/widgets/ComboBox.hpp>
#include <score/widgets/ControlWidgets.hpp>
#include <score/widgets/HelpInteraction.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/network/domain/domain_functions.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <ossia-qt/invoke.hpp>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QFileDialog>
#include <QGraphicsItem>
#include <QGraphicsSceneDragDropEvent>
#include <QLineEdit>
#include <QPalette>
#include <QTextDocument>

#include <score_lib_process_export.h>

#include <verdigris>
namespace Process
{

struct SCORE_LIB_PROCESS_EXPORT DefaultControlLayouts
{
  static Process::PortItemLayout knob() noexcept;
  static Process::PortItemLayout slider() noexcept;
  static Process::PortItemLayout combo() noexcept;
  static Process::PortItemLayout list() noexcept;
  static Process::PortItemLayout lineedit() noexcept;
  static Process::PortItemLayout spinbox() noexcept;
  static Process::PortItemLayout toggle() noexcept;
  static Process::PortItemLayout pad() noexcept;
  static Process::PortItemLayout bang() noexcept;
  static Process::PortItemLayout button() noexcept;
  static Process::PortItemLayout chooser_toggle() noexcept;
};
}

namespace WidgetFactory
{
static_assert(std::numeric_limits<float>::is_iec559, "IEEE 754 required");
template <typename T>
using SetControlValue = typename std::conditional_t<
    std::is_base_of<Process::ControlInlet, T>::value, Process::SetControlValue,
    Process::SetControlOutletValue>;

template <typename Normalizer, typename T>
using ConcreteNormalizer = std::conditional_t<
    std::is_base_of_v<Process::ControlInlet, T>
        || std::is_base_of_v<Process::ControlOutlet, T>,
    UpdatingNormalizer<Normalizer, T>, FixedNormalizer<Normalizer>>;
template <typename ControlUI, typename Normalizer, bool Control>
struct FloatControl
{
  static Process::PortItemLayout layout() noexcept
  {
    using namespace Process;
    if constexpr(
        std::is_same_v<score::QGraphicsKnob, ControlUI>
        || std::is_same_v<score::QGraphicsLogKnob, ControlUI>)
    {
      return DefaultControlLayouts::knob();
    }
    else
    {
      return DefaultControlLayouts::slider();
    }
  }

  template <typename T>
  static auto make_widget(
      T& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    ConcreteNormalizer<Normalizer, T> norm{inlet};

    auto sl = new score::ValueDoubleSlider{parent};
    sl->setOrientation(Qt::Horizontal);
    sl->setContentsMargins(0, 0, 0, 0);
    bindFloatDomain(inlet, inlet, *sl);
    sl->setValue(norm.to01(ossia::convert<double>(inlet.value())));

    if constexpr(Control)
    {
      QObject::connect(
          sl, &score::DoubleSlider::sliderMoved, context,
          [sl, norm, &inlet, &ctx](double v) {
        sl->moving = true;
        ctx.dispatcher.submit<SetControlValue<T>>(inlet, norm.from01(sl->value()));
          });
      QObject::connect(
          sl, &score::DoubleSlider::sliderReleased, context, [sl, norm, &inlet, &ctx]() {
        ctx.dispatcher.submit<SetControlValue<T>>(inlet, norm.from01(sl->value()));
        ctx.dispatcher.commit();
        sl->moving = false;
      });
    }

    QObject::connect(&inlet, &T::valueChanged, sl, [sl, norm](const ossia::value& val) {
      if constexpr(Control)
      {
        if(!sl->moving)
          sl->setValue(norm.to01(ossia::convert<double>(val)));
      }
      else
      {
        sl->setValue(norm.to01(ossia::convert<double>(val)));
      }
    });

    return sl;
  }

  template <typename T, typename Control_T>
  static auto make_item(
      const T& slider, Control_T& inlet, const score::DocumentContext& ctx,
      QGraphicsItem* parent, QObject* context)
  {
    ConcreteNormalizer<Normalizer, T> norm{slider};

    auto sl = new ControlUI{nullptr};
    initWidgetProperties(inlet, *sl);
    bindFloatDomain(slider, inlet, *sl);
    sl->setValue(norm.to01(ossia::convert<double>(inlet.value())));

    if constexpr(Control)
    {
      QObject::connect(sl, &ControlUI::sliderMoved, context, [sl, norm, &inlet, &ctx] {
        sl->moving = true;
        ctx.dispatcher.submit<SetControlValue<Control_T>>(
            inlet, norm.from01(sl->value()));
      });
      QObject::connect(
          sl, &ControlUI::sliderReleased, context, [sl, norm, &inlet, &ctx] {
            ctx.dispatcher.submit<SetControlValue<Control_T>>(
                inlet, norm.from01(sl->value()));
            ctx.dispatcher.commit();
            sl->moving = false;
          });
    }

    QObject::connect(
        &inlet, &Control_T::valueChanged, sl, [sl, norm](const ossia::value& val) {
          if constexpr(Control)
          {
            if(!sl->moving)
              sl->setValue(norm.to01(ossia::convert<double>(val)));
          }
          else
          {
            sl->setValue(norm.to01(ossia::convert<double>(val)));
          }
        });
    QObject::connect(
        &inlet, &Control_T::executionValueChanged, sl,
        [sl, norm](const ossia::value& val) {
      sl->setExecutionValue(norm.to01(ossia::convert<double>(val)));
        });
    QObject::connect(&inlet, &Control_T::executionReset, sl, &ControlUI::resetExecution);

    return sl;
  }
};

using FloatSlider = FloatControl<score::QGraphicsSlider, LinearNormalizer, true>;
using LogFloatSlider = FloatControl<score::QGraphicsLogSlider, LogNormalizer, true>;
using FloatKnob = FloatControl<score::QGraphicsKnob, LinearNormalizer, true>;
using LogFloatKnob = FloatControl<score::QGraphicsLogKnob, LogNormalizer, true>;
using FloatDisplay = FloatControl<score::QGraphicsSlider, LinearNormalizer, false>;
using LogFloatDisplay = FloatControl<score::QGraphicsLogSlider, LogNormalizer, false>;

struct IntSlider
{
  static Process::PortItemLayout layout() noexcept
  {
    return Process::DefaultControlLayouts::slider();
  }

  template <typename T>
  static auto make_widget(
      T& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    auto sl = new score::ValueSlider{parent};
    sl->setOrientation(Qt::Horizontal);
    bindIntDomain(inlet, inlet, *sl);
    sl->setValue(ossia::convert<int>(inlet.value()));
    sl->setContentsMargins(0, 0, 0, 0);

    QObject::connect(
        sl, &score::IntSlider::sliderMoved, context, [sl, &inlet, &ctx](int p) {
          sl->moving = true;
          ctx.dispatcher.submit<SetControlValue<T>>(inlet, p);
        });
    QObject::connect(sl, &score::IntSlider::sliderReleased, context, [sl, &inlet, &ctx] {
      ctx.dispatcher.submit<SetControlValue<T>>(inlet, sl->value());
      ctx.dispatcher.commit();
      sl->moving = false;
    });

    QObject::connect(&inlet, &T::valueChanged, sl, [sl](const ossia::value& val) {
      if(!sl->moving)
        sl->setValue(ossia::convert<int>(val));
    });

    return sl;
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& slider, Control_T& inlet, const score::DocumentContext& ctx,
      QGraphicsItem* parent, QObject* context)
  {
    auto sl = new score::QGraphicsIntSlider{nullptr};
    initWidgetProperties(inlet, *sl);
    bindIntDomain(slider, inlet, *sl);
    sl->setValue(ossia::convert<int>(inlet.value()));

    QObject::connect(
        sl, &score::QGraphicsIntSlider::sliderMoved, context, [=, &inlet, &ctx] {
          sl->moving = true;
          ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, sl->value());
        });
    QObject::connect(
        sl, &score::QGraphicsIntSlider::sliderReleased, context, [&ctx, sl]() {
          ctx.dispatcher.commit();
          sl->moving = false;
        });

    QObject::connect(&inlet, &Control_T::valueChanged, sl, [=](const ossia::value& val) {
      if(!sl->moving)
        sl->setValue(ossia::convert<int>(val));
    });
    QObject::connect(
        &inlet, &Control_T::executionValueChanged, sl, [=](const ossia::value& val) {
          sl->setExecutionValue(ossia::convert<int>(val));
        });
    QObject::connect(
        &inlet, &Control_T::executionReset, sl,
        &score::QGraphicsIntSlider::resetExecution);

    return sl;
  }
};

struct IntRangeSlider
{
  static Process::PortItemLayout layout() noexcept
  {
    return Process::DefaultControlLayouts::slider();
  }

  template <typename T>
  static auto make_widget(
      T& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    // TODO
    return nullptr;
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& slider, Control_T& inlet, const score::DocumentContext& ctx,
      QGraphicsItem* parent, QObject* context)
  {
    auto sl = new score::QGraphicsRangeSlider{parent};
    initWidgetProperties(inlet, *sl);
    bindIntDomain(slider, inlet, *sl);
    sl->setValue(ossia::convert<ossia::vec2f>(inlet.value()));

    QObject::connect(
        sl, &score::QGraphicsRangeSlider::sliderMoved, context, [=, &inlet, &ctx] {
          sl->moving = true;
          ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, sl->value());
        });
    QObject::connect(
        sl, &score::QGraphicsRangeSlider::sliderReleased, context, [&ctx, sl]() {
          ctx.dispatcher.commit();
          sl->moving = false;
        });

    QObject::connect(&inlet, &Control_T::valueChanged, sl, [=](const ossia::value& val) {
      if(!sl->moving)
        sl->setValue(ossia::convert<ossia::vec2f>(val));
    });
    QObject::connect(
        &inlet, &Control_T::executionValueChanged, sl, [=](const ossia::value& val) {
          // TODO
          // sl->setExecutionValue(ossia::convert<ossia::vec2f>(val));
        });
    QObject::connect(
        &inlet, &Control_T::executionReset, sl,
        &score::QGraphicsRangeSlider::resetExecution);

    return sl;
  }
};
struct FloatRangeSlider
{
  static Process::PortItemLayout layout() noexcept
  {
    return Process::DefaultControlLayouts::slider();
  }

  template <typename T>
  static auto make_widget(
      T& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    // TODO
    return nullptr;
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& slider, Control_T& inlet, const score::DocumentContext& ctx,
      QGraphicsItem* parent, QObject* context)
  {
    auto sl = new score::QGraphicsRangeSlider{parent};
    initWidgetProperties(inlet, *sl);
    bindFloatDomain(slider, inlet, *sl);
    sl->setValue(ossia::convert<ossia::vec2f>(inlet.value()));

    QObject::connect(
        sl, &score::QGraphicsRangeSlider::sliderMoved, context, [=, &inlet, &ctx] {
          sl->moving = true;
          ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, sl->value());
        });
    QObject::connect(
        sl, &score::QGraphicsRangeSlider::sliderReleased, context, [&ctx, sl]() {
          ctx.dispatcher.commit();
          sl->moving = false;
        });

    QObject::connect(&inlet, &Control_T::valueChanged, sl, [=](const ossia::value& val) {
      if(!sl->moving)
        sl->setValue(ossia::convert<ossia::vec2f>(val));
    });
    QObject::connect(
        &inlet, &Control_T::executionValueChanged, sl, [=](const ossia::value& val) {
          // TODO
          // sl->setExecutionValue(ossia::convert<ossia::vec2f>(val));
        });
    QObject::connect(
        &inlet, &Control_T::executionReset, sl,
        &score::QGraphicsRangeSlider::resetExecution);

    return sl;
  }
};
struct FloatRangeSpinBox
{
  static Process::PortItemLayout layout() noexcept
  {
    return Process::DefaultControlLayouts::pad();
  }

  template <typename T>
  static auto make_widget(
      T& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    SCORE_TODO;
    return nullptr; // TODO
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& slider, Control_T& inlet, const score::DocumentContext& ctx,
      QGraphicsItem* parent, QObject* context)
  {
    auto sl = new score::QGraphicsXYSpinboxChooser{true, nullptr};
    initWidgetProperties(inlet, *sl);
    bindVec2Domain(slider, inlet, *sl);
    sl->setValue(
        LinearNormalizer::to01(*sl, ossia::convert<ossia::vec2f>(inlet.value())));

    QObject::connect(
        sl, &score::QGraphicsXYSpinboxChooser::sliderMoved, context, [=, &inlet, &ctx] {
          sl->moving = true;
          ctx.dispatcher.submit<SetControlValue<Control_T>>(
              inlet, LinearNormalizer::from01(*sl, sl->value()));
        });
    QObject::connect(
        sl, &score::QGraphicsXYSpinboxChooser::sliderReleased, context, [&ctx, sl]() {
          ctx.dispatcher.commit();
          sl->moving = false;
        });

    QObject::connect(&inlet, &Control_T::valueChanged, sl, [=](const ossia::value& val) {
      if(!sl->moving)
        sl->setValue(LinearNormalizer::to01(*sl, ossia::convert<ossia::vec2f>(val)));
    });

    return sl;
  }
};

struct IntSpinBox
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
    bindIntDomain(inlet, inlet, *sl);
    sl->setValue(ossia::convert<int>(inlet.value()));
    sl->setContentsMargins(0, 0, 0, 0);

    QObject::connect(
        sl, SignalUtils::QSpinBox_valueChanged_int(), context, [&inlet, &ctx](int val) {
      CommandDispatcher<>{ctx.commandStack}.submit<SetControlValue<T>>(inlet, val);
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
    initWidgetProperties(inlet, *sl);
    bindIntDomain(slider, inlet, *sl);
    sl->setValue(ossia::convert<int>(inlet.value()));

    QObject::connect(
        sl, &score::QGraphicsIntSpinbox::sliderMoved, context, [=, &inlet, &ctx] {
          sl->moving = true;
          ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, sl->value());
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

struct FloatSpinBox
{
  static Process::PortItemLayout layout() noexcept
  {
    return Process::DefaultControlLayouts::spinbox();
  }

  template <typename T>
  static auto make_widget(
      T& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    auto sl = new QDoubleSpinBox{parent};
    bindFloatDomain(inlet, inlet, *sl);
    sl->setValue(ossia::convert<float>(inlet.value()));
    sl->setContentsMargins(0, 0, 0, 0);

    QObject::connect(
        sl, SignalUtils::QDoubleSpinBox_valueChanged_double(), context,
        [&inlet, &ctx](double val) {
      CommandDispatcher<>{ctx.commandStack}.submit<SetControlValue<T>>(inlet, val);
    });

    QObject::connect(&inlet, &T::valueChanged, sl, [sl](const ossia::value& val) {
      sl->setValue(ossia::convert<float>(val));
    });

    return sl;
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& slider, Control_T& inlet, const score::DocumentContext& ctx,
      QGraphicsItem* parent, QObject* context)
  {
    ConcreteNormalizer<LinearNormalizer, T> norm{slider};

    auto sl = new score::QGraphicsSpinbox{nullptr};
    initWidgetProperties(inlet, *sl);
    bindFloatDomain(slider, inlet, *sl);
    sl->setValue(norm.to01(ossia::convert<float>(inlet.value())));

    QObject::connect(
        sl, &score::QGraphicsSpinbox::sliderMoved, context,
        [=, &inlet, &ctx] {
      sl->moving = true;
      ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, norm.from01(sl->value()));
        });
    QObject::connect(
        sl, &score::QGraphicsSpinbox::sliderReleased, context, [&ctx, sl]() {
          ctx.dispatcher.commit();
          sl->moving = false;
        });

    QObject::connect(&inlet, &Control_T::valueChanged, sl, [=](const ossia::value& val) {
      if(!sl->moving)
        sl->setValue(norm.to01(ossia::convert<float>(val)));
    });

    QObject::connect(
        &inlet, &Control_T::executionValueChanged, sl, [=](const ossia::value& val) {
          if(!sl->moving)
            sl->setExecutionValue(norm.to01(ossia::convert<float>(val)));
        });
    QObject::connect(
        &inlet, &Control_T::executionReset, sl,
        &score::QGraphicsSpinbox::resetExecution);

    return sl;
  }
};

struct TimeChooser
{
  static Process::PortItemLayout layout() noexcept
  {
    using namespace Process;
    return DefaultControlLayouts::knob();
  }

  template <typename T>
  static auto make_widget(
      T& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    // FIXME
    return nullptr;
  }

  template <typename T, typename Control_T>
  static auto make_item(
      const T& slider, Control_T& inlet, const score::DocumentContext& ctx,
      QGraphicsItem* parent, QObject* context)
  {
    auto sl = new score::QGraphicsTimeChooser{nullptr};
    initWidgetProperties(inlet, *sl);
    // bindFloatDomain(slider, inlet, *sl);
    sl->setValue(ossia::convert<ossia::vec2f>(inlet.value()));

    QObject::connect(
        sl, &score::QGraphicsTimeChooser::sliderMoved, context, [sl, &inlet, &ctx] {
          sl->knob.moving = true;
          sl->combo.moving = true;
          ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, sl->value());
        });
    QObject::connect(
        sl, &score::QGraphicsTimeChooser::sliderReleased, context, [sl, &inlet, &ctx] {
          ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, sl->value());
          ctx.dispatcher.commit();
          sl->knob.moving = false;
          sl->combo.moving = false;
        });

    QObject::connect(
        &inlet, &Control_T::valueChanged, sl, [sl](const ossia::value& val) {
          if(!sl->knob.moving && !sl->combo.moving)
          {
            sl->setValue(ossia::convert<ossia::vec2f>(val));
          }
        });
    QObject::connect(
        &inlet, &Control_T::executionValueChanged, sl, [sl](const ossia::value& val) {
          sl->setExecutionValue(ossia::convert<ossia::vec2f>(val));
        });
    QObject::connect(
        &inlet, &Control_T::executionReset, sl,
        &score::QGraphicsTimeChooser::resetExecution);

    return sl;
  }
};

struct Toggle
{
  static Process::PortItemLayout layout() noexcept
  {
    return Process::DefaultControlLayouts::toggle();
  }

  template <typename T>
  static auto make_widget(
      T& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    auto sl = new QCheckBox{parent};
    sl->setChecked(ossia::convert<bool>(inlet.value()));
    QObject::connect(sl, &QCheckBox::toggled, context, [&inlet, &ctx](bool val) {
      CommandDispatcher<>{ctx.commandStack}.submit<SetControlValue<T>>(inlet, val);
    });

    QObject::connect(&inlet, &T::valueChanged, sl, [sl](const ossia::value& val) {
      sl->setChecked(ossia::convert<bool>(val));
    });

    return sl;
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& toggle, Control_T& inlet, const score::DocumentContext& ctx,
      QGraphicsItem* parent, QObject* context)
  {
    auto cb = new score::QGraphicsCheckBox{nullptr};
    initWidgetProperties(inlet, *cb);
    cb->setState(ossia::convert<bool>(inlet.value()));

    QObject::connect(
        cb, &score::QGraphicsCheckBox::toggled, context,
        [=, &inlet, &ctx](bool toggled) {
      ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, toggled);
      ctx.dispatcher.commit();
        });

    QObject::connect(
        &inlet, &Control_T::valueChanged, cb,
        [cb](const ossia::value& val) { cb->setState(ossia::convert<bool>(val)); });

    return cb;
  }
};

struct ImpulseButton
{
  static Process::PortItemLayout layout() noexcept
  {
    return Process::DefaultControlLayouts::bang();
  }

  template <typename T>
  static auto make_widget(
      T& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    auto sl = new QPushButton{parent};
    const auto& name = inlet.visualName();
    sl->setText(name.isEmpty() ? QObject::tr("Bang") : name);
    sl->setContentsMargins(0, 0, 0, 0);

    auto& cinlet = const_cast<T&>(inlet);
    QObject::connect(sl, &QPushButton::pressed, context, [&cinlet] {
      cinlet.valueChanged(ossia::impulse{});
    });

    return sl;
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& slider, Control_T& inlet, const score::DocumentContext& ctx,
      QGraphicsItem* parent, QObject* context)
  {
    auto toggle = new score::QGraphicsButton{nullptr};
    initWidgetProperties(inlet, *toggle);

    QObject::connect(
        toggle, &score::QGraphicsButton::pressed, context, [=, &inlet](bool pressed) {
          if(pressed)
          {
            inlet.valueChanged(ossia::impulse{});
          }
        });

    return toggle;
  }
};

struct Button
{
  static Process::PortItemLayout layout() noexcept
  {
    return Process::DefaultControlLayouts::bang();
  }

  template <typename T>
  static auto make_widget(
      T& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    auto sl = new QPushButton{parent};
    const auto& name = inlet.visualName();
    sl->setText(name.isEmpty() ? QObject::tr("Bang") : name);
    sl->setContentsMargins(0, 0, 0, 0);

    // TODO should we not make a command here
    auto& cinlet = const_cast<T&>(inlet);
    QObject::connect(
        sl, &QPushButton::pressed, context, [&cinlet] { cinlet.setValue(true); });
    QObject::connect(
        sl, &QPushButton::released, context, [&cinlet] { cinlet.setValue(false); });

    return sl;
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& slider, Control_T& inlet, const score::DocumentContext& ctx,
      QGraphicsItem* parent, QObject* context)
  {
    auto toggle = new score::QGraphicsButton{nullptr};
    initWidgetProperties(inlet, *toggle);

    QObject::connect(
        toggle, &score::QGraphicsButton::pressed, context,
        [=, &inlet, &ctx](bool pressed) {
      ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, pressed);
      ctx.dispatcher.commit();
        });

    return toggle;
  }
};

struct ChooserToggle
{
  static Process::PortItemLayout layout() noexcept
  {
    return Process::DefaultControlLayouts::chooser_toggle();
  }

  template <typename T>
  static constexpr auto getAlternatives(const T& t) -> decltype(auto)
  {
    if constexpr(std::is_member_function_pointer_v<decltype(&T::alternatives)>)
    {
      return t.alternatives();
    }
    else
    {
      return t.alternatives;
    }
  }

  template <typename T>
  static auto make_widget(
      T& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {

    const auto& alts = getAlternatives(inlet);
    SCORE_ASSERT(alts.size() == 2);
    auto toggleBtn = new score::ToggleButton{alts, parent};
    toggleBtn->setCheckable(true);
    bool b = ossia::convert<bool>(inlet.value());
    if(b && !toggleBtn->isChecked())
      toggleBtn->toggle();
    else if(!b && toggleBtn->isChecked())
      toggleBtn->toggle();

    QObject::connect(
        toggleBtn, &score::ToggleButton::toggled, context, [&inlet, &ctx](bool val) {
      CommandDispatcher<>{ctx.commandStack}.submit<SetControlValue<T>>(inlet, val);
    });

    QObject::connect(
        &inlet, &T::valueChanged, toggleBtn, [toggleBtn](const ossia::value& val) {
      bool b = ossia::convert<bool>(val);
      if(b && !toggleBtn->isChecked())
        toggleBtn->toggle();
      else if(!b && toggleBtn->isChecked())
        toggleBtn->toggle();
    });

    return toggleBtn;
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& control, Control_T& inlet, const score::DocumentContext& ctx,
      QGraphicsItem* parent, QObject* context)
  {
    const auto& alts = getAlternatives(control);
    SCORE_ASSERT(alts.size() == 2);
    auto toggle = new score::QGraphicsToggle{alts[0], alts[1], nullptr};
    initWidgetProperties(inlet, *toggle);
    toggle->setState(ossia::convert<bool>(inlet.value()));

    QObject::connect(
        toggle, &score::QGraphicsToggle::toggled, context,
        [=, &inlet, &ctx](bool toggled) {
      ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, toggled);
      ctx.dispatcher.commit();
        });

    QObject::connect(
        &inlet, &Control_T::valueChanged, toggle, [toggle](const ossia::value& val) {
          toggle->setState(ossia::convert<bool>(val));
        });

    return toggle;
  }
};

struct LineEdit
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
      CommandDispatcher<>{ctx.commandStack}.submit<SetControlValue<T>>(
          inlet, sl->text().toStdString());
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
    initWidgetProperties(inlet, *sl);
    sl->setTextWidth(180.);
    sl->setDefaultTextColor(QColor{"#E0B01E"});
    sl->setCursor(Qt::IBeamCursor);

    sl->setPlainText(QString::fromStdString(ossia::convert<std::string>(inlet.value())));

    auto doc = sl->document();
    auto on_edit = [=, &inlet, &ctx] {
      auto cur_str = ossia::convert<std::string>(inlet.value());
      if(cur_str != doc->toPlainText().toStdString())
      {
        CommandDispatcher<>{ctx.commandStack}.submit<SetControlValue<Control_T>>(
            inlet, doc->toPlainText().toStdString());
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

struct ProgramPortScriptDialog : Process::ScriptDialog
{
  ProgramPortScriptDialog(
      std::string_view language, const Process::ControlInlet& port,
      const score::DocumentContext& ctx, QWidget* parent)
      : ScriptDialog{language, ctx, parent}
      , m_port{port}
  {
    this->setText(QString::fromStdString(ossia::convert<std::string>(port.value())));
    con(m_port, &IdentifiedObjectAbstract::identified_object_destroying, this,
        &QWidget::deleteLater);
  }

  void on_accepted() override
  {
    this->setError(0, QString{});
    auto cur = ossia::convert<std::string>(m_port.value());
    auto next = this->text().toStdString();
    if(next != cur)
    {
      ossia::qt::run_async(
          qApp, [&ctx = m_context.commandStack, port = QPointer{&m_port}, next] {
        if(port)
        {
          CommandDispatcher<>{ctx}.submit(new Process::SetControlValue{*port, next});
        }
      });
    }
  }

  const Process::ControlInlet& m_port;
};

struct ProgramEdit
{
  static Process::PortItemLayout layout() noexcept
  {
    return Process::DefaultControlLayouts::lineedit();
  }

  template <typename T>
  static auto make_widget(
      T& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    auto toggle = new QPushButton{QObject::tr("Edit..."), parent};
    QObject::connect(toggle, &QPushButton::clicked, context, [=, &inlet, &ctx] {
      createDialog(inlet, ctx);
    });
    return toggle;
  }

  template <typename T, typename Control_T>
  static auto make_item(
      const T& slider, Control_T& inlet, const score::DocumentContext& ctx,
      QGraphicsItem* parent, QObject* context)
  {
    auto toggle = new score::QGraphicsTextButton{QObject::tr("Edit..."), nullptr};
    initWidgetProperties(inlet, *toggle);

    QObject::connect(
        toggle, &score::QGraphicsTextButton::pressed, context,
        [=, &inlet, &ctx] { createDialog(inlet, ctx); }, Qt::QueuedConnection);

    return toggle;
  }

  static void
  createDialog(const Process::ProgramEdit& inlet, const score::DocumentContext& ctx)
  {
    auto dial = new ProgramPortScriptDialog{inlet.language, inlet, ctx, nullptr};
    dial->exec();
  }
};

struct FileChooser
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
    auto act = new QAction{sl};

    score::setHelp(act, QObject::tr("Opening a File"));
    act->setIcon(QIcon(":/icons/search.png"));
    sl->setPlaceholderText(QObject::tr("Open File"));
    auto on_open = [=, &inlet] {
      auto filename
          = QFileDialog::getOpenFileName(nullptr, "Open File", {}, inlet.filters());
      if(filename.isEmpty())
        return;
      sl->setText(filename);
    };

    QObject::connect(sl, &QLineEdit::returnPressed, on_open);
    QObject::connect(act, &QAction::triggered, on_open);
    sl->addAction(act, QLineEdit::TrailingPosition);

    sl->setText(QString::fromStdString(ossia::convert<std::string>(inlet.value())));
    sl->setContentsMargins(0, 0, 0, 0);
    //sl->setMaximumWidth(70);

    QObject::connect(sl, &QLineEdit::editingFinished, context, [sl, &inlet, &ctx]() {
      CommandDispatcher<>{ctx.commandStack}.submit<SetControlValue<T>>(
          inlet, sl->text().toStdString());
    });
    QObject::connect(&inlet, &T::valueChanged, sl, [sl](const ossia::value& val) {
      sl->setText(QString::fromStdString(ossia::convert<std::string>(val)));
    });
    return sl;
  }

  template <typename T, typename Control_T>
  static score::QGraphicsTextButton* make_item(
      const T& slider, Control_T& inlet, const score::DocumentContext& ctx,
      QGraphicsItem* parent, QObject* context)
  {
    auto bt = new score::QGraphicsTextButton{"Choose a file...", parent};
    initWidgetProperties(inlet, *bt);
    auto on_open = [&inlet, &ctx] {
      auto filename
          = QFileDialog::getOpenFileName(nullptr, "Open File", {}, inlet.filters());
      if(filename.isEmpty())
        return;

      CommandDispatcher<>{ctx.commandStack}.submit<SetControlValue<Control_T>>(
          inlet, filename.toStdString());
    };
    auto on_set = [&inlet, &ctx](const QString& filename) {
      if(filename.isEmpty())
        return;

      CommandDispatcher<>{ctx.commandStack}.submit<SetControlValue<Control_T>>(
          inlet, filename.toStdString());
    };
    QObject::connect(
        bt, &score::QGraphicsTextButton::pressed, &inlet, on_open, Qt::QueuedConnection);
    QObject::connect(
        bt, &score::QGraphicsTextButton::dropped, &inlet, on_set, Qt::QueuedConnection);
    auto set = [=](const ossia::value& val) {
      auto str = QString::fromStdString(ossia::convert<std::string>(val));
      if(str != bt->text())
      {
        if(!str.isEmpty())
          bt->setText(str.split("/").back());
        else
          bt->setText("Choose a file...");
      }
    };

    set(slider.value());
    QObject::connect(&inlet, &Control_T::valueChanged, bt, set);

    return bt;
  }
};

struct Enum
{
  static Process::PortItemLayout layout() noexcept
  {
    return Process::DefaultControlLayouts::list();
  }

  static const auto& toStd(const char* const& s) { return s; }
  static const auto& toStd(const std::string& s) { return s; }
  static auto toStd(const QString& s) { return s.toStdString(); }

  static const auto& convert(const std::string& str, const char*) { return str; }
  static auto convert(const std::string& str, const QString&)
  {
    return QString::fromStdString(str);
  }
  template <typename T>
  static auto make_widget(
      T& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    const auto& values = inlet.getValues();
    using val_t = std::remove_reference_t<decltype(values[0])>;
    auto sl = new QComboBox{parent};
    for(const auto& e : values)
    {
      sl->addItem(e);
    }

    auto set_index = [values, sl](const ossia::value& val) {
      auto v = ossia::convert<std::string>(val);
      auto it = ossia::find(values, convert(v, val_t{}));
      if(it != values.end())
      {
        sl->setCurrentIndex(std::distance(values.begin(), it));
      }
    };
    set_index(inlet.value());

    QObject::connect(
        sl, SignalUtils::QComboBox_currentIndexChanged_int(), context,
        [values, &inlet, &ctx](int idx) {
      CommandDispatcher<>{ctx.commandStack}.submit<SetControlValue<T>>(
          inlet, toStd(values[idx]));
    });

    QObject::connect(
        &inlet, &T::valueChanged, sl, [=](const ossia::value& val) { set_index(val); });

    return sl;
  }

  template <typename T, typename Control_T>
  static auto make_item(
      const T& slider, Control_T& inlet, const score::DocumentContext& ctx,
      QGraphicsItem* parent, QObject* context)
  {
    const auto& values = slider.getValues();
    using val_t = std::remove_reference_t<decltype(values[0])>;

    auto sl = slider.pixmaps.empty() || slider.pixmaps[0] == nullptr
                  ? new score::QGraphicsEnum{values, nullptr}
                  : (score::QGraphicsEnum*)new score::QGraphicsPixmapEnum{
                      values, slider.pixmaps, nullptr};

    initWidgetProperties(inlet, *sl);

    auto set_index = [values, sl](const ossia::value& val) {
      auto v = ossia::convert<std::string>(val);
      auto it = ossia::find(values, convert(v, val_t{}));
      if(it != values.end())
      {
        sl->setValue(std::distance(values.begin(), it));
      }
    };

    set_index(inlet.value());

    QObject::connect(
        sl, &score::QGraphicsEnum::currentIndexChanged, context,
        [sl, &inlet, &ctx](int idx) {
      ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, toStd(sl->array[idx]));
      ctx.dispatcher.commit();
        });

    QObject::connect(&inlet, &Control_T::valueChanged, sl, [=](const ossia::value& val) {
      set_index(val);
    });

    return sl;
  }
};

struct ComboBox
{
  static Process::PortItemLayout layout() noexcept
  {
    return Process::DefaultControlLayouts::combo();
  }

  template <typename T>
  static auto make_widget(
      T& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    const auto& values = inlet.getValues();
    auto sl = new score::ComboBox{parent};
    for(auto& e : values)
    {
      sl->addItem(e.first);
    }
    sl->setContentsMargins(0, 0, 0, 0);

    auto set_index = [values, sl](const ossia::value& val) {
      auto it
          = ossia::find_if(values, [&](const auto& pair) { return pair.second == val; });
      if(it != values.end())
      {
        sl->setCurrentIndex(std::distance(values.begin(), it));
      }
    };
    set_index(inlet.value());

    QObject::connect(
        sl, SignalUtils::QComboBox_currentIndexChanged_int(), context,
        [values, &inlet, &ctx](int idx) {
      CommandDispatcher<>{ctx.commandStack}.submit<SetControlValue<T>>(
          inlet, values[idx].second);
    });

    QObject::connect(
        &inlet, &T::valueChanged, sl, [=](const ossia::value& val) { set_index(val); });

    return sl;
  }

  template <typename U, typename Control_T>
  static QGraphicsItem* make_item(
      const U& slider, Control_T& inlet, const score::DocumentContext& ctx,
      QGraphicsItem* parent, QObject* context)
  {
    const auto N = slider.count();

    const auto& values = slider.getValues();
    QStringList arr;
    arr.reserve(N);
    for(std::size_t i = 0; i < N; i++)
      arr.push_back(values[i].first);

    auto sl = new score::QGraphicsCombo{arr, nullptr};
    initWidgetProperties(inlet, *sl);

    auto set_index = [values, sl](const ossia::value& val) {
      auto it
          = ossia::find_if(values, [&](const auto& pair) { return pair.second == val; });
      if(it != values.end())
      {
        sl->setValue(std::distance(values.begin(), it));
      }
    };
    set_index(inlet.value());

    QObject::connect(
        sl, &score::QGraphicsCombo::sliderMoved, context, [values, sl, &inlet, &ctx] {
          sl->moving = true;
          ctx.dispatcher.submit<SetControlValue<Control_T>>(
              inlet, values[sl->value()].second);
        });
    QObject::connect(sl, &score::QGraphicsCombo::sliderReleased, context, [sl, &ctx] {
      ctx.dispatcher.commit();
      sl->moving = false;
    });

    QObject::connect(&inlet, &Control_T::valueChanged, sl, [=](const ossia::value& val) {
      if(sl->moving)
        return;

      set_index(val);
    });

    return sl;
  }
};

struct TimeSignatureValidator final : public QValidator
{
  static constexpr Process::PortItemLayout layout() noexcept
  {
    using namespace Process;
    return PortItemLayout{};
  }

  using QValidator::QValidator;
  State validate(QString& str, int&) const override
  {
    auto p = ossia::get_time_signature(str.toStdString());
    if(!p)
      return State::Invalid;

    return State::Acceptable;
  }
};

struct HSVSlider
{
  static Process::PortItemLayout layout() noexcept
  {
    return Process::DefaultControlLayouts::pad();
  }

  template <typename T>
  static auto make_widget(
      T& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    SCORE_TODO;
    return nullptr; // TODO
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& slider, Control_T& inlet, const score::DocumentContext& ctx,
      QGraphicsItem* parent, QObject* context)
  {
    auto sl = new score::QGraphicsHSVChooser{nullptr};
    initWidgetProperties(inlet, *sl);
    sl->setRgbaValue(ossia::convert<ossia::vec4f>(inlet.value()));

    QObject::connect(
        sl, &score::QGraphicsHSVChooser::sliderMoved, context, [=, &inlet, &ctx] {
          sl->moving = true;
          ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, sl->rgbaValue());
        });
    QObject::connect(
        sl, &score::QGraphicsHSVChooser::sliderReleased, context, [&ctx, sl]() {
          ctx.dispatcher.commit();
          sl->moving = false;
        });

    QObject::connect(&inlet, &Control_T::valueChanged, sl, [=](const ossia::value& val) {
      if(!sl->moving)
        sl->setRgbaValue(ossia::convert<ossia::vec4f>(val));
    });

    return sl;
  }
};

struct XYSlider
{
  static constexpr Process::PortItemLayout layout() noexcept
  {
    using namespace Process;
    return PortItemLayout{};
  }

  template <typename T>
  static auto make_widget(
      T& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    SCORE_TODO;
    return nullptr; // TODO
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& slider, Control_T& inlet, const score::DocumentContext& ctx,
      QGraphicsItem* parent, QObject* context)
  {
    auto sl = new score::QGraphicsXYChooser{nullptr};
    initWidgetProperties(inlet, *sl);
    sl->setValue(ossia::convert<ossia::vec2f>(inlet.value()));
    bindVec2Domain(slider, inlet, *sl);

    QObject::connect(
        sl, &score::QGraphicsXYChooser::sliderMoved, context, [=, &inlet, &ctx] {
          sl->moving = true;
          ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, sl->value());
        });
    QObject::connect(
        sl, &score::QGraphicsXYChooser::sliderReleased, context, [&ctx, sl]() {
          ctx.dispatcher.commit();
          sl->moving = false;
        });

    QObject::connect(&inlet, &Control_T::valueChanged, sl, [=](const ossia::value& val) {
      if(!sl->moving)
        sl->setValue(ossia::convert<ossia::vec2f>(val));
    });

    return sl;
  }
};

struct XYZSlider
{
  static Process::PortItemLayout layout() noexcept
  {
    return Process::DefaultControlLayouts::pad();
  }

  template <typename T>
  static auto make_widget(
      T& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    SCORE_TODO;
    return nullptr; // TODO
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& slider, Control_T& inlet, const score::DocumentContext& ctx,
      QGraphicsItem* parent, QObject* context)
  {
    auto sl = new score::QGraphicsXYZChooser{nullptr};
    initWidgetProperties(inlet, *sl);
    sl->setValue(ossia::convert<ossia::vec3f>(inlet.value()));
    bindVec3Domain(slider, inlet, *sl);

    QObject::connect(
        sl, &score::QGraphicsXYZChooser::sliderMoved, context, [=, &inlet, &ctx] {
          sl->moving = true;
          ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, sl->value());
        });
    QObject::connect(
        sl, &score::QGraphicsXYZChooser::sliderReleased, context, [&ctx, sl]() {
          ctx.dispatcher.commit();
          sl->moving = false;
        });

    QObject::connect(&inlet, &Control_T::valueChanged, sl, [=](const ossia::value& val) {
      if(!sl->moving)
        sl->setValue(ossia::convert<ossia::vec3f>(val));
    });

    return sl;
  }
};

struct XYSpinboxes
{
  static Process::PortItemLayout layout() noexcept
  {
    return Process::DefaultControlLayouts::pad();
  }

  template <typename T>
  static auto make_widget(
      T& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    SCORE_TODO;
    return nullptr; // TODO
  }

  static QGraphicsItem* make_item(
      const Process::XYSpinboxes& slider, Process::XYSpinboxes& inlet,
      const score::DocumentContext& ctx, QGraphicsItem* parent, QObject* context)
  {
    using Control_T = Process::XYSpinboxes;
    if(slider.integral)
    {
      auto sl = new score::QGraphicsIntXYSpinboxChooser{false, nullptr};
      initWidgetProperties(inlet, *sl);
      bindVec2Domain(slider, inlet, *sl);
      sl->setValue(ossia::convert<ossia::vec2f>(inlet.value()));

      QObject::connect(
          sl, &score::QGraphicsIntXYSpinboxChooser::sliderMoved, context,
          [=, &inlet, &ctx] {
        sl->moving = true;
        auto [x, y] = sl->value();
        ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, ossia::make_vec(x, y));
      });
      QObject::connect(
          sl, &score::QGraphicsIntXYSpinboxChooser::sliderReleased, context,
          [&ctx, sl]() {
        ctx.dispatcher.commit();
        sl->moving = false;
      });

      QObject::connect(
          &inlet, &Control_T::valueChanged, sl, [=](const ossia::value& val) {
        if(!sl->moving)
          sl->setValue(ossia::convert<ossia::vec2f>(val));
      });

      return sl;
    }
    else
    {
      auto sl = new score::QGraphicsXYSpinboxChooser{false, nullptr};
      initWidgetProperties(inlet, *sl);
      bindVec2Domain(slider, inlet, *sl);
      sl->setValue(
          LinearNormalizer::to01(*sl, ossia::convert<ossia::vec2f>(inlet.value())));

      QObject::connect(
          sl, &score::QGraphicsXYSpinboxChooser::sliderMoved, context,
          [=, &inlet, &ctx] {
        sl->moving = true;
        ctx.dispatcher.submit<SetControlValue<Control_T>>(
            inlet, LinearNormalizer::from01(*sl, sl->value()));
      });
      QObject::connect(
          sl, &score::QGraphicsXYSpinboxChooser::sliderReleased, context, [&ctx, sl]() {
        ctx.dispatcher.commit();
        sl->moving = false;
      });

      QObject::connect(
          &inlet, &Control_T::valueChanged, sl, [=](const ossia::value& val) {
        if(!sl->moving)
          sl->setValue(LinearNormalizer::to01(*sl, ossia::convert<ossia::vec2f>(val)));
      });

      return sl;
    }
  }
};

struct XYZSpinboxes
{
  static Process::PortItemLayout layout() noexcept
  {
    return Process::DefaultControlLayouts::pad();
  }

  template <typename T>
  static auto make_widget(
      T& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    SCORE_TODO;
    return nullptr; // TODO
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& slider, Control_T& inlet, const score::DocumentContext& ctx,
      QGraphicsItem* parent, QObject* context)
  {
    auto sl = new score::QGraphicsXYZSpinboxChooser{nullptr};
    initWidgetProperties(inlet, *sl);
    bindVec3Domain(slider, inlet, *sl);
    sl->setValue(
        LinearNormalizer::to01(*sl, ossia::convert<ossia::vec3f>(inlet.value())));

    QObject::connect(
        sl, &score::QGraphicsXYZSpinboxChooser::sliderMoved, context, [=, &inlet, &ctx] {
          sl->moving = true;
          ctx.dispatcher.submit<SetControlValue<Control_T>>(
              inlet, LinearNormalizer::from01(*sl, sl->value()));
        });
    QObject::connect(
        sl, &score::QGraphicsXYZSpinboxChooser::sliderReleased, context, [&ctx, sl]() {
          ctx.dispatcher.commit();
          sl->moving = false;
        });

    QObject::connect(&inlet, &Control_T::valueChanged, sl, [=](const ossia::value& val) {
      if(!sl->moving)
        sl->setValue(LinearNormalizer::to01(*sl, ossia::convert<ossia::vec3f>(val)));
    });

    return sl;
  }
};

struct MultiSlider
{
  static Process::PortItemLayout layout() noexcept
  {
    return Process::DefaultControlLayouts::slider();
  }

  template <typename T>
  static auto make_widget(
      T& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    SCORE_TODO;
    return nullptr; // TODO
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& slider, Control_T& inlet, const score::DocumentContext& ctx,
      QGraphicsItem* parent, QObject* context)
  {
    auto sl = new score::QGraphicsMultiSlider{nullptr};
    initWidgetProperties(inlet, *sl);
    sl->setValue(inlet.value());
    sl->setRange(inlet.domain());

    QObject::connect(
        sl, &score::QGraphicsMultiSlider::sliderMoved, context, [=, &inlet, &ctx] {
          sl->moving = true;
          ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, sl->value());
        });
    QObject::connect(
        sl, &score::QGraphicsMultiSlider::sliderReleased, context, [&ctx, sl]() {
          ctx.dispatcher.commit();
          sl->moving = false;
        });

    QObject::connect(&inlet, &Control_T::valueChanged, sl, [=](const ossia::value& val) {
      if(!sl->moving)
        sl->setValue(std::move(val));
    });

    return sl;
  }
};

struct MultiSliderXY
{
  static Process::PortItemLayout layout() noexcept
  {
    return Process::DefaultControlLayouts::slider();
  }

  template <typename T>
  static auto make_widget(
      T& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    SCORE_TODO;
    return nullptr; // TODO
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& slider, Control_T& inlet, const score::DocumentContext& ctx,
      QGraphicsItem* parent, QObject* context)
  {
    auto sl = new score::QGraphicsMultiSliderXY{nullptr};
    initWidgetProperties(inlet, *sl);
    sl->setValue(inlet.value());
    sl->setRange(inlet.domain());

    QObject::connect(
        sl, &score::QGraphicsMultiSliderXY::sliderMoved, context, [=, &inlet, &ctx] {
      sl->moving = true;
      ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, sl->value());
    });
    QObject::connect(
        sl, &score::QGraphicsMultiSliderXY::sliderReleased, context, [&ctx, sl]() {
      ctx.dispatcher.commit();
      sl->moving = false;
    });

    QObject::connect(&inlet, &Control_T::valueChanged, sl, [=](const ossia::value& val) {
      if(!sl->moving)
        sl->setValue(std::move(val));
    });

    return sl;
  }
};

/// Outlets
using Bargraph = FloatDisplay;
}
