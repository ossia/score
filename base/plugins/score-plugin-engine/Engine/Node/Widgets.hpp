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
#include <score/widgets/GraphicWidgets.hpp>
#include <State/Value.hpp>
#include <QPainter>
#include <QCheckBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QApplication>
#include <score_plugin_engine_export.h>
#include <QGraphicsProxyWidget>
#include <Engine/Node/TimeSignature.hpp>
namespace Control
{
using SetControlValue = Scenario::Command::SetControlValue;
SCORE_PLUGIN_ENGINE_EXPORT const QPalette& transparentPalette();
static inline auto transparentStylesheet()
{
  return QStringLiteral("QWidget { background-color:transparent }");
}

inline QGraphicsItem* wrapWidget(QWidget* widg)
{
  widg->setMaximumWidth(150);
  widg->setContentsMargins(0, 0, 0, 0);
  widg->setPalette(transparentPalette());
  widg->setAutoFillBackground(false);
  widg->setStyleSheet(transparentStylesheet());

  auto wrap = new QGraphicsProxyWidget{};
  wrap->setWidget(widg);
  wrap->setContentsMargins(0, 0, 0, 0);
  return wrap;
}

struct FloatSlider : ControlInfo
{
    static const constexpr bool must_validate = false;
    using type = float;
    const float min{};
    const float max{};
    const float init{};

    template<std::size_t N>
    constexpr FloatSlider(const char (&name)[N], float v1, float v2, float v3):
      ControlInfo{name}, min{v1}, max{v2}, init{v3}
    {
    }

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

    float fromValue(const ossia::value& v) const
    {
      return ossia::convert<float>(v);
    }
    ossia::value toValue(float v) const { return v; }

    template<typename T>
    static auto make_widget(const T& slider, Process::ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
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

      QObject::connect(&inlet, &Process::ControlInlet::valueChanged,
                       sl, [=] (ossia::value val) {
        if(!sl->moving)
          sl->setValue((ossia::convert<double>(val) - min) / (max - min));
      });

      return sl;
    }

    template<typename T>
    static QGraphicsItem* make_item(const T& slider, Process::ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
    {
      const auto min = slider.getMin();
      const auto max = slider.getMax();
      auto sl = new score::QGraphicsSlider{nullptr};
      sl->min = min;
      sl->max = max;
      sl->setRect({0., 0., 150., 15.});
      sl->setValue((ossia::convert<double>(inlet.value()) - min) / (max - min));

      QObject::connect(sl, &score::QGraphicsSlider::sliderMoved,
                       context, [=,&inlet,&ctx] {
        sl->moving = true;
        ctx.dispatcher.submitCommand<SetControlValue>(inlet, min + sl->value() * (max - min));
      });
      QObject::connect(sl, &score::QGraphicsSlider::sliderReleased,
                       context, [&ctx,sl] () {
        ctx.dispatcher.commit();
        sl->moving = false;
      });

      QObject::connect(&inlet, &Process::ControlInlet::valueChanged,
                       sl, [=] (ossia::value val) {
        if(!sl->moving)
          sl->setValue((ossia::convert<double>(val) - min) / (max - min));
      });

      return sl;
    }
};

struct LogFloatSlider : ControlInfo
{
    static const constexpr bool must_validate = false;
    using type = float;
    const float min{};
    const float max{};
    const float init{};

    template<std::size_t N>
    constexpr LogFloatSlider(const char (&name)[N], float v1, float v2, float v3):
      ControlInfo{name}, min{v1}, max{v2}, init{v3}
    {
    }

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

    float fromValue(const ossia::value& v) const
    {
      return ossia::convert<float>(v);
    }
    ossia::value toValue(float v) const { return v; }

    static float from01(float min, float max, float val)
    {
      return std::exp2(min + val * (max - min));
    }
    static float to01(float min, float max, float val)
    {
      return (std::log2(val) - min) / (max - min);
    }
    template<typename T>
    static auto make_widget(const T& slider, Process::ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
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

      QObject::connect(&inlet, &Process::ControlInlet::valueChanged,
                       sl, [=] (ossia::value val) {
        if(!sl->moving)
          sl->setValue(to01(min, max, ossia::convert<double>(val)));
      });

      return sl;
    }

