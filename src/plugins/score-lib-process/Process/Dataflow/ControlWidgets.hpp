#pragma once
#include <Process/Commands/SetControlValue.hpp>
#include <Process/Dataflow/TimeSignature.hpp>
#include <Process/Dataflow/ControlWidgetDomains.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/graphics/GraphicWidgets.hpp>
#include <score/graphics/widgets/QGraphicsMultiSlider.hpp>
#include <score/graphics/GraphicsItem.hpp>
#include <score/tools/Unused.hpp>
#include <score/widgets/SignalUtils.hpp>
#include <score/widgets/ControlWidgets.hpp>
#include <score/widgets/ComboBox.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QCheckBox>
#include <QGraphicsItem>
#include <QLineEdit>
#include <QPalette>
#include <QTextDocument>
#include <private/qwidgettextcontrol_p.h>
#include <score_lib_process_export.h>

namespace WidgetFactory
{
static_assert(std::numeric_limits<float>::is_iec559, "IEEE 754 required");

template <typename T>
using SetControlValue = typename std::conditional_t<
    std::is_base_of<Process::ControlInlet, T>::value,
    Process::SetControlValue,
    Process::SetControlOutletValue
>;

template <typename Normalizer, typename T>
using ConcreteNormalizer = std::conditional_t<
  std::is_base_of_v<Process::ControlInlet, T> || std::is_base_of_v<Process::ControlOutlet, T>,
  UpdatingNormalizer<Normalizer, T>,
  FixedNormalizer<Normalizer>
>;
template <typename ControlUI, typename Normalizer, bool Control>
struct FloatControl
{
  template <typename T, typename Control_T>
  static auto make_widget(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QWidget* parent,
      QObject* context)
  {
    ConcreteNormalizer<Normalizer, T> norm{slider};

    auto sl = new score::ValueDoubleSlider{parent};
    sl->setOrientation(Qt::Horizontal);
    sl->setContentsMargins(0, 0, 0, 0);
    bindFloatDomain(slider, inlet, *sl);
    sl->setValue(norm.to01(ossia::convert<double>(inlet.value())));

    if constexpr(Control)
    {
      QObject::connect(sl, &score::DoubleSlider::sliderMoved, context, [sl, norm, &inlet, &ctx](double v) {
        sl->moving = true;
        ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, norm.from01(sl->value()));
      });
      QObject::connect(sl, &score::DoubleSlider::sliderReleased, context, [sl, norm, &inlet, &ctx]() {
        ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, norm.from01(sl->value()));
        ctx.dispatcher.commit();
        sl->moving = false;
      });
    }

    QObject::connect(&inlet, &Control_T::valueChanged, sl, [sl, norm] (ossia::value val) {
      if constexpr(Control)
      {
        if (!sl->moving)
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
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context)
  {
    ConcreteNormalizer<Normalizer, T> norm{slider};

    auto sl = new ControlUI{nullptr};
    bindFloatDomain(slider, inlet, *sl);
    sl->setValue(norm.to01(ossia::convert<double>(inlet.value())));

    if constexpr(Control)
    {
      QObject::connect(sl, &ControlUI::sliderMoved, context, [sl, norm, &inlet, &ctx] {
        sl->moving = true;
        ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, norm.from01(sl->value()));
      });
      QObject::connect(sl, &ControlUI::sliderReleased, context, [sl, norm, &inlet, &ctx] {
        ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, norm.from01(sl->value()));
        ctx.dispatcher.commit();
        sl->moving = false;
      });
    }

    QObject::connect(&inlet, &Control_T::valueChanged, sl, [sl, norm](ossia::value val) {
      if constexpr(Control)
      {
        if (!sl->moving)
          sl->setValue(norm.to01(ossia::convert<double>(val)));
      }
      else
      {
        sl->setValue(norm.to01(ossia::convert<double>(val)));
      }
    });

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
  template <typename T, typename Control_T>
  static auto make_widget(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QWidget* parent,
      QObject* context)
  {
    auto sl = new score::ValueSlider{parent};
    sl->setOrientation(Qt::Horizontal);
    bindIntDomain(slider, inlet, *sl);
    sl->setValue(ossia::convert<int>(inlet.value()));
    sl->setContentsMargins(0, 0, 0, 0);

    QObject::connect(sl, &score::IntSlider::sliderMoved, context, [sl, &inlet, &ctx](int p) {
      sl->moving = true;
      ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, p);
    });
    QObject::connect(sl, &score::IntSlider::sliderReleased, context, [sl, &inlet, &ctx] {
      ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, sl->value());
      ctx.dispatcher.commit();
      sl->moving = false;
    });

