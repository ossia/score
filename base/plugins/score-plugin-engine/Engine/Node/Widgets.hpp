#pragma once
#include <Engine/Node/Port.hpp>
#include <Process/Dataflow/Port.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/widgets/DoubleSlider.hpp>
#include <score/widgets/SignalUtils.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/TextLabel.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <Scenario/Commands/SetControlValue.hpp>
#include <Engine/Node/BaseWidgets.hpp>
#include <ossia/network/domain/domain.hpp>
#include <State/Value.hpp>
#include <QPainter>
#include <QCheckBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QApplication>
#include <score_plugin_engine_export.h>

namespace Process
{
using SetControlValue = Scenario::Command::SetControlValue;
SCORE_PLUGIN_ENGINE_EXPORT const QPalette& transparentPalette();
static inline auto transparentStylesheet()
{
  return QStringLiteral("QWidget { background-color:transparent }");
}

struct FloatSlider : ControlInfo
{
  using type = float;
  const float min{};
  const float max{};
  const float init{};

  auto getMin() const { return min; }
  auto getMax() const { return max; }

  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    auto p = new Process::ControlInlet(id, parent);
    p->type = Process::PortType::Message;
    p->setValue(init);
    p->setDomain(ossia::make_domain(min, max));
    p->setCustomData(name);
    return p;
  }

  template<typename T>
  static auto make_widget(const T& slider, ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    const auto min = slider.getMin();
    const auto max = slider.getMax();
    auto sl = new ValueDoubleSlider{parent};
    sl->setOrientation(Qt::Horizontal);
    sl->setContentsMargins(0, 0, 0, 0);
    sl->min = min;
    sl->max = max;
    sl->setValue((ossia::convert<double>(inlet.value()) - min) / (max - min));

    QObject::connect(sl, &score::DoubleSlider::sliderMoved,
            context, [=,&inlet,&ctx] (int v) {
      sl->moving = true;
      ctx.dispatcher.submitCommand<SetControlValue>(inlet, min + (v / score::DoubleSlider::max) * (max - min));
    });
    QObject::connect(sl, &score::DoubleSlider::sliderReleased,
            context, [&ctx,sl] () {
      ctx.dispatcher.commit();
      sl->moving = false;
    });

    QObject::connect(&inlet, &ControlInlet::valueChanged,
            context, [=] (ossia::value val) {
      if(!sl->moving)
        sl->setValue((ossia::convert<double>(val) - min) / (max - min));
    });

    return sl;
  }

  template<typename T>
  static auto make_item(const T& slider, ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    return make_widget(slider, inlet, ctx, parent, context);
  }
};

struct LogFloatSlider : ControlInfo
{
  using type = float;
  const float min{};
  const float max{};
  const float init{};

  auto getMin() const { return min; }
  auto getMax() const { return max; }

  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    auto p = new Process::ControlInlet(id, parent);
    p->type = Process::PortType::Message;
    p->setValue(init);
    p->setDomain(ossia::make_domain(min, max));
    p->setCustomData(name);
    return p;
  }

  static float from01(float min, float max, float val)
  {
    return std::exp2(min + val * (max - min));
  }
  static float to01(float min, float max, float val)
  {
    return (std::log2(val) - min) / (max - min);
  }
  template<typename T>
  static auto make_widget(const T& slider, ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    const auto min = std::log2(slider.getMin());
    const auto max = std::log2(slider.getMax());
    auto sl = new ValueLogDoubleSlider{parent};
    sl->setOrientation(Qt::Horizontal);
    sl->setContentsMargins(0, 0, 0, 0);
    sl->min = min;
    sl->max = max;
    sl->setValue(to01(min, max, ossia::convert<double>(inlet.value())));

    QObject::connect(sl, &score::DoubleSlider::sliderMoved,
            context, [=,&inlet,&ctx] (int v) {
      sl->moving = true;
      ctx.dispatcher.submitCommand<SetControlValue>(inlet, from01(min, max, v / score::DoubleSlider::max));
    });
    QObject::connect(sl, &score::DoubleSlider::sliderReleased,
            context, [&ctx,sl] () {
      ctx.dispatcher.commit();
      sl->moving = false;
    });

    QObject::connect(&inlet, &ControlInlet::valueChanged,
            context, [=] (ossia::value val) {
      if(!sl->moving)
        sl->setValue(to01(min, max, ossia::convert<double>(val)));
    });

    return sl;
  }

  template<typename T>
  static auto make_item(const T& slider, ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    return make_widget(slider, inlet, ctx, parent, context);
  }
};