    template<typename T>
    static QGraphicsItem* make_item(const T& slider, Process::ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
    {
      const auto min = std::log2(slider.getMin());
      const auto max = std::log2(slider.getMax());
      auto sl = new score::QGraphicsLogSlider{nullptr};
      sl->min = min;
      sl->max = max;
      sl->setRect({0., 0., 150., 15.});
      sl->setValue(to01(min, max, ossia::convert<double>(inlet.value())));

      QObject::connect(sl, &score::QGraphicsLogSlider::sliderMoved,
                       context, [=,&inlet,&ctx] {
        sl->moving = true;
        ctx.dispatcher.submitCommand<SetControlValue>(inlet, from01(min, max, sl->value()));
      });
      QObject::connect(sl, &score::QGraphicsLogSlider::sliderReleased,
                       context, [&ctx,sl] () {
        ctx.dispatcher.commit();
        sl->moving = false;
      });

      QObject::connect(&inlet, &Process::ControlInlet::valueChanged,
                       sl, [=] (ossia::value val) {
        if(!sl->moving)
          sl->setValue(to01(min, max, ossia::convert<double>(val)));
      });

      return sl;
    }
};

struct IntSlider: ControlInfo
{
    using type = int;
    const int min{};
    const int max{};
    const int init{};

    static const constexpr bool must_validate = false;
    template<std::size_t N>
    constexpr IntSlider(const char (&name)[N], int v1, int v2, int v3):
      ControlInfo{name}, min{v1}, max{v2}, init{v3}
    {
    }

    int fromValue(const ossia::value& v) const
    {
      return ossia::convert<int>(v);
    }
    ossia::value toValue(int v) const { return v; }

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
    static auto make_widget(const T& slider, Process::ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
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

      QObject::connect(&inlet, &Process::ControlInlet::valueChanged,
                       sl, [sl] (ossia::value val) {
        if(!sl->moving)
          sl->setValue(ossia::convert<int>(val));
      });

      return sl;
    }

    template<typename T>
    static QGraphicsItem* make_item(const T& slider, Process::ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
    {
      const auto min = slider.getMin();
      const auto max = slider.getMax();
      auto sl = new score::QGraphicsIntSlider{nullptr};
      sl->setRange(min, max);
      sl->setRect({0., 0., 150., 15.});
      sl->setValue(ossia::convert<int>(inlet.value()));

      QObject::connect(sl, &score::QGraphicsIntSlider::sliderMoved,
                       context, [=,&inlet,&ctx] {
        sl->moving = true;
        ctx.dispatcher.submitCommand<SetControlValue>(inlet, sl->value());
      });
      QObject::connect(sl, &score::QGraphicsIntSlider::sliderReleased,
                       context, [&ctx,sl] () {
        ctx.dispatcher.commit();
        sl->moving = false;
      });

      QObject::connect(&inlet, &Process::ControlInlet::valueChanged,
                       sl, [=] (ossia::value val) {
        if(!sl->moving)
          sl->setValue(ossia::convert<int>(val));
      });

      return sl;
    }

};
struct IntSpinBox: ControlInfo
{
    static const constexpr bool must_validate = false;
    using type = int;
    const int min{};
    const int max{};
    const int init{};

    int fromValue(const ossia::value& v) const
    {
      return ossia::convert<int>(v);
    }
    ossia::value toValue(int v) const { return v; }

    template<std::size_t N>
    constexpr IntSpinBox(const char (&name)[N], int v1, int v2, int v3):
      ControlInfo{name}, min{v1}, max{v2}, init{v3}
    {
    }

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
    static auto make_widget(const T& slider, Process::ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
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

      QObject::connect(&inlet, &Process::ControlInlet::valueChanged,
                       sl, [sl] (ossia::value val) {
        sl->setValue(ossia::convert<int>(val));
      });

      return sl;
    }