    QObject::connect(&inlet, &Control_T::valueChanged, sl, [sl](ossia::value val) {
      if (!sl->moving)
        sl->setValue(ossia::convert<int>(val));
    });

    return sl;
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context)
  {
    auto sl = new score::QGraphicsIntSlider{nullptr};
    bindIntDomain(slider, inlet, *sl);
    sl->setValue(ossia::convert<int>(inlet.value()));

    QObject::connect(sl, &score::QGraphicsIntSlider::sliderMoved, context, [=, &inlet, &ctx] {
      sl->moving = true;
      ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, sl->value());
    });
    QObject::connect(sl, &score::QGraphicsIntSlider::sliderReleased, context, [&ctx, sl]() {
      ctx.dispatcher.commit();
      sl->moving = false;
    });

    QObject::connect(&inlet, &Control_T::valueChanged, sl, [=](ossia::value val) {
      if (!sl->moving)
        sl->setValue(ossia::convert<int>(val));
    });

    return sl;
  }
};

struct IntSpinBox
{
  template <typename T, typename Control_T>
  static auto make_widget(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QWidget* parent,
      QObject* context)
  {
    auto sl = new QSpinBox{parent};
    bindIntDomain(slider, inlet, *sl);
    sl->setValue(ossia::convert<int>(inlet.value()));
    sl->setContentsMargins(0, 0, 0, 0);

    QObject::connect(
        sl, SignalUtils::QSpinBox_valueChanged_int(), context, [&inlet, &ctx](int val) {
          CommandDispatcher<>{ctx.commandStack}.submit<SetControlValue<Control_T>>(inlet, val);
        });

    QObject::connect(&inlet, &Control_T::valueChanged, sl, [sl](ossia::value val) {
      sl->setValue(ossia::convert<int>(val));
    });

    return sl;
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context)
  {
    auto sl = new score::QGraphicsIntSlider{nullptr};
    sl->setValue(ossia::convert<int>(inlet.value()));
    bindIntDomain(slider, inlet, *sl);

    QObject::connect(sl, &score::QGraphicsIntSlider::sliderMoved, context, [=, &inlet, &ctx] {
      sl->moving = true;
      ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, sl->value());
    });
    QObject::connect(sl, &score::QGraphicsIntSlider::sliderReleased, context, [&ctx, sl]() {
      ctx.dispatcher.commit();
      sl->moving = false;
    });

    QObject::connect(&inlet, &Control_T::valueChanged, sl, [=](ossia::value val) {
      if (!sl->moving)
        sl->setValue(ossia::convert<int>(val));
    });

    return sl;
  }
};

struct Toggle
{
  template <typename T, typename Control_T>
  static auto make_widget(
      const T& toggle,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QWidget* parent,
      QObject* context)
  {
    auto sl = new QCheckBox{parent};
    sl->setChecked(ossia::convert<bool>(inlet.value()));
    QObject::connect(sl, &QCheckBox::toggled, context, [&inlet, &ctx](bool val) {
      CommandDispatcher<>{ctx.commandStack}.submit<SetControlValue<Control_T>>(inlet, val);
    });

    QObject::connect(&inlet, &Control_T::valueChanged, sl, [sl](ossia::value val) {
      sl->setChecked(ossia::convert<bool>(val));
    });

    return sl;
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& toggle,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context)
  {
    auto cb = new score::QGraphicsCheckBox{nullptr};

    QObject::connect(cb, &score::QGraphicsCheckBox::toggled, context, [=, &inlet, &ctx] (bool toggled){
      ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, toggled);
      ctx.dispatcher.commit();
    });

    QObject::connect(&inlet, &Control_T::valueChanged, cb, [cb](ossia::value val) {
      cb->setState(ossia::convert<bool>(val));
    });

    return cb;
  }
};

