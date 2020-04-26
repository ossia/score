#pragma once
#include <Process/Dataflow/ControlWidgets.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <ossia/network/domain/domain.hpp>

namespace Process
{
struct FloatSlider;
struct LogFloatSlider;
struct IntSlider;
struct IntSpinBox;
struct Toggle;
struct ChooserToggle;
struct ComboBox;
struct LineEdit;
struct Enum;
struct TimeSignatureChooser;
struct Button;
struct HSVSlider;
struct XYSlider;
}
UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT,
    Process::Port,
    Process::FloatSlider,
    "af2b4fc3-aecb-4c15-a5aa-1c573a239925")
UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT,
    Process::Port,
    Process::LogFloatSlider,
    "5554eb67-bcc8-45ab-8ec2-37a3f191aa64")

UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT,
    Process::Port,
    Process::IntSlider,
    "348b80a4-45dc-4f70-8f5f-6546c85089a2")

UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT,
    Process::Port,
    Process::IntSpinBox,
    "238399a0-7e81-47e3-896f-08e8856e2973")

UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT,
    Process::Port,
    Process::Toggle,
    "fb27e4cb-ea7f-41e2-ad92-2354498c1b6b")

UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT,
    Process::Port,
    Process::ChooserToggle,
    "27d488b6-784b-4bfc-8e7f-e28ef030c248")

UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT,
    Process::Port,
    Process::LineEdit,
    "9ae797ea-d94c-4792-acec-9ec1932bae5d")

UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT,
    Process::Port,
    Process::Enum,
    "8b1d76c4-3838-4ac0-9b9c-c12bc5db8e8a")

UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT,
    Process::Port,
    Process::ComboBox,
    "485680cc-b8b9-4a01-acc7-3e8334bdc016")

UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT,
    Process::Port,
    Process::TimeSignatureChooser,
    "91970a5f-aab8-4993-8410-4aff56adf7dc")

UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT,
    Process::Port,
    Process::Button,
    "feb87e84-e0d2-428f-96ff-a123ac964f59")

UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT,
    Process::Port,
    Process::HSVSlider,
    "8f38638e-9f9f-48b0-ae36-1cba86ef5703")
UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT,
    Process::Port,
    Process::XYSlider,
    "8093743c-584f-4bb9-97d4-6c7602f87116")

namespace Process
{
struct FloatSlider : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(FloatSlider)
  using control_type = WidgetFactory::FloatSlider;
  FloatSlider(
      float min,
      float max,
      float init,
      const QString& name,
      Id<Process::Port> id,
      QObject* parent)
      : ControlInlet{id, parent}
  {
    hidden = true;
    setValue(init);
    setDomain(ossia::make_domain(min, max));
    setCustomData(name);
  }

  auto getMin() const noexcept { return domain().get().convert_min<float>(); }
  auto getMax() const noexcept { return domain().get().convert_max<float>(); }
  using Process::ControlInlet::ControlInlet;
};

struct LogFloatSlider : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(LogFloatSlider)
  using control_type = WidgetFactory::LogFloatSlider;
  LogFloatSlider(
      float min,
      float max,
      float init,
      const QString& name,
      Id<Process::Port> id,
      QObject* parent)
      : ControlInlet{id, parent}
  {
    hidden = true;
    setValue(init);
    setDomain(ossia::make_domain(min, max));
    setCustomData(name);
  }

  auto getMin() const noexcept { return domain().get().convert_min<float>(); }
  auto getMax() const noexcept { return domain().get().convert_max<float>(); }
  using Process::ControlInlet::ControlInlet;
};

struct IntSlider : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(IntSlider)
  using control_type = WidgetFactory::IntSlider;
  IntSlider(
      int min,
      int max,
      int init,
      const QString& name,
      Id<Process::Port> id,
      QObject* parent)
      : ControlInlet{id, parent}
  {
    hidden = true;
    setValue(init);
    setDomain(ossia::make_domain(min, max));
    setCustomData(name);
  }

  auto getMin() const noexcept { return domain().get().convert_min<int>(); }
  auto getMax() const noexcept { return domain().get().convert_max<int>(); }
  using Process::ControlInlet::ControlInlet;
};

struct IntSpinBox : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(IntSpinBox)
  using control_type = WidgetFactory::IntSpinBox;
  IntSpinBox(
      int min,
      int max,
      int init,
      const QString& name,
      Id<Process::Port> id,
      QObject* parent)
      : ControlInlet{id, parent}
  {
    hidden = true;
    setValue(init);
    setDomain(ossia::make_domain(min, max));
    setCustomData(name);
  }

  auto getMin() const noexcept { return domain().get().convert_min<int>(); }
  auto getMax() const noexcept { return domain().get().convert_max<int>(); }
  using Process::ControlInlet::ControlInlet;
};

struct Toggle : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(Toggle)
  using control_type = WidgetFactory::Toggle;
  Toggle(bool init, const QString& name, Id<Process::Port> id, QObject* parent)
      : ControlInlet{id, parent}
  {
    hidden = true;
    setValue(init);
    setDomain(State::Domain{ossia::domain_base<bool>{}});
    setCustomData(name);
  }

  using Process::ControlInlet::ControlInlet;
};

struct ChooserToggle : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(ChooserToggle)
  using control_type = WidgetFactory::ChooserToggle;
  ChooserToggle(
      QStringList alternatives,
      bool init,
      const QString& name,
      Id<Process::Port> id,
      QObject* parent)
      : ControlInlet{id, parent}
  {
    hidden = true;
    setValue(
        init ? alternatives[1].toStdString() : alternatives[0].toStdString());
    setDomain(State::Domain{ossia::domain_base<std::string>{
        {alternatives[0].toStdString(), alternatives[1].toStdString()}}});
    setCustomData(name);
  }

  QStringList alternatives() const noexcept
  {
    const auto& dom = *this->domain().get().v.target<ossia::domain_base<std::string>>();
    auto it = dom.values.begin();
    auto& s1 = *it;
    auto& s2 = *(++it);
    return {QString::fromStdString(s1), QString::fromStdString(s2)};
  }
  using Process::ControlInlet::ControlInlet;
};