    template<typename T>
    static QGraphicsItem* make_item(const T& slider, Process::ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
    {
      const auto min = slider.getMin();
      const auto max = slider.getMax();
      auto sl = new score::QGraphicsIntSlider{nullptr};
      sl->setRange(min, max);
      sl->setRect({0., 0., 150., 15.});
      sl->setValue(ossia::convert<int>(inlet.value()));

      QObject::connect(sl, &score::QGraphicsIntSlider::sliderMoved,
                       context, [=,&inlet,&ctx] {
        sl->moving = true;
        ctx.dispatcher.submitCommand<SetControlValue>(inlet, sl->value());
      });
      QObject::connect(sl, &score::QGraphicsIntSlider::sliderReleased,
                       context, [&ctx,sl] () {
        ctx.dispatcher.commit();
        sl->moving = false;
      });

      QObject::connect(&inlet, &Process::ControlInlet::valueChanged,
                       sl, [=] (ossia::value val) {
        if(!sl->moving)
          sl->setValue(ossia::convert<int>(val));
      });

      return sl;
    }

};
struct Toggle: ControlInfo
{
    static const constexpr bool must_validate = false;
    template<std::size_t N>
    constexpr Toggle(const char (&name)[N], bool v1):
      ControlInfo{name}, init{v1}
    {
    }

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

    bool fromValue(const ossia::value& v) const
    {
      return ossia::convert<bool>(v);
    }
    ossia::value toValue(bool v) const { return v; }