struct Button
{
  template <typename T, typename Control_T>
  static auto make_widget(
      const T& slider,
      const Control_T& inlet,
      const score::DocumentContext& ctx,
      QWidget* parent,
      QObject* context)
  {
    auto sl = new QPushButton{parent};
    sl->setText(inlet.customData().isEmpty() ? QObject::tr("Bang") : inlet.customData());
    sl->setContentsMargins(0, 0, 0, 0);

    // TODO should we not make a command here
    auto& cinlet = const_cast<Control_T&>(inlet);
    QObject::connect(sl, &QPushButton::pressed, context, [&cinlet] { cinlet.setValue(true); });
    QObject::connect(sl, &QPushButton::released, context, [&cinlet] { cinlet.setValue(false); });

    return sl;
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context)
  {
    auto toggle = new score::QGraphicsButton{nullptr};

    QObject::connect(toggle, &score::QGraphicsButton::pressed, context, [=, &inlet, &ctx] (bool pressed){
      ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, pressed);
      ctx.dispatcher.commit();
    });

    return toggle;
  }
};
struct ChooserToggle
{
  template <typename T>
  static constexpr auto getAlternatives(const T& t) -> decltype(auto)
  {
    if constexpr (std::is_member_function_pointer_v<decltype(&T::alternatives)>)
    {
      return t.alternatives();
    }
    else
    {
      return t.alternatives;
    }
  }
  template <typename T, typename Control_T>
  static auto make_widget(
      const T& control,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QWidget* parent,
      QObject* context)
  {

    const auto& alts = getAlternatives(control);
    SCORE_ASSERT(alts.size() == 2);
    auto toggleBtn = new score::ToggleButton{alts, parent};
    toggleBtn->setCheckable(true);
    bool b = ossia::convert<bool>(inlet.value());
    if (b && !toggleBtn->isChecked())
      toggleBtn->toggle();
    else if (!b && toggleBtn->isChecked())
      toggleBtn->toggle();

    QObject::connect(toggleBtn, &score::ToggleButton::toggled, context, [&inlet, &ctx](bool val) {
      CommandDispatcher<>{ctx.commandStack}.submit<SetControlValue<Control_T>>(inlet, val);
    });

    QObject::connect(&inlet, &Control_T::valueChanged, toggleBtn, [toggleBtn](ossia::value val) {
      bool b = ossia::convert<bool>(val);
      if (b && !toggleBtn->isChecked())
        toggleBtn->toggle();
      else if (!b && toggleBtn->isChecked())
        toggleBtn->toggle();
    });

    return toggleBtn;
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& control,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context)
  {
    const auto& alts = getAlternatives(control);
    SCORE_ASSERT(alts.size() == 2);
    auto toggle = new score::QGraphicsToggle{alts[0], alts[1], nullptr};

    QObject::connect(toggle, &score::QGraphicsToggle::toggled, context, [=, &inlet, &ctx] (bool toggled){
      ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, toggled);
      ctx.dispatcher.commit();
    });

    QObject::connect(&inlet, &Control_T::valueChanged, toggle, [toggle](ossia::value val) {
      toggle->setState(ossia::convert<bool>(val));
    });

    return toggle;
  }
};

struct LineEdit
{
  template <typename T, typename Control_T>
  static auto make_widget(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QWidget* parent,
      QObject* context)
  {
    auto sl = new QLineEdit{parent};
    sl->setText(QString::fromStdString(ossia::convert<std::string>(inlet.value())));
    sl->setContentsMargins(0, 0, 0, 0);
    sl->setMaximumWidth(70);
    QObject::connect(sl, &QLineEdit::editingFinished, context, [sl, &inlet, &ctx]() {
      CommandDispatcher<>{ctx.commandStack}.submit<SetControlValue<Control_T>>(
          inlet, sl->text().toStdString());
    });

    QObject::connect(&inlet, &Control_T::valueChanged, sl, [sl](ossia::value val) {
      sl->setText(QString::fromStdString(ossia::convert<std::string>(val)));
    });

    return sl;
  }
  struct LineEditItem : public QGraphicsTextItem
  {
    LineEditItem() {
      setTextInteractionFlags(Qt::TextEditorInteraction);
      auto ctl = this->findChild<QWidgetTextControl*>();
      if(ctl)
      {
        ctl->setAcceptRichText(false);
      }
    }
  };
  template <typename T, typename Control_T>
  static LineEditItem* make_item(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context)
  {
    auto sl = new LineEditItem{};
    sl->setTextWidth(180.);
    sl->setDefaultTextColor(QColor{"#E0B01E"});
    sl->setCursor(Qt::IBeamCursor);

    sl->setPlainText(QString::fromStdString(ossia::convert<std::string>(inlet.value())));

    auto doc = sl->document();
    QObject::connect(doc, &QTextDocument::contentsChanged, context, [=, &inlet, &ctx] {
      CommandDispatcher<>{ctx.commandStack}.submit<SetControlValue<Control_T>>(
          inlet, doc->toPlainText().toStdString());
    });
    QObject::connect(&inlet, &Control_T::valueChanged, sl, [=](const ossia::value& val) {
      auto str = QString::fromStdString(ossia::convert<std::string>(val));
      if (str != doc->toPlainText())
        doc->setPlainText(str);
    });

    return sl;
  }
};

