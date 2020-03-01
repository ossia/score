#include <QDebug>
#include "Port.hpp"

#include "PortItem.hpp"

#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortListWidget.hpp>
#include <Control/Widgets.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <ossia/dataflow/port.hpp>

#include <ossia-qt/value_metatypes.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Process::Port)
W_OBJECT_IMPL(Process::Inlet)
W_OBJECT_IMPL(Process::Outlet)
W_OBJECT_IMPL(Process::ValueInlet)
W_OBJECT_IMPL(Process::ValueOutlet)
W_OBJECT_IMPL(Process::AudioInlet)
W_OBJECT_IMPL(Process::AudioOutlet)
W_OBJECT_IMPL(Process::MidiInlet)
W_OBJECT_IMPL(Process::MidiOutlet)
W_OBJECT_IMPL(Process::ControlInlet)
W_OBJECT_IMPL(Process::ControlOutlet)
namespace Process
{
MODEL_METADATA_IMPL_CPP(Inlet)
MODEL_METADATA_IMPL_CPP(Outlet)
MODEL_METADATA_IMPL_CPP(ValueInlet)
MODEL_METADATA_IMPL_CPP(ValueOutlet)
MODEL_METADATA_IMPL_CPP(AudioInlet)
MODEL_METADATA_IMPL_CPP(AudioOutlet)
MODEL_METADATA_IMPL_CPP(MidiInlet)
MODEL_METADATA_IMPL_CPP(MidiOutlet)
MODEL_METADATA_IMPL_CPP(ControlInlet)
MODEL_METADATA_IMPL_CPP(ControlOutlet)
Port::~Port() {}

Port::Port(Id<Port> c, const QString& name, QObject* parent)
    : IdentifiedObject<Port>{c, name, parent}
{
}

Port::Port(DataStream::Deserializer& vis, QObject* parent)
    : IdentifiedObject{vis, parent}
{
  vis.writeTo(*this);
}
Port::Port(JSONObject::Deserializer& vis, QObject* parent)
    : IdentifiedObject{vis, parent}
{
  vis.writeTo(*this);
}
Port::Port(DataStream::Deserializer&& vis, QObject* parent)
    : IdentifiedObject{vis, parent}
{
  vis.writeTo(*this);
}
Port::Port(JSONObject::Deserializer&& vis, QObject* parent)
    : IdentifiedObject{vis, parent}
{
  vis.writeTo(*this);
}

void Port::addCable(const Path<Cable>& c)
{
  m_cables.push_back(c);
  cablesChanged();
}
void Port::setCables(const std::vector<Path<Cable>>& c)
{
  m_cables = c;
  cablesChanged();
}

void Port::removeCable(const Path<Cable>& c)
{
  auto it = ossia::find(m_cables, c);
  if (it != m_cables.end())
  {
    m_cables.erase(it);
    cablesChanged();
  }
}

const QString& Port::customData() const noexcept
{
  return m_customData;
}
const QString& Port::exposed() const noexcept
{
  return m_exposed;
}
const QString& Port::description() const noexcept
{
  return m_description;
}

const State::AddressAccessor& Port::address() const noexcept
{
  return m_address;
}

const std::vector<Path<Cable>>& Port::cables() const noexcept
{
  return m_cables;
}

void Port::setCustomData(const QString& customData)
{
  if (m_customData == customData)
    return;

  m_customData = customData;
  customDataChanged(m_customData);
}

void Port::setExposed(const QString& exposed)
{
  if (m_exposed == exposed)
    return;

  m_exposed = exposed;
  exposedChanged(m_exposed);
}

void Port::setDescription(const QString& description)
{
  if (m_description == description)
    return;

  m_description = description;
  descriptionChanged(m_description);
}

void Port::setAddress(const State::AddressAccessor& address)
{
  if (m_address == address)
    return;

  m_address = address;
  addressChanged(m_address);
}

///////////////////////////////
/// Inlet
///////////////////////////////

Inlet::~Inlet() {}

Inlet::Inlet(Id<Process::Port> c, QObject* parent)
    : Port{std::move(c), QStringLiteral("Inlet"), parent}
{
}

Inlet::Inlet(DataStream::Deserializer& vis, QObject* parent)
    : Port{vis, parent}
{
  vis.writeTo(*this);
}
Inlet::Inlet(JSONObject::Deserializer& vis, QObject* parent)
    : Port{vis, parent}
{
  vis.writeTo(*this);
}
Inlet::Inlet(DataStream::Deserializer&& vis, QObject* parent)
    : Port{vis, parent}
{
  vis.writeTo(*this);
}
Inlet::Inlet(JSONObject::Deserializer&& vis, QObject* parent)
    : Port{vis, parent}
{
  vis.writeTo(*this);
}

void Inlet::forChildInlets(const smallfun::function<void (Inlet&)>& f) const noexcept
{
}

void Inlet::mapExecution(ossia::inlet& exec, const smallfun::function<void (Inlet&, ossia::inlet&)>& f) const noexcept
{
}

ControlInlet::ControlInlet(Id<Process::Port> c, QObject* parent)
    : Inlet{std::move(c), parent}
{
}
ControlInlet::~ControlInlet() {}

ControlInlet::ControlInlet(DataStream::Deserializer& vis, QObject* parent)
    : Inlet{vis, parent}
{
  vis.writeTo(*this);
}
ControlInlet::ControlInlet(JSONObject::Deserializer& vis, QObject* parent)
    : Inlet{vis, parent}
{
  vis.writeTo(*this);
}
ControlInlet::ControlInlet(DataStream::Deserializer&& vis, QObject* parent)
    : Inlet{vis, parent}
{
  vis.writeTo(*this);
}
ControlInlet::ControlInlet(JSONObject::Deserializer&& vis, QObject* parent)
    : Inlet{vis, parent}
{
  vis.writeTo(*this);
}

Outlet::~Outlet() {}

Outlet::Outlet(Id<Process::Port> c, QObject* parent)
    : Port{std::move(c), QStringLiteral("Outlet"), parent}
{
}

Outlet::Outlet(DataStream::Deserializer& vis, QObject* parent)
    : Port{vis, parent}
{
  vis.writeTo(*this);
}
Outlet::Outlet(JSONObject::Deserializer& vis, QObject* parent)
    : Port{vis, parent}
{
  vis.writeTo(*this);
}
Outlet::Outlet(DataStream::Deserializer&& vis, QObject* parent)
    : Port{vis, parent}
{
  vis.writeTo(*this);
}
Outlet::Outlet(JSONObject::Deserializer&& vis, QObject* parent)
    : Port{vis, parent}
{
  vis.writeTo(*this);
}

void Outlet::forChildInlets(const smallfun::function<void (Inlet&)>& f) const noexcept
{
}

void Outlet::mapExecution(ossia::outlet& exec, const smallfun::function<void (Inlet&, ossia::inlet&)>& f) const noexcept
{
}

AudioInlet::~AudioInlet() {}

AudioInlet::AudioInlet(Id<Process::Port> c, QObject* parent)
  : Inlet{std::move(c), parent}
{
}

AudioInlet::AudioInlet(DataStream::Deserializer& vis, QObject* parent)
  : Inlet{vis, parent}
{
  vis.writeTo(*this);
}
AudioInlet::AudioInlet(JSONObject::Deserializer& vis, QObject* parent)
  : Inlet{vis, parent}
{
  vis.writeTo(*this);
}
AudioInlet::AudioInlet(DataStream::Deserializer&& vis, QObject* parent)
  : Inlet{vis, parent}
{
  vis.writeTo(*this);
}
AudioInlet::AudioInlet(JSONObject::Deserializer&& vis, QObject* parent)
  : Inlet{vis, parent}
{
  vis.writeTo(*this);
}

AudioOutlet::~AudioOutlet() {}

AudioOutlet::AudioOutlet(Id<Process::Port> c, QObject* parent)
  : Outlet{std::move(c), parent}
  , gainInlet{std::make_unique<ControlInlet>(Id<Process::Port>{0}, this)}
  , panInlet{std::make_unique<ControlInlet>(Id<Process::Port>{1}, this)}
  , m_gain{1.}
  , m_pan{ossia::sqrt_2 / 2., ossia::sqrt_2 / 2.}
{
}

AudioOutlet::AudioOutlet(DataStream::Deserializer& vis, QObject* parent)
  : Outlet{vis, parent}
{
  vis.writeTo(*this);
}
AudioOutlet::AudioOutlet(JSONObject::Deserializer& vis, QObject* parent)
  : Outlet{vis, parent}
{
  vis.writeTo(*this);
}
AudioOutlet::AudioOutlet(DataStream::Deserializer&& vis, QObject* parent)
  : Outlet{vis, parent}
{
  vis.writeTo(*this);
}
AudioOutlet::AudioOutlet(JSONObject::Deserializer&& vis, QObject* parent)
  : Outlet{vis, parent}
{
  vis.writeTo(*this);
}

void AudioOutlet::forChildInlets(const smallfun::function<void (Inlet&)>& f) const noexcept
{
  f(*gainInlet);
  f(*panInlet);
}

void AudioOutlet::mapExecution(ossia::outlet& exec, const smallfun::function<void (Inlet&, ossia::inlet&)>& f) const noexcept
{
  auto audio_exec = safe_cast<ossia::audio_outlet*>(&exec);
  f(*gainInlet, audio_exec->gain_inlet);
  f(*panInlet, audio_exec->pan_inlet);
}

bool AudioOutlet::propagate() const
{
  return m_propagate;
}

void AudioOutlet::setPropagate(bool propagate)
{
  if (m_propagate == propagate)
    return;

  m_propagate = propagate;
  propagateChanged(m_propagate);
}

double AudioOutlet::gain() const
{
  return m_gain;
}

void AudioOutlet::setGain(double gain)
{
  if (m_gain == gain)
    return;

  m_gain = gain;
  gainChanged(m_gain);
}

ossia::small_vector<double, 2> AudioOutlet::pan() const
{
  return m_pan;
}

void AudioOutlet::setPan(ossia::small_vector<double, 2> pan)
{
  if (m_pan == pan)
    return;

  m_pan = pan;
  panChanged(m_pan);
}


MidiInlet::~MidiInlet() {}

MidiInlet::MidiInlet(Id<Process::Port> c, QObject* parent)
  : Inlet{std::move(c), parent}
{
}

MidiInlet::MidiInlet(DataStream::Deserializer& vis, QObject* parent)
  : Inlet{vis, parent}
{
  vis.writeTo(*this);
}
MidiInlet::MidiInlet(JSONObject::Deserializer& vis, QObject* parent)
  : Inlet{vis, parent}
{
  vis.writeTo(*this);
}
MidiInlet::MidiInlet(DataStream::Deserializer&& vis, QObject* parent)
  : Inlet{vis, parent}
{
  vis.writeTo(*this);
}
MidiInlet::MidiInlet(JSONObject::Deserializer&& vis, QObject* parent)
  : Inlet{vis, parent}
{
  vis.writeTo(*this);
}

MidiOutlet::~MidiOutlet() {}

MidiOutlet::MidiOutlet(Id<Process::Port> c, QObject* parent)
  : Outlet{std::move(c), parent}
{
}

MidiOutlet::MidiOutlet(DataStream::Deserializer& vis, QObject* parent)
  : Outlet{vis, parent}
{
  vis.writeTo(*this);
}
MidiOutlet::MidiOutlet(JSONObject::Deserializer& vis, QObject* parent)
  : Outlet{vis, parent}
{
  vis.writeTo(*this);
}
MidiOutlet::MidiOutlet(DataStream::Deserializer&& vis, QObject* parent)
  : Outlet{vis, parent}
{
  vis.writeTo(*this);
}
MidiOutlet::MidiOutlet(JSONObject::Deserializer&& vis, QObject* parent)
  : Outlet{vis, parent}
{
  vis.writeTo(*this);
}

ControlOutlet::ControlOutlet(Id<Process::Port> c, QObject* parent)
    : Outlet{std::move(c), parent}
{
}

ControlOutlet::~ControlOutlet() {}

ControlOutlet::ControlOutlet(DataStream::Deserializer& vis, QObject* parent)
    : Outlet{vis, parent}
{
  vis.writeTo(*this);
}
ControlOutlet::ControlOutlet(JSONObject::Deserializer& vis, QObject* parent)
    : Outlet{vis, parent}
{
  vis.writeTo(*this);
}
ControlOutlet::ControlOutlet(DataStream::Deserializer&& vis, QObject* parent)
    : Outlet{vis, parent}
{
  vis.writeTo(*this);
}
ControlOutlet::ControlOutlet(JSONObject::Deserializer&& vis, QObject* parent)
    : Outlet{vis, parent}
{
  vis.writeTo(*this);
}


ValueInlet::~ValueInlet() {}

ValueInlet::ValueInlet(Id<Process::Port> c, QObject* parent)
  : Inlet{std::move(c), parent}
{
}

ValueInlet::ValueInlet(DataStream::Deserializer& vis, QObject* parent)
  : Inlet{vis, parent}
{
  vis.writeTo(*this);
}
ValueInlet::ValueInlet(JSONObject::Deserializer& vis, QObject* parent)
  : Inlet{vis, parent}
{
  vis.writeTo(*this);
}
ValueInlet::ValueInlet(DataStream::Deserializer&& vis, QObject* parent)
  : Inlet{vis, parent}
{
  vis.writeTo(*this);
}
ValueInlet::ValueInlet(JSONObject::Deserializer&& vis, QObject* parent)
  : Inlet{vis, parent}
{
  vis.writeTo(*this);
}

ValueOutlet::~ValueOutlet() {}

ValueOutlet::ValueOutlet(Id<Process::Port> c, QObject* parent)
  : Outlet{std::move(c), parent}
{
}

ValueOutlet::ValueOutlet(DataStream::Deserializer& vis, QObject* parent)
  : Outlet{vis, parent}
{
  vis.writeTo(*this);
}
ValueOutlet::ValueOutlet(JSONObject::Deserializer& vis, QObject* parent)
  : Outlet{vis, parent}
{
  vis.writeTo(*this);
}
ValueOutlet::ValueOutlet(DataStream::Deserializer&& vis, QObject* parent)
  : Outlet{vis, parent}
{
  vis.writeTo(*this);
}
ValueOutlet::ValueOutlet(JSONObject::Deserializer&& vis, QObject* parent)
  : Outlet{vis, parent}
{
  vis.writeTo(*this);
}



PortFactory::~PortFactory() {}

Dataflow::PortItem* PortFactory::makeItem(
    Inlet& port,
    const Process::Context& ctx,
    QGraphicsItem* parent,
    QObject* context)
{
  return new Dataflow::PortItem{port, ctx, parent};
}

Dataflow::PortItem* PortFactory::makeItem(
    Outlet& port,
    const Process::Context& ctx,
    QGraphicsItem* parent,
    QObject* context)
{
  return new Dataflow::PortItem{port, ctx, parent};
}

void PortFactory::setupInletInspector(
    Inlet& port,
    const score::DocumentContext& ctx,
    QWidget* parent,
    Inspector::Layout& lay,
    QObject* context)
{
  PortWidgetSetup::setupInLayout(port, ctx, lay, parent);
}

void PortFactory::setupOutletInspector(
    Outlet& port,
    const score::DocumentContext& ctx,
    QWidget* parent,
    Inspector::Layout& lay,
    QObject* context)
{
  PortWidgetSetup::setupInLayout(port, ctx, lay, parent);
}


QWidget* PortFactory::makeControlWidget(
      ControlInlet& port,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context)
{
  return nullptr;
}

QGraphicsItem* PortFactory::makeControlItem(
      ControlInlet& port,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context)
{
  auto& dom = port.domain().get();
  if (bool(dom))
  {
    auto min = dom.convert_min<float>();
    auto max = dom.convert_max<float>();
    struct
    {
      float min, max;
      float getMin() const { return min; }
      float getMax() const { return max; }
    } info{min, max};
    return WidgetFactory::FloatSlider::make_item(info, port, ctx, nullptr, context);
  }
  else
  {
    struct SliderInfo
    {
      static float getMin() { return 0.; }
      static float getMax() { return 1.; }
    };
    return WidgetFactory::FloatSlider::make_item(SliderInfo{}, port, ctx, nullptr, context);
  }
}

QGraphicsItem* PortFactory::makeControlItem(
      ControlOutlet& port,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context)
{
  auto& dom = port.domain().get();
  if (bool(dom))
  {
    auto min = dom.convert_min<float>();
    auto max = dom.convert_max<float>();
    struct
    {
      float min, max;
      float getMin() const { return min; }
      float getMax() const { return max; }
    } info{min, max};
    return WidgetFactory::FloatSlider::make_item(info, port, ctx, parent, context);
  }
  else
  {
    struct SliderInfo
    {
      static float getMin() { return 0.; }
      static float getMax() { return 1.; }
    };
    return WidgetFactory::FloatSlider::make_item(SliderInfo{}, port, ctx, parent, context);
  }
}

Port* PortFactoryList::loadMissing(const VisitorVariant& vis, QObject* parent)
    const
{
  return nullptr;
}

PortFactoryList::~PortFactoryList() {}

std::unique_ptr<Inlet> load_inlet(DataStreamWriter& wr, QObject* parent)
{
  static auto& il
      = score::AppComponents().interfaces<Process::PortFactoryList>();
  return std::unique_ptr<Process::Inlet>((Process::Inlet*)deserialize_interface(il, wr, parent));
}

std::unique_ptr<Inlet> load_inlet(JSONObjectWriter& wr, QObject* parent)
{
  static auto& il
      = score::AppComponents().interfaces<Process::PortFactoryList>();
  return std::unique_ptr<Process::Inlet>((Process::Inlet*)deserialize_interface(il, wr, parent));
}

std::unique_ptr<Outlet> load_outlet(DataStreamWriter& wr, QObject* parent)
{
  static auto& il
      = score::AppComponents().interfaces<Process::PortFactoryList>();
  return std::unique_ptr<Process::Outlet>((Process::Outlet*)deserialize_interface(il, wr, parent));
}

std::unique_ptr<Outlet> load_outlet(JSONObjectWriter& wr, QObject* parent)
{
  static auto& il
      = score::AppComponents().interfaces<Process::PortFactoryList>();
  return std::unique_ptr<Process::Outlet>((Process::Outlet*)deserialize_interface(il, wr, parent));
}

static auto copy_port(const Port& src, Port& dst)
{
  dst.hidden = src.hidden;
  dst.setCustomData(src.customData());
  dst.setAddress(src.address());
  dst.setExposed(src.exposed());
  dst.setDescription(src.description());
  for(auto& cable : src.cables())
    dst.addCable(cable);
}

template<typename T, typename W>
auto load_port_t(W& wr, QObject* parent)
{
  auto out = [&] {
    if constexpr(std::is_base_of_v<Inlet, T>)
        return load_inlet(wr, parent).release();
    else if constexpr(std::is_base_of_v<Outlet, T>)
        return load_outlet(wr, parent).release();
    else
        return nullptr;
  }();

  if(auto p = dynamic_cast<T*>(out))
  {
    return std::unique_ptr<T>(static_cast<T*>(out));
  }
  else if(out)
  {
    // Pre 2.0
    auto new_p = std::make_unique<T>(out->id(), parent);
    copy_port(*out, *new_p);
    delete out;
    return new_p;
  }
  else
  {
    // This works with a specific id because it is only for pre 1.0 saves
    return std::make_unique<T>(Id<Process::Port>(0), parent);
  }
}

std::unique_ptr<ValueInlet> load_value_inlet(DataStreamWriter& wr, QObject* parent)
{
  return load_port_t<ValueInlet>(wr, parent);
}

std::unique_ptr<ValueInlet> load_value_inlet(JSONObjectWriter& wr, QObject* parent)
{
  return load_port_t<ValueInlet>(wr, parent);
}

std::unique_ptr<ValueOutlet> load_value_outlet(DataStreamWriter& wr, QObject* parent)
{
  return load_port_t<ValueOutlet>(wr, parent);
}

std::unique_ptr<ValueOutlet> load_value_outlet(JSONObjectWriter& wr, QObject* parent)
{
  return load_port_t<ValueOutlet>(wr, parent);
}

std::unique_ptr<ControlInlet> load_control_inlet(DataStreamWriter& wr, QObject* parent)
{
  return load_port_t<ControlInlet>(wr, parent);
}

std::unique_ptr<ControlInlet> load_control_inlet(JSONObjectWriter& wr, QObject* parent)
{
  return load_port_t<ControlInlet>(wr, parent);
}

std::unique_ptr<ControlOutlet> load_control_outlet(DataStreamWriter& wr, QObject* parent)
{
  return load_port_t<ControlOutlet>(wr, parent);
}

std::unique_ptr<ControlOutlet> load_control_outlet(JSONObjectWriter& wr, QObject* parent)
{
  return load_port_t<ControlOutlet>(wr, parent);
}

std::unique_ptr<AudioInlet> load_audio_inlet(DataStreamWriter& wr, QObject* parent)
{
    return load_port_t<AudioInlet>(wr, parent);
}

std::unique_ptr<AudioInlet> load_audio_inlet(JSONObjectWriter& wr, QObject* parent)
{
    return load_port_t<AudioInlet>(wr, parent);
}

std::unique_ptr<AudioOutlet> load_audio_outlet(DataStreamWriter& wr, QObject* parent)
{
    return load_port_t<AudioOutlet>(wr, parent);
}

std::unique_ptr<AudioOutlet> load_audio_outlet(JSONObjectWriter& wr, QObject* parent)
{
    return load_port_t<AudioOutlet>(wr, parent);
}

std::unique_ptr<MidiInlet> load_midi_inlet(DataStreamWriter& wr, QObject* parent)
{
    return load_port_t<MidiInlet>(wr, parent);
}

std::unique_ptr<MidiInlet> load_midi_inlet(JSONObjectWriter& wr, QObject* parent)
{
    return load_port_t<MidiInlet>(wr, parent);
}

std::unique_ptr<MidiOutlet> load_midi_outlet(DataStreamWriter& wr, QObject* parent)
{
    return load_port_t<MidiOutlet>(wr, parent);
}

std::unique_ptr<MidiOutlet> load_midi_outlet(JSONObjectWriter& wr, QObject* parent)
{
    return load_port_t<MidiOutlet>(wr, parent);
}
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::Port>(const Process::Port& p)
{
  insertDelimiter();
  m_stream << p.hidden << p.m_customData << p.m_exposed
           << p.m_description << p.m_address;
  insertDelimiter();
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::Port>(Process::Port& p)
{
  checkDelimiter();
  m_stream >> p.hidden >> p.m_customData >> p.m_exposed
      >> p.m_description >> p.m_address;
  checkDelimiter();
}

template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::read<Process::Port>(const Process::Port& p)
{
  obj["Hidden"] = (bool)p.hidden;
  obj["Custom"] = p.m_customData;
  obj["Exposed"] = p.m_exposed;
  obj["Description"] = p.m_description;
  obj["Address"] = toJsonObject(p.m_address);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write<Process::Port>(Process::Port& p)
{
  p.hidden = obj["Hidden"].toBool();
  p.m_customData = obj["Custom"].toString();
  p.m_exposed = obj["Exposed"].toString();
  p.m_description = obj["Description"].toString();
  p.m_address = fromJsonObject<State::AddressAccessor>(obj["Address"]);
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::Inlet>(const Process::Inlet& p)
{
  read((Process::Port&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::Inlet>(Process::Inlet& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::read<Process::Inlet>(const Process::Inlet& p)
{
  read((Process::Port&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write<Process::Inlet>(Process::Inlet& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::AudioInlet>(const Process::AudioInlet& p)
{
  // read((Process::Outlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::AudioInlet>(Process::AudioInlet& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::read<Process::AudioInlet>(const Process::AudioInlet& p)
{
  // read((Process::Outlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write<Process::AudioInlet>(Process::AudioInlet& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::MidiInlet>(const Process::MidiInlet& p)
{
    // read((Process::Outlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::MidiInlet>(Process::MidiInlet& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::read<Process::MidiInlet>(const Process::MidiInlet& p)
{
    // read((Process::Outlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write<Process::MidiInlet>(Process::MidiInlet& p)
{
}


template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::ControlInlet>(const Process::ControlInlet& p)
{
  // read((Process::Inlet&)p);
  readFrom(p.m_value);
  readFrom(p.m_domain);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::ControlInlet>(Process::ControlInlet& p)
{
  writeTo(p.m_value);
  writeTo(p.m_domain);
}

template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::read<Process::ControlInlet>(const Process::ControlInlet& p)
{
  // read((Process::Inlet&)p);
  obj[strings.Value] = toJsonObject(p.m_value);
  obj[strings.Domain] = toJsonObject(p.m_domain);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write<Process::ControlInlet>(Process::ControlInlet& p)
{
  p.m_value = fromJsonObject<ossia::value>(obj[strings.Value]);
  p.m_domain = fromJsonObject<State::Domain>(obj[strings.Domain].toObject());
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::Outlet>(const Process::Outlet& p)
{
  read((Process::Port&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::Outlet>(Process::Outlet& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::read<Process::Outlet>(const Process::Outlet& p)
{
  read((Process::Port&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write<Process::Outlet>(Process::Outlet& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::AudioOutlet>(const Process::AudioOutlet& p)
{
  // read((Process::Outlet&)p);
  m_stream
      << *p.gainInlet << *p.panInlet
      << p.m_gain << p.m_pan << p.m_propagate;
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::AudioOutlet>(Process::AudioOutlet& p)
{
  p.gainInlet = Process::load_control_inlet(*this, &p);
  p.panInlet = Process::load_control_inlet(*this, &p);
  m_stream >> p.m_gain >> p.m_pan >> p.m_propagate;
}

template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::read<Process::AudioOutlet>(const Process::AudioOutlet& p)
{
  // read((Process::Outlet&)p);
  obj["GainInlet"] = toJsonObject(*p.gainInlet);
  obj["PanInlet"] = toJsonObject(*p.panInlet);

  obj["Gain"] = p.m_gain;
  obj["Pan"] = toJsonValueArray(p.m_pan);
  obj["Propagate"] = p.m_propagate;

}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write<Process::AudioOutlet>(Process::AudioOutlet& p)
{
  {
    JSONObjectWriter writer{obj["GainInlet"].toObject()};
    p.gainInlet = Process::load_control_inlet(writer, &p);
  }
  {
    JSONObjectWriter writer{obj["PanInlet"].toObject()};
    p.panInlet = Process::load_control_inlet(writer, &p);
  }

  p.m_gain = obj["Gain"].toDouble();
  p.m_pan = fromJsonValueArray<ossia::small_vector<double, 2>>(obj["Pan"].toArray());
  p.m_propagate = obj["Propagate"].toBool();
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::MidiOutlet>(const Process::MidiOutlet& p)
{
    // read((Process::Outlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::MidiOutlet>(Process::MidiOutlet& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::read<Process::MidiOutlet>(const Process::MidiOutlet& p)
{
    // read((Process::Outlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write<Process::MidiOutlet>(Process::MidiOutlet& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::ControlOutlet>(const Process::ControlOutlet& p)
{
  // read((Process::Outlet&)p);
  readFrom(p.m_value);
  readFrom(p.m_domain);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::ControlOutlet>(Process::ControlOutlet& p)
{
  writeTo(p.m_value);
  writeTo(p.m_domain);
}

template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::read<Process::ControlOutlet>(const Process::ControlOutlet& p)
{
  // read((Process::Outlet&)p);
  obj[strings.Value] = toJsonValue(p.m_value);
  obj[strings.Domain] = toJsonObject(p.m_domain);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write<Process::ControlOutlet>(Process::ControlOutlet& p)
{
  p.m_value = fromJsonValue<ossia::value>(obj[strings.Value]);
  p.m_domain = fromJsonObject<State::Domain>(obj[strings.Domain].toObject());
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::ValueInlet>(const Process::ValueInlet& p)
{
    // read((Process::Outlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::ValueInlet>(Process::ValueInlet& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::read<Process::ValueInlet>(const Process::ValueInlet& p)
{
    // read((Process::Outlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write<Process::ValueInlet>(Process::ValueInlet& p)
{
}


template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::ValueOutlet>(const Process::ValueOutlet& p)
{
    // read((Process::Outlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::ValueOutlet>(Process::ValueOutlet& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::read<Process::ValueOutlet>(const Process::ValueOutlet& p)
{
    // read((Process::Outlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write<Process::ValueOutlet>(Process::ValueOutlet& p)
{
}