    template<typename T>
    static auto make_widget(const T& slider, Process::ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
    {
      auto sl = new QCheckBox{parent};
      sl->setChecked(ossia::convert<bool>(inlet.value()));
      sl->setContentsMargins(0, 0, 0, 0);

      QObject::connect(sl, &QCheckBox::toggled,
                       context, [&inlet,&ctx] (bool val) {
        CommandDispatcher<>{ctx.commandStack}.submitCommand<SetControlValue>(inlet, val);
      });

      QObject::connect(&inlet, &Process::ControlInlet::valueChanged,
                       sl, [sl] (ossia::value val) {
        sl->setChecked(ossia::convert<bool>(val));
      });

      return sl;
    }

    template<typename T>
    static QGraphicsItem* make_item(const T& slider, Process::ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
    {
      return wrapWidget(make_widget(slider, inlet, ctx, parent, context));
    }
};


struct ChooserToggle: ControlInfo
{
    static const constexpr bool must_validate = false;
    template<std::size_t N>
    constexpr ChooserToggle(const char (&name)[N], std::array<const char*, 2> alt, bool v1):
      ControlInfo{name}, alternatives{alt}, init{v1}
    {
    }
    using type = bool;
    std::array<const char*, 2> alternatives;
    const bool init{};

    bool fromValue(const ossia::value& v) const
    {
      return ossia::convert<bool>(v);
    }
    ossia::value toValue(bool v) const { return v; }

    auto create_inlet(Id<Process::Port> id, QObject* parent) const
    {
      auto p = new Process::ControlInlet(id, parent);
      p->type = Process::PortType::Message;
      p->setValue(init);
      p->setCustomData(name);
      return p;
    }

    template<typename T>
    static auto make_widget(const T& slider, Process::ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
    {
      auto sl = new ToggleButton{slider.alternatives, parent};
      sl->setCheckable(true);
      bool b = ossia::convert<bool>(inlet.value());
      if(b && !sl->isChecked())
        sl->toggle();
      else if(!b && sl->isChecked())
        sl->toggle();
      sl->setContentsMargins(0, 0, 0, 0);

      QObject::connect(sl, &QCheckBox::toggled,
                       context, [&inlet,&ctx] (bool val) {
        CommandDispatcher<>{ctx.commandStack}.submitCommand<SetControlValue>(inlet, val);
      });

      QObject::connect(&inlet, &Process::ControlInlet::valueChanged,
                       sl, [sl] (ossia::value val) {
        bool b = ossia::convert<bool>(val);
        if(b && !sl->isChecked())
          sl->toggle();
        else if(!b && sl->isChecked())
          sl->toggle();
      });

      return sl;
    }

    template<typename T>
    static QGraphicsItem* make_item(const T& slider, Process::ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
    {
      return wrapWidget(make_widget(slider, inlet, ctx, parent, context));
    }
};
struct LineEdit: ControlInfo
{
    static const constexpr bool must_validate = false;
    template<std::size_t N>
    constexpr LineEdit(const char (&name)[N], QLatin1Literal v1):
      ControlInfo{name}, init{v1}
    {
    }

    std::string fromValue(const ossia::value& v) const
    {
      return ossia::convert<std::string>(v);
    }
    ossia::value toValue(std::string v) const { return ossia::value{std::move(v)}; }

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
    static auto make_widget(const T& slider, Process::ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
    {
      auto sl = new QLineEdit{parent};
      sl->setText(QString::fromStdString(ossia::convert<std::string>(inlet.value())));
      sl->setContentsMargins(0, 0, 0, 0);

      QObject::connect(sl, &QLineEdit::editingFinished,
                       context, [sl,&inlet,&ctx] () {
        CommandDispatcher<>{ctx.commandStack}.submitCommand<SetControlValue>(inlet, sl->text().toStdString());
      });

      QObject::connect(&inlet, &Process::ControlInlet::valueChanged,
                       sl, [sl] (ossia::value val) {
        sl->setText(QString::fromStdString(ossia::convert<std::string>(val)));
      });

      return sl;
    }
    template<typename T>
    static QGraphicsItem* make_item(const T& slider, Process::ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
    {
      return wrapWidget(make_widget(slider, inlet, ctx, parent, context));
    }

};
struct RGBAEdit: ControlInfo
{
    static const constexpr bool must_validate = false;
    using type = std::array<float, 4>;
    std::array<float, 4> init{};
};
struct XYZEdit: ControlInfo
{
    static const constexpr bool must_validate = false;
    using type = std::array<float, 3>;
    std::array<float, 3> init{};
};
template<typename T, std::size_t N>
struct ComboBox: ControlInfo
{
    static const constexpr bool must_validate = false;
    using type = T;
    const std::size_t init{};
    const std::array<std::pair<const char*, T>, N> values;

    template<std::size_t M, typename Arr>
    constexpr ComboBox(const char (&name)[M], std::size_t in, Arr arr):
      ControlInfo{name}, init{in}, values{arr}
    {
    }

    const auto& getValues() const { return values; }

    auto create_inlet(Id<Process::Port> id, QObject* parent) const
    {
      auto p = new Process::ControlInlet(id, parent);
      p->type = Process::PortType::Message;
      p->setValue(values[init].second);
      p->setCustomData(name);
      return p;
    }


    T fromValue(const ossia::value& v) const
    {
      return ossia::convert<T>(v);
    }
    ossia::value toValue(T v) const { return ossia::value{std::move(v)}; }

    template<typename U>
    static auto make_widget(const U& slider, Process::ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
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

      QObject::connect(&inlet, &Process::ControlInlet::valueChanged,
                       sl, [=] (const ossia::value& val) {
        set_index(val);
      });

      return sl;
    }

    template<typename U>
    static QGraphicsItem* make_item(const U& slider, Process::ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
    {
      const auto& values = slider.getValues();
      std::array<const char*, N> arr;
      for(std::size_t i = 0; i < N; i++)
        arr[i] = values[i].first;

      auto sl = new score::QGraphicsComboSlider{arr, nullptr};
      sl->setRect({0., 0., 150., 15.});

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

      QObject::connect(sl, &score::QGraphicsComboSlider::sliderMoved,
                       context, [values,sl,&inlet,&ctx] {
        sl->moving = true;
        ctx.dispatcher.submitCommand<SetControlValue>(inlet, values[sl->value()].second);
      });
      QObject::connect(sl, &score::QGraphicsComboSlider::sliderReleased,
                       context, [sl,&ctx] {
        ctx.dispatcher.commit();
        sl->moving = false;
      });

      QObject::connect(&inlet, &Process::ControlInlet::valueChanged,
                       sl, [=] (const ossia::value& val) {
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
    static const constexpr bool must_validate = true;
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

    static const auto& toStd(const char* const& s) { return s; }
    static const auto& toStd(const std::string& s) { return s; }
    static auto toStd(const QString& s) { return s.toStdString(); }

    static const auto& convert(const std::string& str, const char*)
    { return str; }
    static auto convert(const std::string& str, const QString&)
    { return QString::fromStdString(str); }


    optional<std::string> fromValue(const ossia::value& v) const
    {
      auto t = v.target<std::string>();
      if(t)
      {
        const auto& val = convert(*t, typename ArrT::value_type{});
        if(auto it = ossia::find(values, val); it != values.end())
        {
          return *t;
        }
      }
      return {};
    }
    ossia::value toValue(std::string v) const { return ossia::value{std::move(v)}; }

    auto create_inlet(Id<Process::Port> id, QObject* parent) const
    {
      auto p = new Process::ControlInlet(id, parent);
      p->type = Process::PortType::Message;
      p->setValue(std::string(values[init]));
      p->setCustomData(name);
      return p;
    }

    template<typename T>
    static auto make_widget(const T& slider, Process::ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
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

      QObject::connect(&inlet, &Process::ControlInlet::valueChanged,
                       sl, [=] (const ossia::value& val) {
        set_index(val);
      });

      return sl;
    }

    template<typename T>
    static QGraphicsItem* make_item(const T& slider, Process::ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
    {
      const auto& values = slider.getValues();
      using val_t = std::remove_reference_t<decltype(values[0])>;
      auto sl = new score::QGraphicsComboSlider{values, nullptr};
      sl->setRect({0., 0., 150., 15.});

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

      QObject::connect(sl, &score::QGraphicsComboSlider::sliderMoved,
                       context, [values,sl,&inlet,&ctx] {
        sl->moving = true;
        ctx.dispatcher.submitCommand<SetControlValue>(inlet, toStd(values[sl->value()]));
      });
      QObject::connect(sl, &score::QGraphicsComboSlider::sliderReleased,
                       context, [sl,&ctx] {
        ctx.dispatcher.commit();
        sl->moving = false;
      });

      QObject::connect(&inlet, &Process::ControlInlet::valueChanged,
                       sl, [=] (const ossia::value& val) {
        if(sl->moving)
          return;

        set_index(val);
      });

      return sl;
    }
};

struct TimeSignatureChooser: ControlInfo
{
    static const constexpr bool must_validate = true;
    using type = time_signature;
    const std::string_view init;
    template<std::size_t M>
    constexpr TimeSignatureChooser(const char (&name)[M], std::string_view in):
      ControlInfo{name}, init{in}
    {
    }

    ossia::value toValue(time_signature v) const
    {
      std::string s; s.reserve(8);
      s += std::to_string(v.first);
      s += '/';
      s += std::to_string(v.second);
      return ossia::value{std::move(s)};
    }

    optional<time_signature> fromValue(const ossia::value& v) const
    {
      if(auto str = v.target<std::string>())
      {
        return get_time_signature(*str);
      }
      return ossia::none;
    }
    auto create_inlet(Id<Process::Port> id, QObject* parent) const
    {
      auto p = new Process::ControlInlet(id, parent);
      p->type = Process::PortType::Message;
      p->setValue(std::string{init});
      p->setCustomData(name);
      return p;
    }

    template<typename U>
    static auto make_widget(const U& slider, Process::ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
    {
      auto sl = new QLineEdit;
      struct TimeSignatureValidator final : public QValidator
      {
          State validate(QString& str, int &) const override
          {
            auto p = get_time_signature(str.toStdString());
            if(!p)
              return State::Invalid;

            return State::Acceptable;
          }
      };

      sl->setValidator(new TimeSignatureValidator);
      sl->setContentsMargins(0, 0, 0, 0);

      auto set_text = [sl] (const ossia::value& val)
      {
        const auto& vptr = val.target<std::string>();
        if(!vptr)
          return;
        if(!get_time_signature(*vptr))
          return;

        sl->setText(QString::fromStdString(*vptr));
      };
      set_text(inlet.value());

      QObject::connect(sl, &QLineEdit::editingFinished,
                       context, [sl,&inlet,&ctx] {
        CommandDispatcher<>{ctx.commandStack}.submitCommand<SetControlValue>(inlet, sl->text().toStdString());
      });

      QObject::connect(&inlet, &Process::ControlInlet::valueChanged,
                       sl, [=] (const ossia::value& val) {
        set_text(val);
      });

      return sl;
    }

    template<typename U>
    static QGraphicsItem* make_item(const U& slider, Process::ControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
    {
      return wrapWidget(make_widget(slider, inlet, ctx, parent, context));
    }

};

template<typename T1, typename T2>
constexpr auto make_enum(const T1& t1, std::size_t s, const T2& t2)
{
  return Control::Enum<T2>(t1, s, t2);
}
/*
template<std::size_t N1, std::size_t N2>
Enum(const char (&name)[N1], std::size_t i, const std::array<const char*, N2>& v) -> Enum<std::array<const char*, N2>>;
*/
}