struct IntSlider: ControlInfo
{
  using type = int;
  const int min{};
  const int max{};
  const int init{};

  auto getMin() const { return min; }
  auto getMax() const { return max; }

  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    auto p = new Process::ControlInlet(id, parent);
    p->type = Process::PortType::Message;
    p->setValue(init);
    p->setDomain(ossia::make_domain(min, max));
    p->setCustomData(name);
    return p;
  }

  template<typename T>
  static auto make_widget(const T& slider, ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    const auto min = slider.getMin();
    const auto max = slider.getMax();
    auto sl = new ValueSlider{parent};
    sl->setOrientation(Qt::Horizontal);
    sl->setRange(min, max);
    sl->setValue(ossia::convert<int>(inlet.value()));
    sl->setContentsMargins(0, 0, 0, 0);

    QObject::connect(sl, &QSlider::sliderMoved,
            context, [sl,&inlet,&ctx] (int p) {
      sl->moving = true;
      ctx.dispatcher.submitCommand<SetControlValue>(inlet, p);
    });
    QObject::connect(sl, &QSlider::sliderReleased,
            context, [&ctx,sl] () {
      ctx.dispatcher.commit();
      sl->moving = false;
    });

    QObject::connect(&inlet, &ControlInlet::valueChanged,
            context, [sl] (ossia::value val) {
      if(!sl->moving)
        sl->setValue(ossia::convert<int>(val));
    });

    return sl;
  }

  template<typename T>
  static auto make_item(const T& slider, ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    return make_widget(slider, inlet, ctx, parent, context);
  }

};
struct IntSpinBox: ControlInfo
{
  using type = int;
  const int min{};
  const int max{};
  const int init{};

  auto getMin() const { return min; }
  auto getMax() const { return max; }

  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    auto p = new Process::ControlInlet(id, parent);
    p->type = Process::PortType::Message;
    p->setValue(init);
    p->setDomain(ossia::make_domain(min, max));
    p->setCustomData(name);
    return p;
  }

  template<typename T>
  static auto make_widget(const T& slider, ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    const auto min = slider.getMin();
    const auto max = slider.getMax();
    auto sl = new QSpinBox{parent};
    sl->setRange(min, max);
    sl->setValue(ossia::convert<int>(inlet.value()));
    sl->setContentsMargins(0, 0, 0, 0);

    QObject::connect(sl, SignalUtils::QSpinBox_valueChanged_int(),
            context, [&inlet,&ctx] (int val) {
      CommandDispatcher<>{ctx.commandStack}.submitCommand<SetControlValue>(inlet, val);
    });

    QObject::connect(&inlet, &ControlInlet::valueChanged,
            context, [sl] (ossia::value val) {
      sl->setValue(ossia::convert<int>(val));
    });

    return sl;
  }

  template<typename T>
  static auto make_item(const T& slider, ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    return make_widget(slider, inlet, ctx, parent, context);
  }

};
struct Toggle: ControlInfo
{
  using type = bool;
  const bool init{};
  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    auto p = new Process::ControlInlet(id, parent);
    p->type = Process::PortType::Message;
    p->setValue(init);
    p->setCustomData(name);
    return p;
  }

  template<typename T>
  static auto make_widget(const T& slider, ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    auto sl = new QCheckBox{parent};
    sl->setChecked(ossia::convert<bool>(inlet.value()));
    sl->setContentsMargins(0, 0, 0, 0);

    QObject::connect(sl, &QCheckBox::toggled,
            context, [&inlet,&ctx] (bool val) {
      CommandDispatcher<>{ctx.commandStack}.submitCommand<SetControlValue>(inlet, val);
    });

    QObject::connect(&inlet, &ControlInlet::valueChanged,
            context, [sl] (ossia::value val) {
      sl->setChecked(ossia::convert<bool>(val));
    });

    return sl;
  }

  template<typename T>
  static auto make_item(const T& slider, ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    return make_widget(slider, inlet, ctx, parent, context);
  }
};