struct Enum
{
  static const auto& toStd(const char* const& s) { return s; }
  static const auto& toStd(const std::string& s) { return s; }
  static auto toStd(const QString& s) { return s.toStdString(); }

  static const auto& convert(const std::string& str, const char*) { return str; }
  static auto convert(const std::string& str, const QString&)
  {
    return QString::fromStdString(str);
  }

  template <typename T, typename Control_T>
  static auto make_widget(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QWidget* parent,
      QObject* context)
  {
    const auto& values = slider.getValues();
    using val_t = std::remove_reference_t<decltype(values[0])>;
    auto sl = new QComboBox{parent};
    for (const auto& e : values)
    {
      sl->addItem(e);
    }

    auto set_index = [values, sl](const ossia::value& val) {
      auto v = ossia::convert<std::string>(val);
      auto it = ossia::find(values, convert(v, val_t{}));
      if (it != values.end())
      {
        sl->setCurrentIndex(std::distance(values.begin(), it));
      }
    };
    set_index(inlet.value());

    QObject::connect(
        sl,
        SignalUtils::QComboBox_currentIndexChanged_int(),
        context,
        [values, &inlet, &ctx](int idx) {
          CommandDispatcher<>{ctx.commandStack}.submit<SetControlValue<Control_T>>(
              inlet, toStd(values[idx]));
        });

    QObject::connect(
        &inlet, &Control_T::valueChanged, sl, [=](const ossia::value& val) { set_index(val); });

    return sl;
  }

  template <typename T, typename Control_T>
  static auto make_item(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context)
  {
    const auto& values = slider.getValues();
    using val_t = std::remove_reference_t<decltype(values[0])>;

    auto sl = slider.pixmaps.empty() || slider.pixmaps[0] == nullptr
                  ? new score::QGraphicsEnum{values, nullptr}
                  : (score::QGraphicsEnum*)new score::QGraphicsPixmapEnum{
                      values, slider.pixmaps, nullptr};

    auto set_index = [values, sl](const ossia::value& val) {
      auto v = ossia::convert<std::string>(val);
      auto it = ossia::find(values, convert(v, val_t{}));
      if (it != values.end())
      {
        sl->setValue(std::distance(values.begin(), it));
      }
    };

    set_index(inlet.value());

    QObject::connect(
        sl, &score::QGraphicsEnum::currentIndexChanged, context, [sl, &inlet, &ctx](int idx) {
          ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, toStd(sl->array[idx]));
          ctx.dispatcher.commit();
        });

    QObject::connect(
        &inlet, &Control_T::valueChanged, sl, [=](const ossia::value& val) { set_index(val); });

    return sl;
  }
};

struct ComboBox
{
  template <typename U, typename Control_T>
  static auto make_widget(
      const U& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QWidget* parent,
      QObject* context)
  {
    const auto& values = slider.getValues();
    auto sl = new score::ComboBox{parent};
    for (auto& e : values)
    {
      sl->addItem(e.first);
    }
    sl->setContentsMargins(0, 0, 0, 0);

    auto set_index = [values, sl](const ossia::value& val) {
      auto it = ossia::find_if(values, [&](const auto& pair) { return pair.second == val; });
      if (it != values.end())
      {
        sl->setCurrentIndex(std::distance(values.begin(), it));
      }
    };
    set_index(inlet.value());

    QObject::connect(
        sl,
        SignalUtils::QComboBox_currentIndexChanged_int(),
        context,
        [values, &inlet, &ctx](int idx) {
          CommandDispatcher<>{ctx.commandStack}.submit<SetControlValue<Control_T>>(
              inlet, values[idx].second);
        });

    QObject::connect(
        &inlet, &Control_T::valueChanged, sl, [=](const ossia::value& val) { set_index(val); });

    return sl;
  }

