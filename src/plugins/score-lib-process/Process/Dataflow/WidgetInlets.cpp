#include <Process/Dataflow/WidgetInlets.hpp>

#include <score/plugins/SerializableHelpers.hpp>
#include <score/tools/FileWatch.hpp>

#include <ossia/dataflow/port.hpp>

#include <ossia-qt/invoke.hpp>

#include <QCoreApplication>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Process::FileChooserBase)
W_OBJECT_IMPL(Process::FileChooser)
W_OBJECT_IMPL(Process::AudioFileChooser)
W_OBJECT_IMPL(Process::VideoFileChooser)
W_OBJECT_IMPL(Process::ImpulseButton)
namespace Process
{
Enum::Enum(
    const std::vector<std::string>& dom, std::vector<QString> pixmaps, std::string init,
    const QString& name, Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
    , pixmaps{std::move(pixmaps)}
{
  for(auto& val : dom)
    values.push_back(QString::fromStdString(val));

  hidden = true;
  setInit(init);
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
  setInit(init);
  setValue(init);
  ossia::domain_base<std::string> dom;
  for(auto& val : values)
    dom.values.push_back(val.toStdString());
  setDomain(State::Domain{dom});
  setName(name);
}

Enum::~Enum() { }

Enum::Enum(DataStream::Deserializer& vis, QObject* parent)
    : ControlInlet{vis, parent}
{
  vis.writeTo(*this);
}
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
  setInit(init);
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
  setInit(init);
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
  setInit(init);
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
  setInit(init);
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
  setInit(init);
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
  setInit(init);
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

IntRangeSlider::IntRangeSlider(
    int min, int max, ossia::vec2f init, const QString& name, Id<Port> id,
    QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  setInit(init);
  setValue(init);
  setDomain(ossia::make_domain(min, max));
  setName(name);
}

IntRangeSlider::~IntRangeSlider() { }

void IntRangeSlider::setupExecution(ossia::inlet& inl) const noexcept
{
  auto& port = **safe_cast<ossia::value_inlet*>(&inl);
  port.type = ossia::val_type::VEC2F;
  port.domain = domain().get();
}

FloatRangeSlider::FloatRangeSlider(
    float min, float max, ossia::vec2f init, const QString& name, Id<Port> id,
    QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  setInit(init);
  setValue(init);
  setDomain(ossia::make_domain(min, max));
  setName(name);
}

FloatRangeSlider::~FloatRangeSlider() { }

void FloatRangeSlider::setupExecution(ossia::inlet& inl) const noexcept
{
  auto& port = **safe_cast<ossia::value_inlet*>(&inl);
  port.type = ossia::val_type::VEC2F;
  port.domain = domain().get();
}

IntRangeSpinBox::~IntRangeSpinBox() { }

void IntRangeSpinBox::setupExecution(ossia::inlet& inl) const noexcept
{
  auto& port = **safe_cast<ossia::value_inlet*>(&inl);
  port.type = ossia::val_type::VEC2F;
  port.domain = domain().get();
}

FloatRangeSpinBox::FloatRangeSpinBox(
    float min, float max, ossia::vec2f init, const QString& name, Id<Port> id,
    QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  setInit(init);
  setValue(init);
  setDomain(ossia::make_domain(min, max));
  setName(name);
}

FloatRangeSpinBox::~FloatRangeSpinBox() { }

void FloatRangeSpinBox::setupExecution(ossia::inlet& inl) const noexcept
{
  auto& port = **safe_cast<ossia::value_inlet*>(&inl);
  port.type = ossia::val_type::VEC2F;
  port.domain = domain().get();
}

IntSpinBox::IntSpinBox(
    int min, int max, int init, const QString& name, Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  setInit(init);
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

FloatSpinBox::FloatSpinBox(
    float min, float max, float init, const QString& name, Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  setInit(init);
  setValue(init);
  setDomain(ossia::make_domain(min, max));
  setName(name);
}

void FloatSpinBox::setupExecution(ossia::inlet& inl) const noexcept
{
  auto& port = **safe_cast<ossia::value_inlet*>(&inl);
  port.type = ossia::val_type::FLOAT;
  port.domain = domain().get();
}

FloatSpinBox::~FloatSpinBox() { }

TimeChooser::TimeChooser(
    float min, float max, float init, const QString& name, Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  setValue(ossia::vec2f{init, 1.f});
  setInit(value());
  setDomain(ossia::make_domain(ossia::vec2f{min, 0.f}, ossia::vec2f{max, 1.f}));
  setName(name);
}

TimeChooser::~TimeChooser() { }

void TimeChooser::setupExecution(ossia::inlet& inl) const noexcept
{
  auto& port = **safe_cast<ossia::value_inlet*>(&inl);
  port.type = ossia::val_type::VEC2F;
  // port.type = ossia::second_u{};
  // port.domain = domain().get();
}

Toggle::Toggle(bool init, const QString& name, Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  setInit(init);
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
  setInit(value());
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
  setInit(value());
  setName(name);
}

void LineEdit::setupExecution(ossia::inlet& inl) const noexcept
{
  auto& port = **safe_cast<ossia::value_inlet*>(&inl);
  port.type = ossia::val_type::STRING;
  port.domain = domain().get();
}

LineEdit::~LineEdit() { }

ProgramEdit::ProgramEdit(QString init, const QString& name, Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  setValue(init.toStdString());
  setInit(value());
  setName(name);
}

void ProgramEdit::setupExecution(ossia::inlet& inl) const noexcept
{
  auto& port = **safe_cast<ossia::value_inlet*>(&inl);
  port.type = ossia::val_type::STRING;
  port.domain = domain().get();
}

ProgramEdit::~ProgramEdit() { }

FileChooserBase::FileChooserBase(
    QString init, QString filters, const QString& name, Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  setValue(init.toStdString());
  setInit(value());
  setName(name);
  m_filters = filters;
}

void FileChooserBase::setupExecution(ossia::inlet& inl) const noexcept
{
  auto& port = **safe_cast<ossia::value_inlet*>(&inl);
  port.type = ossia::val_type::STRING;
  port.domain = domain().get();
}

void FileChooserBase::enableFileWatch()
{
  auto fun = std::make_shared<std::function<void()>>([ptr = QPointer{this}]() mutable {
    ossia::qt::run_async(qApp, [ptr] {
      if(ptr)
      {
        // This will trigger a reload
        // FIXME make it work with execution value changes
        ptr->valueChanged(ptr->value());
      }
    });
  });

  auto& f = score::FileWatch::instance();
  f.add(QString::fromStdString(ossia::convert<std::string>(this->value())), fun);

  connect(
      this, &Process::FileChooser::valueChanged, this,
      [cur_s = ossia::convert<std::string>(this->value()),
       fun](const ossia::value& v) mutable {
    auto new_s = ossia::convert<std::string>(v);
    if(new_s != cur_s)
    {
      auto& f = score::FileWatch::instance();
      f.remove(QString::fromStdString(cur_s), fun);
      f.add(QString::fromStdString(new_s), fun);
      cur_s = new_s;
    }
      });

  connect(this, &Process::FileChooser::destroying, this, [this, fun] {
    auto& f = score::FileWatch::instance();
    f.remove(QString::fromStdString(ossia::convert<std::string>(this->value())), fun);
  });
}

FileChooserBase::~FileChooserBase()
{
  destroying();
}

FileChooser::FileChooser(
    QString init, QString filters, const QString& name, Id<Port> id, QObject* parent)
    : FileChooserBase{init, filters, name, id, parent}
{
}

FileChooser::~FileChooser() { }

AudioFileChooser::AudioFileChooser(
    QString init, QString filters, const QString& name, Id<Port> id, QObject* parent)
    : FileChooserBase{init, filters, name, id, parent}
{
}

AudioFileChooser::~AudioFileChooser() { }

static QString toFilters(const QSet<QString>& exts)
{
  QString res;
  for(const auto& s : exts)
  {
    res += "*.";
    res += s;
    res += " ";
  }
  if(!res.isEmpty())
    res.resize(res.size() - 1);
  return res;
}

static QString videoFilesTypes()
{
  // FIXME refactor supported formats with Video process
  QSet<QString> files = {"mkv",  "mov", "mp4", "h264", "avi",  "hap", "mpg",
                         "mpeg", "imf", "mxf", "mts",  "m2ts", "mj2", "webm"};
  return QString{"Videos (%1)"}.arg(toFilters(files));
}

VideoFileChooser::VideoFileChooser(
    QString init, QString filters, const QString& name, Id<Port> id, QObject* parent)
    : FileChooserBase{init, videoFilesTypes(), name, id, parent}
{
}

VideoFileChooser::~VideoFileChooser() { }

Button::Button(const QString& name, Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  setValue(false);
  setInit(false);
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
  setInit(ossia::impulse{});
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
  setInit(init);
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
  setInit(init);
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
  setInit(init);
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
  setInit(init);
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

XYZSpinboxes::XYZSpinboxes(
    ossia::vec3f init, const QString& name, Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  setValue(init);
  setInit(init);
  setName(name);
  setDomain(ossia::make_domain(ossia::vec3f{0., 0., 0.}, ossia::vec3f{1., 1., 1.}));
}

XYSpinboxes::XYSpinboxes(
    ossia::vec2f min, ossia::vec2f max, ossia::vec2f init, bool integral,
    const QString& name, Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
    , integral{integral}
{
  hidden = true;
  setValue(init);
  setInit(init);
  setName(name);
  setDomain(ossia::make_domain(min, max));
}

XYSpinboxes::~XYSpinboxes() { }

void XYSpinboxes::setupExecution(ossia::inlet& inl) const noexcept
{
  auto& port = **safe_cast<ossia::value_inlet*>(&inl);
  port.type = ossia::val_type::VEC2F;
  port.domain = domain().get();
}

XYZSpinboxes::XYZSpinboxes(
    ossia::vec3f min, ossia::vec3f max, ossia::vec3f init, const QString& name,
    Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  setValue(init);
  setInit(init);
  setName(name);
  setDomain(ossia::make_domain(min, max));
}

XYZSpinboxes::~XYZSpinboxes() { }

void XYZSpinboxes::setupExecution(ossia::inlet& inl) const noexcept
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
  setInit(value());
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



MultiSliderXY::MultiSliderXY(
    ossia::value init, const QString& name, Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  setValue(std::move(init));
  setInit(value());
  setName(name);
  setDomain(ossia::make_domain(0., 1.));
}

MultiSliderXY::~MultiSliderXY() { }

ossia::value MultiSliderXY::getMin() const noexcept
{
  return domain().get().get_min();
}

ossia::value MultiSliderXY::getMax() const noexcept
{
  return domain().get().get_max();
}

void MultiSliderXY::setupExecution(ossia::inlet& inl) const noexcept
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
DataStreamReader::read(const Process::FloatSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::FloatSlider& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read(const Process::FloatSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write(Process::FloatSlider& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::FloatKnob& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::FloatKnob& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read(const Process::FloatKnob& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write(Process::FloatKnob& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::LogFloatSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::LogFloatSlider& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read(const Process::LogFloatSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write(Process::LogFloatSlider& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::IntSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::IntSlider& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read(const Process::IntSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write(Process::IntSlider& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::IntRangeSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::IntRangeSlider& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read(const Process::IntRangeSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write(Process::IntRangeSlider& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::FloatRangeSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::FloatRangeSlider& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read(const Process::FloatRangeSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write(Process::FloatRangeSlider& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::IntRangeSpinBox& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::IntRangeSpinBox& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read(const Process::IntRangeSpinBox& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write(Process::IntRangeSpinBox& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::FloatRangeSpinBox& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::FloatRangeSpinBox& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read(const Process::FloatRangeSpinBox& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write(Process::FloatRangeSpinBox& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::IntSpinBox& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::IntSpinBox& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read(const Process::IntSpinBox& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write(Process::IntSpinBox& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::FloatSpinBox& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::FloatSpinBox& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read(const Process::FloatSpinBox& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write(Process::FloatSpinBox& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::TimeChooser& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::TimeChooser& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read(const Process::TimeChooser& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write(Process::TimeChooser& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::Toggle& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::Toggle& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONReader::read(const Process::Toggle& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write(Process::Toggle& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::ChooserToggle& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::ChooserToggle& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read(const Process::ChooserToggle& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write(Process::ChooserToggle& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::LineEdit& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::LineEdit& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read(const Process::LineEdit& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write(Process::LineEdit& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::ProgramEdit& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::ProgramEdit& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read(const Process::ProgramEdit& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write(Process::ProgramEdit& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::FileChooser& p)
{
  read((const Process::ControlInlet&)p);
  m_stream << p.filters();
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::FileChooser& p)
{
  QString f;
  m_stream >> f;
  p.setFilters(f);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read(const Process::FileChooser& p)
{
  read((const Process::ControlInlet&)p);
  obj["Filters"] = p.filters();
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write(Process::FileChooser& p)
{
  QString f;
  f <<= obj["Filters"];
  p.setFilters(f);
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::AudioFileChooser& p)
{
  read((const Process::ControlInlet&)p);
  m_stream << p.filters();
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::AudioFileChooser& p)
{
  QString f;
  m_stream >> f;
  p.setFilters(f);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read(const Process::AudioFileChooser& p)
{
  read((const Process::ControlInlet&)p);
  obj["Filters"] = p.filters();
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write(Process::AudioFileChooser& p)
{
  QString f;
  f <<= obj["Filters"];
  p.setFilters(f);
}

template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read(const Process::VideoFileChooser& p)
{
  read((const Process::ControlInlet&)p);
  m_stream << p.filters();
}
template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write(Process::VideoFileChooser& p)
{
  QString f;
  m_stream >> f;
  p.setFilters(f);
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONReader::read(const Process::VideoFileChooser& p)
{
  read((const Process::ControlInlet&)p);
  obj["Filters"] = p.filters();
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write(Process::VideoFileChooser& p)
{
  QString f;
  f <<= obj["Filters"];
  p.setFilters(f);
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::Enum& p)
{
  read((const Process::ControlInlet&)p);
  m_stream << p.values << p.pixmaps;
}
template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write(Process::Enum& p)
{
  m_stream >> p.values >> p.pixmaps;
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONReader::read(const Process::Enum& p)
{
  read((const Process::ControlInlet&)p);
  obj["Values"] = p.values;
  obj["Pixmaps"] = p.pixmaps;
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write(Process::Enum& p)
{
  p.values <<= obj["Values"];
  p.pixmaps <<= obj["Pixmaps"];
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::ComboBox& p)
{
  read((const Process::ControlInlet&)p);
  m_stream << p.alternatives;
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::ComboBox& p)
{
  m_stream >> p.alternatives;
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read(const Process::ComboBox& p)
{
  read((const Process::ControlInlet&)p);
  obj["Values"] = p.alternatives;
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write(Process::ComboBox& p)
{
  p.alternatives <<= obj["Values"];
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::Button& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::Button& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONReader::read(const Process::Button& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write(Process::Button& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::ImpulseButton& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::ImpulseButton& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read(const Process::ImpulseButton& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write(Process::ImpulseButton& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::HSVSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::HSVSlider& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read(const Process::HSVSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write(Process::HSVSlider& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::XYSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::XYSlider& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read(const Process::XYSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write(Process::XYSlider& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::XYZSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::XYZSlider& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read(const Process::XYZSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write(Process::XYZSlider& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::XYSpinboxes& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::XYSpinboxes& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read(const Process::XYSpinboxes& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write(Process::XYSpinboxes& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::XYZSpinboxes& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::XYZSpinboxes& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read(const Process::XYZSpinboxes& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write(Process::XYZSpinboxes& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::MultiSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::MultiSlider& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read(const Process::MultiSlider& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write(Process::MultiSlider& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::MultiSliderXY& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::MultiSliderXY& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read(const Process::MultiSliderXY& p)
{
  read((const Process::ControlInlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write(Process::MultiSliderXY& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::Bargraph& p)
{
  read((const Process::ControlOutlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::Bargraph& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read(const Process::Bargraph& p)
{
  read((const Process::ControlOutlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write(Process::Bargraph& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read(const QString& p)
{
  m_stream << p;
}
template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write(QString& p)
{
  m_stream >> p;
}