struct ChooserToggle: ControlInfo
{
  using type = bool;
  std::array<const char*, 2> alternatives;
  const bool init{};
  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    auto p = new Process::ControlInlet(id, parent);
    p->type = Process::PortType::Message;
    p->setValue(init);
    p->setCustomData(name);
    return p;
  }

  template<typename T>
  static auto make_widget(const T& slider, ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    auto sl = new ToggleButton{parent};
    sl->setCheckable(true);
    sl->alternatives[0] = slider.alternatives[0];
    sl->alternatives[1] = slider.alternatives[1];
    sl->setChecked(ossia::convert<bool>(inlet.value()));
    sl->setContentsMargins(0, 0, 0, 0);

    QObject::connect(sl, &QCheckBox::toggled,
            context, [&inlet,&ctx] (bool val) {
      CommandDispatcher<>{ctx.commandStack}.submitCommand<SetControlValue>(inlet, val);
    });

    QObject::connect(&inlet, &ControlInlet::valueChanged,
            context, [sl] (ossia::value val) {
      sl->setChecked(ossia::convert<bool>(val));
    });

    return sl;
  }

  template<typename T>
  static auto make_item(const T& slider, ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    return make_widget(slider, inlet, ctx, parent, context);
  }
};
struct LineEdit: ControlInfo
{
  using type = std::string;
  const QLatin1Literal init{};
  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    auto p = new Process::ControlInlet(id, parent);
    p->type = Process::PortType::Message;
    p->setValue(std::string(init.latin1(), init.size()));
    p->setCustomData(name);
    return p;
  }

  template<typename T>
  static auto make_widget(const T& slider, ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    auto sl = new QLineEdit{parent};
    sl->setText(QString::fromStdString(ossia::convert<std::string>(inlet.value())));
    sl->setContentsMargins(0, 0, 0, 0);

    QObject::connect(sl, &QLineEdit::editingFinished,
            context, [sl,&inlet,&ctx] () {
      CommandDispatcher<>{ctx.commandStack}.submitCommand<SetControlValue>(inlet, sl->text().toStdString());
    });

    QObject::connect(&inlet, &ControlInlet::valueChanged,
            context, [sl] (ossia::value val) {
      sl->setText(QString::fromStdString(ossia::convert<std::string>(val)));
    });

    return sl;
  }
  template<typename T>
  static auto make_item(const T& slider, ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    return make_widget(slider, inlet, ctx, parent, context);
  }

};
struct RGBAEdit: ControlInfo
{
  using type = std::array<float, 4>;
  std::array<float, 4> init{};
};
struct XYZEdit: ControlInfo
{
  using type = std::array<float, 3>;
  std::array<float, 3> init{};
};
template<typename T, std::size_t N>
struct ComboBox: ControlInfo
{
  using type = T;
  const std::size_t init{};
  const std::array<std::pair<const char*, T>, N> values;

  const auto& getValues() const { return values; }

  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    auto p = new Process::ControlInlet(id, parent);
    p->type = Process::PortType::Message;
    p->setValue(values[init].second);
    p->setCustomData(name);
    return p;
  }

  template<typename U>
  static auto make_widget(const U& slider, ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    const auto& values = slider.getValues();
    auto sl = new QComboBox{parent};
    for(auto& e : values)
    {
      sl->addItem(e.first);
    }
    sl->setContentsMargins(0, 0, 0, 0);

    auto set_index = [values,sl] (const ossia::value& val)
    {
      auto v = ossia::convert<T>(val);
      auto it = ossia::find_if(values, [&] (const auto& pair) { return pair.second == v; });
      if(it != values.end())
      {
        sl->setCurrentIndex(std::distance(values.begin(), it));
      }
    };
    set_index(inlet.value());

    QObject::connect(sl, SignalUtils::QComboBox_currentIndexChanged_int(),
            context, [values,&inlet,&ctx] (int idx) {
      CommandDispatcher<>{ctx.commandStack}.submitCommand<SetControlValue>(inlet, values[idx].second);
    });

    QObject::connect(&inlet, &ControlInlet::valueChanged,
            context, [=] (const ossia::value& val) {
      set_index(val);
    });

    return sl;
  }

  template<typename U>
  static auto make_item(const U& slider, ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    const auto& values = slider.getValues();
    std::array<const char*, N> arr;
    for(std::size_t i = 0; i < N; i++)
      arr[i] = values[i].first;

    auto sl = new ComboSlider{arr, parent};

    sl->setRange(0, N - 1);
    sl->setContentsMargins(0, 0, 0, 0);
    sl->setOrientation(Qt::Horizontal);

    auto set_index = [values,sl] (const ossia::value& val)
    {
      auto v = ossia::convert<T>(val);
      auto it = ossia::find_if(values, [&] (const auto& pair) { return pair.second == v; });
      if(it != values.end())
      {
        sl->setValue(std::distance(values.begin(), it));
      }
    };
    set_index(inlet.value());

    QObject::connect(sl, &QSlider::sliderMoved,
            context, [values,sl,&inlet,&ctx] (int p) {
      sl->moving = true;
      ctx.dispatcher.submitCommand<SetControlValue>(inlet, values[p].second);
    });
    QObject::connect(sl, &QSlider::sliderReleased,
            context, [sl,&ctx] () {
      ctx.dispatcher.commit();
      sl->moving = false;
    });

    QObject::connect(&inlet, &ControlInlet::valueChanged,
            context, [=] (const ossia::value& val) {
      if(sl->moving)
        return;

      set_index(val);
    });

    return sl;
  }

};