struct LineEdit : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(LineEdit)
  using control_type = WidgetFactory::LineEdit;
  LineEdit(
      QString init,
      const QString& name,
      Id<Process::Port> id,
      QObject* parent)
      : ControlInlet{id, parent}
  {
    hidden = true;
    setValue(init.toStdString());
    setCustomData(name);
  }

  using Process::ControlInlet::ControlInlet;
};

struct SCORE_LIB_PROCESS_EXPORT ComboBox : public Process::ControlInlet
{
  using control_type = WidgetFactory::ComboBox;
  MODEL_METADATA_IMPL(ComboBox)
  std::vector<std::pair<QString, ossia::value>> alternatives;
  ComboBox(
      std::vector<std::pair<QString, ossia::value>> values,
      ossia::value init,
      const QString& name,
      Id<Process::Port> id,
      QObject* parent)
      : ControlInlet{id, parent}, alternatives{std::move(values)}
  {
    hidden = true;
    setValue(init);
    std::vector<ossia::value> vals;
    for (auto& v : alternatives)
      vals.push_back(v.second);
    setDomain(State::Domain{ossia::make_domain(vals)});
    setCustomData(name);
  }

  const auto& getValues() const noexcept { return alternatives; }
  auto count() const noexcept { return alternatives.size(); }

  ComboBox(DataStream::Deserializer& vis, QObject* parent);
  ComboBox(JSONObject::Deserializer& vis, QObject* parent);
  ComboBox(DataStream::Deserializer&& vis, QObject* parent);
  ComboBox(JSONObject::Deserializer&& vis, QObject* parent);
};

struct SCORE_LIB_PROCESS_EXPORT Enum : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(Enum)
  using control_type = WidgetFactory::Enum;
  std::vector<QString> values;
  std::vector<QString> pixmaps;
  Enum(
      const ossia::flat_set<std::string>& dom,
      std::vector<QString> pixmaps,
      std::string init,
      const QString& name,
      Id<Process::Port> id,
      QObject* parent)
    : ControlInlet{id, parent}, pixmaps{std::move(pixmaps)}
  {
    for (auto& val : dom)
      values.push_back(QString::fromStdString(val));

    hidden = true;
    setValue(init);
    setDomain(State::Domain{ossia::domain_base<std::string>{dom}});
    setCustomData(name);
  }

  Enum(
      const QStringList& values,
      std::vector<QString> pixmaps,
      std::string init,
      const QString& name,
      Id<Process::Port> id,
      QObject* parent)
    : ControlInlet{id, parent}, values{values.begin(), values.end()}, pixmaps{std::move(pixmaps)}
  {
    hidden = true;
    setValue(init);
    ossia::domain_base<std::string> dom;
    for (auto& val : values)
      dom.values.insert(val.toStdString());
    setDomain(State::Domain{dom});
    setCustomData(name);
  }

  const std::vector<QString>& getValues() const { return values; }

  Enum(DataStream::Deserializer& vis, QObject* parent);
  Enum(JSONObject::Deserializer& vis, QObject* parent);
  Enum(DataStream::Deserializer&& vis, QObject* parent);
  Enum(JSONObject::Deserializer&& vis, QObject* parent);
};

struct TimeSignatureChooser : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(TimeSignatureChooser)
  using control_type = WidgetFactory::TimeSignatureChooser;
  TimeSignatureChooser(
      std::string init,
      const QString& name,
      Id<Process::Port> id,
      QObject* parent)
      : ControlInlet{id, parent}
  {
    using namespace std::literals;
    hidden = true;
    setValue(init);
    setDomain(
        State::Domain{ossia::domain_base<std::string>{{"3/4"s, "4/4"s}}});
    setCustomData(name);
  }

  using Process::ControlInlet::ControlInlet;
};

struct Button : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(Button)
  using control_type = WidgetFactory::Button;
  Button(const QString& name, Id<Process::Port> id, QObject* parent)
      : ControlInlet{id, parent}
  {
    hidden = true;
    setValue(false);
    setDomain(State::Domain{ossia::domain_base<bool>{}});
    setCustomData(name);
  }

  using Process::ControlInlet::ControlInlet;
};


struct HSVSlider : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(HSVSlider)
  using control_type = WidgetFactory::HSVSlider;
  HSVSlider(
      ossia::vec4f init,
      const QString& name,
      Id<Process::Port> id,
      QObject* parent)
      : ControlInlet{id, parent}
  {
    hidden = true;
    setValue(init);
    setCustomData(name);
  }

  auto getMin() const noexcept { return ossia::vec4f{0., 0., 0., 0.}; }
  auto getMax() const noexcept { return ossia::vec4f{1., 1., 1., 1.}; }
  using Process::ControlInlet::ControlInlet;
};

struct XYSlider : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(XYSlider)
  using control_type = WidgetFactory::XYSlider;
  XYSlider(
      ossia::vec2f init,
      const QString& name,
      Id<Process::Port> id,
      QObject* parent)
      : ControlInlet{id, parent}
  {
    hidden = true;
    setValue(init);
    setCustomData(name);
  }

  auto getMin() const noexcept { return ossia::vec4f{0., 0.}; }
  auto getMax() const noexcept { return ossia::vec4f{1., 1.}; }
  using Process::ControlInlet::ControlInlet;
};

}