  template <typename U, typename Control_T>
  static QGraphicsItem* make_item(
      const U& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context)
  {
    const auto N = slider.count();

    const auto& values = slider.getValues();
    QStringList arr;
    arr.reserve(N);
    for (std::size_t i = 0; i < N; i++)
      arr.push_back(values[i].first);

    auto sl = new score::QGraphicsCombo{arr, nullptr};

    auto set_index = [values, sl](const ossia::value& val) {
      auto it = ossia::find_if(values, [&](const auto& pair) { return pair.second == val; });
      if (it != values.end())
      {
        sl->setValue(std::distance(values.begin(), it));
      }
    };
    set_index(inlet.value());

    QObject::connect(sl, &score::QGraphicsCombo::sliderMoved, context, [values, sl, &inlet, &ctx] {
      sl->moving = true;
      ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, values[sl->value()].second);
    });
    QObject::connect(sl, &score::QGraphicsCombo::sliderReleased, context, [sl, &ctx] {
      ctx.dispatcher.commit();
      sl->moving = false;
    });

    QObject::connect(&inlet, &Control_T::valueChanged, sl, [=](const ossia::value& val) {
      if (sl->moving)
        return;

      set_index(val);
    });

    return sl;
  }
};

struct TimeSignatureValidator final : public QValidator
{
  using QValidator::QValidator;
  State validate(QString& str, int&) const override
  {
    auto p = Control::get_time_signature(str.toStdString());
    if (!p)
      return State::Invalid;

    return State::Acceptable;
  }
};

struct RGBAEdit
{
  // TODO
};

struct XYZEdit
{
  // TODO
};

struct HSVSlider
{
  template <typename T, typename Control_T>
  static auto make_widget(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QWidget* parent,
      QObject* context)
  {
    SCORE_TODO;
    return nullptr; // TODO
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context)
  {
    auto sl = new score::QGraphicsHSVChooser{nullptr};
    sl->setValue(ossia::convert<ossia::vec4f>(inlet.value()));

    QObject::connect(sl, &score::QGraphicsHSVChooser::sliderMoved, context, [=, &inlet, &ctx] {
      sl->moving = true;
      ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, sl->value());
    });
    QObject::connect(sl, &score::QGraphicsHSVChooser::sliderReleased, context, [&ctx, sl]() {
      ctx.dispatcher.commit();
      sl->moving = false;
    });

    QObject::connect(&inlet, &Control_T::valueChanged, sl, [=](ossia::value val) {
      if (!sl->moving)
        sl->setValue(ossia::convert<ossia::vec4f>(val));
    });

    return sl;
  }
};

struct XYSlider
{
  template <typename T, typename Control_T>
  static auto make_widget(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QWidget* parent,
      QObject* context)
  {
    SCORE_TODO;
    return nullptr; // TODO
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context)
  {
    auto sl = new score::QGraphicsXYChooser{nullptr};
    sl->setValue(ossia::convert<ossia::vec2f>(inlet.value()));

    QObject::connect(sl, &score::QGraphicsXYChooser::sliderMoved, context, [=, &inlet, &ctx] {
      sl->moving = true;
      ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, sl->value());
    });
    QObject::connect(sl, &score::QGraphicsXYChooser::sliderReleased, context, [&ctx, sl]() {
      ctx.dispatcher.commit();
      sl->moving = false;
    });

    QObject::connect(&inlet, &Control_T::valueChanged, sl, [=](ossia::value val) {
      if (!sl->moving)
        sl->setValue(ossia::convert<ossia::vec2f>(val));
    });

    return sl;
  }
};


struct MultiSlider
{
  template <typename T, typename Control_T>
  static auto make_widget(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QWidget* parent,
      QObject* context)
  {
    SCORE_TODO;
    return nullptr; // TODO
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context)
  {
    auto sl = new score::QGraphicsMultiSlider{nullptr};
    sl->setValue(inlet.value());

    QObject::connect(sl, &score::QGraphicsMultiSlider::sliderMoved, context, [=, &inlet, &ctx] {
      sl->moving = true;
      ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, sl->value());
    });
    QObject::connect(sl, &score::QGraphicsMultiSlider::sliderReleased, context, [&ctx, sl]() {
      ctx.dispatcher.commit();
      sl->moving = false;
    });

    QObject::connect(&inlet, &Control_T::valueChanged, sl, [=](ossia::value val) {
      if (!sl->moving)
        sl->setValue(std::move(val));
    });

    return sl;
  }
};


/// Outlets
using Bargraph = FloatDisplay;
}