template<typename ArrT>
struct Enum: ControlInfo
{
  using type = std::string;
  const std::size_t init{};
  const ArrT values;

  const auto& getValues() const { return values; }

  template<std::size_t N1>
  constexpr Enum(const char (&name)[N1], std::size_t i, const ArrT& v)
    : ControlInfo{name}
    , init{i}
    , values{v}
  {
  }

  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    auto p = new Process::ControlInlet(id, parent);
    p->type = Process::PortType::Message;
    p->setValue(std::string(values[init]));
    p->setCustomData(name);
    return p;
  }

  static const auto& toStd(const char* const& s) { return s; }
  static const auto& toStd(const std::string& s) { return s; }
  static auto toStd(const QString& s) { return s.toStdString(); }

  static const auto& convert(const std::string& str, const char*)
  { return str; }
  static auto convert(const std::string& str, const QString&)
  { return QString::fromStdString(str); }


  template<typename T>
  static auto make_widget(const T& slider, ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    const auto& values = slider.getValues();
    using val_t = std::remove_reference_t<decltype(values[0])>;
    auto sl = new QComboBox{parent};
    for(const auto& e : values)
    {
      sl->addItem(e);
    }

    auto set_index = [values,sl] (const ossia::value& val)
    {
      auto v = ossia::convert<std::string>(val);
      auto it = ossia::find(values, convert(v, val_t{}));
      if(it != values.end())
      {
        sl->setCurrentIndex(std::distance(values.begin(), it));
      }
    };
    set_index(inlet.value());

    QObject::connect(sl, SignalUtils::QComboBox_currentIndexChanged_int(),
            context, [values,&inlet,&ctx] (int idx) {
      CommandDispatcher<>{ctx.commandStack}.submitCommand<SetControlValue>(inlet, toStd(values[idx]));
    });

    QObject::connect(&inlet, &ControlInlet::valueChanged,
            context, [=] (const ossia::value& val) {
      set_index(val);
    });

    return sl;
  }

  template<typename T>
  static auto make_item(const T& slider, ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
  {
    const auto& values = slider.getValues();
    using val_t = std::remove_reference_t<decltype(values[0])>;
    auto sl = new ComboSlider{values, parent};
    sl->setRange(0, values.size() - 1);
    sl->setContentsMargins(0, 0, 0, 0);
    sl->setOrientation(Qt::Horizontal);

    auto set_index = [values,sl] (const ossia::value& val)
    {
      auto v = ossia::convert<std::string>(val);
      auto it = ossia::find(values, convert(v, val_t{}));
      if(it != values.end())
      {
        sl->setValue(std::distance(values.begin(), it));
      }
    };
    set_index(inlet.value());

    QObject::connect(sl, &QSlider::sliderMoved,
            context, [values,sl,&inlet,&ctx] (int p) {
      sl->moving = true;
      ctx.dispatcher.submitCommand<SetControlValue>(inlet, toStd(values[p]));
    });
    QObject::connect(sl, &QSlider::sliderReleased,
            context, [sl,&ctx] () {
      ctx.dispatcher.commit();
      sl->moving = false;
    });

    QObject::connect(&inlet, &ControlInlet::valueChanged,
            context, [=] (const ossia::value& val) {
      if(sl->moving)
        return;

      set_index(val);
    });

    return sl;
  }
};


template<typename T1, typename T2>
constexpr auto make_enum(const T1& t1, std::size_t s, const T2& t2)
{
    return Process::Enum<T2>(t1, s, t2);
}
/*
template<std::size_t N1, std::size_t N2>
Enum(const char (&name)[N1], std::size_t i, const std::array<const char*, N2>& v) -> Enum<std::array<const char*, N2>>;
*/
}
