#include <Process/Dataflow/WidgetInlets.hpp>

#include <score/plugins/SerializableHelpers.hpp>

#include <ossia/dataflow/port.hpp>
namespace Process
{
Enum::Enum(DataStream::Deserializer& vis, QObject* parent)
    : ControlInlet{vis, parent}
{
  vis.writeTo(*this);
}
Enum::Enum(
    const std::vector<std::string>& dom, std::vector<QString> pixmaps, std::string init,
    const QString& name, Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
    , pixmaps{std::move(pixmaps)}
{
  for(auto& val : dom)
    values.push_back(QString::fromStdString(val));

  hidden = true;
  setValue(init);
  setDomain(State::Domain{ossia::domain_base<std::string>{dom}});
  setName(name);
}

Enum::Enum(
    const QStringList& values, std::vector<QString> pixmaps, std::string init,
    const QString& name, Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
    , values{values.begin(), values.end()}
    , pixmaps{std::move(pixmaps)}
{
  hidden = true;
  setValue(init);
  ossia::domain_base<std::string> dom;
  for(auto& val : values)
    dom.values.push_back(val.toStdString());
  setDomain(State::Domain{dom});
  setName(name);
}

Enum::~Enum() { }

Enum::Enum(JSONObject::Deserializer& vis, QObject* parent)
    : ControlInlet{vis, parent}
{
  vis.writeTo(*this);
}
Enum::Enum(DataStream::Deserializer&& vis, QObject* parent)
    : ControlInlet{vis, parent}
{
  vis.writeTo(*this);
}
Enum::Enum(JSONObject::Deserializer&& vis, QObject* parent)
    : ControlInlet{vis, parent}
{
  vis.writeTo(*this);
}

void Enum::setupExecution(ossia::inlet& inl) const noexcept
{
  auto& port = **safe_cast<ossia::value_inlet*>(&inl);
  port.type = ossia::val_type::STRING;
  port.domain = domain().get();
}

ComboBox::ComboBox(DataStream::Deserializer& vis, QObject* parent)
    : ControlInlet{vis, parent}
{
  vis.writeTo(*this);
}
ComboBox::ComboBox(
    std::vector<std::pair<QString, ossia::value>> values, ossia::value init,
    const QString& name, Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
    , alternatives{std::move(values)}
{
  hidden = true;
  setValue(init);
  std::vector<ossia::value> vals;
  for(auto& v : alternatives)
    vals.push_back(v.second);
  setDomain(State::Domain{ossia::make_domain(vals)});
  setName(name);
}

void ComboBox::setupExecution(ossia::inlet& inl) const noexcept
{
  auto& port = **safe_cast<ossia::value_inlet*>(&inl);
  port.domain = domain().get();
}

ComboBox::~ComboBox() { }

ComboBox::ComboBox(JSONObject::Deserializer& vis, QObject* parent)
    : ControlInlet{vis, parent}
{
  vis.writeTo(*this);
}
ComboBox::ComboBox(DataStream::Deserializer&& vis, QObject* parent)
    : ControlInlet{vis, parent}
{
  vis.writeTo(*this);
}
ComboBox::ComboBox(JSONObject::Deserializer&& vis, QObject* parent)
    : ControlInlet{vis, parent}
{
  vis.writeTo(*this);
}

HSVSlider::HSVSlider(
    ossia::vec4f init, const QString& name, Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  setValue(init);
  setName(name);
}

HSVSlider::~HSVSlider() { }

void HSVSlider::setupExecution(ossia::inlet& i) const noexcept
{
  safe_cast<ossia::value_inlet*>(&i)->data.type = ossia::rgba_u{};
}

FloatSlider::FloatSlider(
    float min, float max, float init, const QString& name, Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  setValue(init);
  setDomain(ossia::make_domain(min, max));
  setName(name);
}

FloatSlider::~FloatSlider() { }

void FloatSlider::setupExecution(ossia::inlet& inl) const noexcept
{
  auto& port = **safe_cast<ossia::value_inlet*>(&inl);
  port.type = ossia::val_type::FLOAT;
  port.domain = domain().get();
}

FloatKnob::FloatKnob(
    float min, float max, float init, const QString& name, Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  setValue(init);
  setDomain(ossia::make_domain(min, max));
  setName(name);
}

FloatKnob::~FloatKnob() { }

void FloatKnob::setupExecution(ossia::inlet& inl) const noexcept
{
  auto& port = **safe_cast<ossia::value_inlet*>(&inl);
  port.type = ossia::val_type::FLOAT;
  port.domain = domain().get();
}

LogFloatSlider::LogFloatSlider(
    float min, float max, float init, const QString& name, Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  setValue(init);
  setDomain(ossia::make_domain(min, max));
  setName(name);
}

LogFloatSlider::~LogFloatSlider() { }

void LogFloatSlider::setupExecution(ossia::inlet& inl) const noexcept
{
  auto& port = **safe_cast<ossia::value_inlet*>(&inl);
  port.type = ossia::val_type::FLOAT;
  port.domain = domain().get();
}

IntSlider::IntSlider(
    int min, int max, int init, const QString& name, Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  setValue(init);
  setDomain(ossia::make_domain(min, max));
  setName(name);
}

IntSlider::~IntSlider() { }

void IntSlider::setupExecution(ossia::inlet& inl) const noexcept
{
  auto& port = **safe_cast<ossia::value_inlet*>(&inl);
  port.type = ossia::val_type::INT;
  port.domain = domain().get();
}

IntSpinBox::IntSpinBox(
    int min, int max, int init, const QString& name, Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  setValue(init);
  setDomain(ossia::make_domain(min, max));
  setName(name);
}

void IntSpinBox::setupExecution(ossia::inlet& inl) const noexcept
{
  auto& port = **safe_cast<ossia::value_inlet*>(&inl);
  port.type = ossia::val_type::INT;
  port.domain = domain().get();
}

IntSpinBox::~IntSpinBox() { }

Toggle::Toggle(bool init, const QString& name, Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  setValue(init);
  setDomain(State::Domain{ossia::domain_base<bool>{}});
  setName(name);
}

void Toggle::setupExecution(ossia::inlet& inl) const noexcept
{
  auto& port = **safe_cast<ossia::value_inlet*>(&inl);
  port.type = ossia::val_type::BOOL;
  port.domain = domain().get();
}

Toggle::~Toggle() { }

ChooserToggle::ChooserToggle(
    QStringList alternatives, bool init, const QString& name, Id<Port> id,
    QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  setValue(init ? alternatives[1].toStdString() : alternatives[0].toStdString());
  setDomain(State::Domain{ossia::domain_base<std::string>{
      {alternatives[0].toStdString(), alternatives[1].toStdString()}}});
  setName(name);
}

void ChooserToggle::setupExecution(ossia::inlet& inl) const noexcept
{
  auto& port = **safe_cast<ossia::value_inlet*>(&inl);
  port.type = ossia::val_type::STRING;
  port.domain = domain().get();
}

ChooserToggle::~ChooserToggle() { }

QStringList ChooserToggle::alternatives() const noexcept
{
  const auto& dom = *this->domain().get().v.target<ossia::domain_base<std::string>>();
  auto it = dom.values.begin();
  auto& s1 = *it;
  auto& s2 = *(++it);
  return {QString::fromStdString(s1), QString::fromStdString(s2)};
}

LineEdit::LineEdit(QString init, const QString& name, Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  setValue(init.toStdString());
  setName(name);
}

void LineEdit::setupExecution(ossia::inlet& inl) const noexcept
{
  auto& port = **safe_cast<ossia::value_inlet*>(&inl);
  port.type = ossia::val_type::STRING;
  port.domain = domain().get();
}

LineEdit::~LineEdit() { }

Button::Button(const QString& name, Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  setValue(false);
  setDomain(State::Domain{ossia::domain_base<bool>{}});
  setName(name);
}

void Button::setupExecution(ossia::inlet& inl) const noexcept
{
  auto& port = **safe_cast<ossia::value_inlet*>(&inl);
  port.type = ossia::val_type::BOOL;
  port.domain = domain().get();
}

Button::~Button() { }

ImpulseButton::ImpulseButton(const QString& name, Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  setValue(ossia::impulse{});
  setDomain(State::Domain{ossia::domain_base<ossia::impulse>{}});
  setName(name);
}

void ImpulseButton::setupExecution(ossia::inlet& inl) const noexcept
{
  auto& port = **safe_cast<ossia::value_inlet*>(&inl);
  port.type = ossia::val_type::IMPULSE;
  port.domain = domain().get();
}

ImpulseButton::~ImpulseButton() { }

XYSlider::XYSlider(ossia::vec2f init, const QString& name, Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  setValue(init);
  setName(name);
  setDomain(ossia::make_domain(ossia::vec2f{0., 0.}, ossia::vec2f{1., 1.}));
}

XYSlider::XYSlider(
    ossia::vec2f min, ossia::vec2f max, ossia::vec2f init, const QString& name,
    Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  setValue(init);
  setName(name);
  setDomain(ossia::make_domain(min, max));
}

void XYSlider::setupExecution(ossia::inlet& inl) const noexcept
{
  auto& port = **safe_cast<ossia::value_inlet*>(&inl);
  port.type = ossia::val_type::VEC2F;
  port.domain = domain().get();
}

XYSlider::~XYSlider() { }

XYZSlider::XYZSlider(
    ossia::vec3f init, const QString& name, Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  setValue(init);
  setName(name);
  setDomain(ossia::make_domain(ossia::vec3f{0., 0., 0.}, ossia::vec3f{1., 1., 1.}));
}

XYZSlider::XYZSlider(
    ossia::vec3f min, ossia::vec3f max, ossia::vec3f init, const QString& name,
    Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  setValue(init);
  setName(name);
  setDomain(ossia::make_domain(min, max));
}

XYZSlider::~XYZSlider() { }

void XYZSlider::setupExecution(ossia::inlet& inl) const noexcept
{
  auto& port = **safe_cast<ossia::value_inlet*>(&inl);
  port.type = ossia::val_type::VEC3F;
  port.domain = domain().get();
}

MultiSlider::MultiSlider(
    ossia::value init, const QString& name, Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  setValue(std::move(init));
  setName(name);
  setDomain(ossia::make_domain(0., 1.));
}

MultiSlider::~MultiSlider() { }

ossia::value MultiSlider::getMin() const noexcept
{
  return domain().get().get_min();
}

ossia::value MultiSlider::getMax() const noexcept
{
  return domain().get().get_max();
}

void MultiSlider::setupExecution(ossia::inlet& inl) const noexcept
{
  auto& port = **safe_cast<ossia::value_inlet*>(&inl);
  port.type = ossia::val_type::LIST;
  port.domain = domain().get();
}

Bargraph::Bargraph(
    float min, float max, float init, const QString& name, Id<Port> id, QObject* parent)
    : ControlOutlet{id, parent}
{
  hidden = true;
  setValue(init);
  setDomain(ossia::make_domain(min, max));
  setName(name);
}

Bargraph::~Bargraph() { }

}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::FloatSlider>(const Process::FloatSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::FloatSlider>(Process::FloatSlider& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read<Process::FloatSlider>(const Process::FloatSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write<Process::FloatSlider>(Process::FloatSlider& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::FloatKnob>(const Process::FloatKnob& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::FloatKnob>(Process::FloatKnob& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read<Process::FloatKnob>(const Process::FloatKnob& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write<Process::FloatKnob>(Process::FloatKnob& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::LogFloatSlider>(const Process::LogFloatSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::LogFloatSlider>(Process::LogFloatSlider& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read<Process::LogFloatSlider>(const Process::LogFloatSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write<Process::LogFloatSlider>(Process::LogFloatSlider& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::IntSlider>(const Process::IntSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::IntSlider>(Process::IntSlider& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read<Process::IntSlider>(const Process::IntSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write<Process::IntSlider>(Process::IntSlider& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::IntSpinBox>(const Process::IntSpinBox& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::IntSpinBox>(Process::IntSpinBox& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read<Process::IntSpinBox>(const Process::IntSpinBox& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write<Process::IntSpinBox>(Process::IntSpinBox& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::Toggle>(const Process::Toggle& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::Toggle>(Process::Toggle& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONReader::read<Process::Toggle>(const Process::Toggle& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write<Process::Toggle>(Process::Toggle& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::ChooserToggle>(const Process::ChooserToggle& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::ChooserToggle>(Process::ChooserToggle& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read<Process::ChooserToggle>(const Process::ChooserToggle& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write<Process::ChooserToggle>(Process::ChooserToggle& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::LineEdit>(const Process::LineEdit& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::LineEdit>(Process::LineEdit& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read<Process::LineEdit>(const Process::LineEdit& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write<Process::LineEdit>(Process::LineEdit& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::Enum>(const Process::Enum& p)
{
  read((const Process::ControlInlet&)p);
  m_stream << p.values << p.pixmaps;
}
template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write<Process::Enum>(Process::Enum& p)
{
  m_stream >> p.values >> p.pixmaps;
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONReader::read<Process::Enum>(const Process::Enum& p)
{
  read((const Process::ControlInlet&)p);
  obj["Values"] = p.values;
  obj["Pixmaps"] = p.pixmaps;
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write<Process::Enum>(Process::Enum& p)
{
  p.values <<= obj["Values"];
  p.pixmaps <<= obj["Pixmaps"];
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::ComboBox>(const Process::ComboBox& p)
{
  read((const Process::ControlInlet&)p);
  m_stream << p.alternatives;
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::ComboBox>(Process::ComboBox& p)
{
  m_stream >> p.alternatives;
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read<Process::ComboBox>(const Process::ComboBox& p)
{
  read((const Process::ControlInlet&)p);
  obj["Values"] = p.alternatives;
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write<Process::ComboBox>(Process::ComboBox& p)
{
  p.alternatives <<= obj["Values"];
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::Button>(const Process::Button& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::Button>(Process::Button& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONReader::read<Process::Button>(const Process::Button& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write<Process::Button>(Process::Button& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::ImpulseButton>(const Process::ImpulseButton& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::ImpulseButton>(Process::ImpulseButton& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read<Process::ImpulseButton>(const Process::ImpulseButton& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write<Process::ImpulseButton>(Process::ImpulseButton& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::HSVSlider>(const Process::HSVSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::HSVSlider>(Process::HSVSlider& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read<Process::HSVSlider>(const Process::HSVSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write<Process::HSVSlider>(Process::HSVSlider& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::XYSlider>(const Process::XYSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::XYSlider>(Process::XYSlider& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read<Process::XYSlider>(const Process::XYSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write<Process::XYSlider>(Process::XYSlider& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::XYZSlider>(const Process::XYZSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::XYZSlider>(Process::XYZSlider& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read<Process::XYZSlider>(const Process::XYZSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write<Process::XYZSlider>(Process::XYZSlider& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::MultiSlider>(const Process::MultiSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::MultiSlider>(Process::MultiSlider& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read<Process::MultiSlider>(const Process::MultiSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write<Process::MultiSlider>(Process::MultiSlider& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::Bargraph>(const Process::Bargraph& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::Bargraph>(Process::Bargraph& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read<Process::Bargraph>(const Process::Bargraph& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write<Process::Bargraph>(Process::Bargraph& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read<QString>(const QString& p)
{
  m_stream << p;
}
template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write<QString>(QString& p)
{
  m_stream >> p;
}
