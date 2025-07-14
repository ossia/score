// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/CodeWriter.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortSerialization.hpp>
#include <Process/Dataflow/PrettyPortName.hpp>

#include <score/tools/Bind.hpp>

#include <ossia/detail/closest_element.hpp>
#include <ossia/network/common/destination_qualifiers.hpp>
#include <ossia/network/dataspace/color.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>

#include <QColor>

#include <Color/GradientModel.hpp>
#include <Color/GradientPresenter.hpp>
#include <fmt/format.h>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Gradient::ProcessModel)
namespace Gradient
{
ProcessModel::ProcessModel(
    const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent)
    : Process::
        ProcessModel{duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
    , outlet{Process::make_value_outlet(Id<Process::Port>(0), this)}
{
  m_colors.insert(std::make_pair(0.2, QColor(Qt::black)));
  m_colors.insert(std::make_pair(0.8, QColor(Qt::white)));

  State::AddressAccessor addr;
  addr.qualifiers.get().unit = ossia::rgba_u{};
  outlet->setAddress(addr);

  metadata().setInstanceName(*this);
  init();
}

ProcessModel::~ProcessModel() { }

void ProcessModel::init()
{
  outlet->setName("Out");
  auto update_invalid_address = [this](const State::AddressAccessor& addr) {
    State::AddressAccessor copy = addr;
    auto& qual = copy.qualifiers.get();

    bool change = false;
    // Check if it's any color unit
    if(qual.unit.which() != ossia::unit_t{ossia::rgba_u{}}.which())
    {
      qual.unit = ossia::rgba_u{};
      change = true;
    }

    if(!qual.accessors.empty())
    {
      qual.accessors = {};
      change = true;
    }

    if(change)
    {
      outlet->setAddress(std::move(copy));
    }
    prettyNameChanged();
  };
  con(*outlet, &Process::Outlet::addressChanged, this, update_invalid_address);
  connect(outlet.get(), &Process::Port::cablesChanged, this, [this] {
    prettyNameChanged();
  });
  update_invalid_address(outlet->address());
  m_outlets.push_back(outlet.get());
}

QString ProcessModel::prettyName() const noexcept
{
  auto& doc = score::IDocument::documentContext(*this);
  if(auto name = Process::displayNameForPort(*outlet, doc); !name.isEmpty())
  {
    return name;
  }
  return QStringLiteral("Gradient");
}

const ProcessModel::gradient_colors& ProcessModel::gradient() const
{
  return m_colors;
}

void ProcessModel::setGradient(const ProcessModel::gradient_colors& c)
{
  if(m_colors != c)
  {
    m_colors = c;
    gradientChanged();
  }
}

std::unique_ptr<Process::CodeWriter>
ProcessModel::codeWriter(Process::CodeFormat) const noexcept
{
  struct CodeWriter : Process::CodeWriter
  {
    using Process::CodeWriter::CodeWriter;

    static ossia::hunter_lab to_ossia_color(QColor c)
    {
      switch(c.spec())
      {
        case QColor::Rgb: {
          ossia::rgb r{(float)c.redF(), (float)c.greenF(), (float)c.blueF()};
          return ossia::hunter_lab{r};
        }
        case QColor::Hsv:
        case QColor::Cmyk:
        case QColor::Hsl:
          return to_ossia_color(c.toRgb());
        case QColor::Invalid:
        default:
          return ossia::hunter_lab{};
      }
    }
    std::string typeName() const noexcept override { return "ao::ColorAutomation"; }
    std::string initializer() const noexcept override
    {
      std::string init_list;
      auto& self = static_cast<const ProcessModel&>(this->self);
      auto& g = self.gradient();
      for(auto [pos, col] : g)
      {
        auto lab = to_ossia_color(col);
        init_list += fmt::format(
            "{{ {}, {{ {}, {}, {} }} }}, ", pos, lab.dataspace_value[0],
            lab.dataspace_value[1], lab.dataspace_value[2]);
      }
      return fmt::format(
          ".inputs = {{ .curve = {{ .value = {{ {{ {} }} }} }} }}", init_list);
    }

    std::string accessInlet(const Id<Process::Port>& id) const noexcept override
    {
      return "<! INVALID !>";
    }

    std::string accessOutlet(const Id<Process::Port>& id) const noexcept override
    {
      return fmt::format("{}.outputs.value.value", variable);
    }

    std::string execute() const noexcept override
    {
      return fmt::format(
          "{}( {{ .frames = g_tick.frames, .relative_position = "
          "g_tick.relative_position }} "
          ");",
          variable);
    }
  };
  return std::make_unique<CodeWriter>(*this);
}

const ::State::AddressAccessor& ProcessModel::address() const
{
  return outlet->address();
}

bool ProcessModel::tween() const
{
  return m_tween;
}

void ProcessModel::setTween(bool tween)
{
  if(m_tween == tween)
    return;

  m_tween = tween;
  tweenChanged(tween);
}

void ProcessModel::setDurationAndScale(const TimeVal& newDuration) noexcept
{
  // We only need to change the duration.
  setDuration(newDuration);
}

void ProcessModel::setDurationAndGrow(const TimeVal& newDuration) noexcept
{
  setDuration(newDuration);
}

void ProcessModel::setDurationAndShrink(const TimeVal& newDuration) noexcept
{
  setDuration(newDuration);
}

TimeVal ProcessModel::contentDuration() const noexcept
{
  auto lastPoint = 1.;
  if(!m_colors.empty())
  {
    auto back = m_colors.rbegin()->first;
    lastPoint = std::max(1., back);
  }

  return TimeVal(duration().impl * lastPoint);
}

void ProcessModel::loadPreset(const Process::Preset& preset)
{
  m_colors.clear();

  auto json = readJson(preset.data);
  JSONWriter wr{json};
  wr.writeTo(m_colors);
  gradientChanged();
}

Process::Preset ProcessModel::savePreset() const noexcept
{
  Process::Preset p;
  p.name = this->metadata().getName();
  p.key = {this->concreteKey(), {}};

  JSONReader r;
  r.readFrom(m_colors);

  p.data = r.toByteArray();
  return p;
}

std::optional<Process::MagneticInfo>
ProcessModel::magneticPosition(const QObject* o, const TimeVal t) const noexcept
{
  double pos = t.impl / double(this->duration().impl);

  auto it = ossia::closest_element_it(m_colors, pos);
  if(it != m_colors.end())
    return Process::MagneticInfo{TimeVal(it->first * this->duration().impl), true};
  return {};
}
}

template <>
void DataStreamReader::read(const Gradient::ProcessModel& autom)
{
  m_stream << *autom.outlet;
  m_stream << autom.m_colors << autom.m_tween;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gradient::ProcessModel& autom)
{
  autom.outlet = Process::load_value_outlet(*this, &autom);
  m_stream >> autom.m_colors >> autom.m_tween;

  checkDelimiter();
}

template <>
void JSONReader::read(const Gradient::ProcessModel& autom)
{
  obj["Outlet"] = *autom.outlet;
  obj["Gradient"] = autom.m_colors;
  obj["Tween"] = autom.tween();
}

template <>
void JSONWriter::write(Gradient::ProcessModel& autom)
{
  JSONWriter writer{obj["Outlet"]};
  autom.outlet = Process::load_value_outlet(writer, &autom);
  SCORE_ASSERT(autom.outlet);
  if(autom.outlet->address() == State::AddressAccessor{})
  {
    // Set as ARGB to keep compat with previous versions
    State::AddressAccessor addr;
    addr.qualifiers.get().unit = ossia::argb_u{};
    autom.outlet->setAddress(addr);
  }

  autom.setTween(obj["Tween"].toBool());
  autom.m_colors <<= obj["Gradient"];
}
