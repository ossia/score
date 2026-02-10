#include "Port.hpp"

#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/ControlWidgets.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Dataflow/PortListWidget.hpp>
#include <Process/Dataflow/PortSerialization.hpp>
#include <Process/Dataflow/PrettyPortName.hpp>
#include <Process/ProcessContext.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/graphics/GraphicsLayout.hpp>
#include <score/graphics/RectItem.hpp>
#include <score/graphics/TextItem.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>

#include <ossia/dataflow/port.hpp>
#include <ossia/network/common/destination_qualifiers.hpp>

#include <ossia-qt/name_utils.hpp>
#include <ossia-qt/value_metatypes.hpp>

#include <QDebug>
#include <QIODevice>

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

Port::~Port() { }

Port::Port(Id<Port> c, const QString& name, QObject* parent)
    : IdentifiedObject<Port>{std::move(c), name, parent}
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

void Port::addCable(const Cable& c)
{
  for(auto& old_c : m_cables)
  {
    auto& old_c_last_fragment = old_c.unsafePath().vec().back();
    if(bool cable_already_in_port = c.id().val() == old_c_last_fragment.id())
    {
      SCORE_SOFT_ASSERT(!cable_already_in_port);
      c.resetCache();
      cablesChanged();
      return;
    }
  }

  c.resetCache();
  m_cables.push_back(c);
  cablesChanged();
}

void Port::takeCables(Port&& c)
{
  // TODO how do we reset their cache
  m_cables = std::move(c.cables());
  cablesChanged();
  c.cablesChanged();
}

const QString& Port::visualName() const noexcept
{
  if(Q_LIKELY(!m_name.isEmpty()))
    return m_name;
  else
    return m_exposed;
}

const QString& Port::visualDescription() const noexcept
{
  if(!m_description.isEmpty())
    return m_description;
  else if(Q_LIKELY(!m_name.isEmpty()))
    return m_name;
  else
    return m_exposed;
}

void Port::removeCable(const Path<Cable>& c)
{
  auto it = ossia::find(m_cables, c);
  if(it != m_cables.end())
  {
    m_cables.erase(it);
    cablesChanged();
  }
}

const QString& Port::name() const noexcept
{
  return m_name;
}
const QString& Port::exposed() const noexcept
{
  if(!m_exposed.isEmpty())
    return m_exposed;
  else
    return m_name;
}
const QString& Port::description() const noexcept
{
  return m_description;
}

Device::FullAddressAccessorSettings Port::settings() const noexcept
{
  Device::FullAddressAccessorSettings set;
  set.address = this->m_address;
  return set;
}

void Port::setSettings(const Device::FullAddressAccessorSettings& set) noexcept
{
  setAddress(set.address);
}

const State::AddressAccessor& Port::address() const noexcept
{
  return m_address;
}

const std::vector<Path<Cable>>& Port::cables() const noexcept
{
  return m_cables;
}

void Port::setName(const QString& name)
{
  if(m_name == name)
    return;

  if(name.isEmpty())
  {
    qDebug() << "Warning: tried to create a port with an empty name";
    return;
  }

  m_name = name;
  QString newExposed = m_name.toLower();
  ossia::net::sanitize_name(newExposed);
  setExposed(newExposed);
  nameChanged(m_name);
}

void Port::setExposed(const QString& exposed)
{
  if(m_exposed == exposed)
    return;

  if(exposed.isEmpty())
  {
    qDebug() << "Warning: tried to create a port with an empty exposed name";
    return;
  }

  m_exposed = exposed;
  exposedChanged(m_exposed);
}

void Port::setDescription(const QString& description)
{
  if(m_description == description)
    return;

  m_description = description;
  descriptionChanged(m_description);
}

void Port::setAddress(const State::AddressAccessor& address)
{
  if(m_address == address)
    return;

  m_address = address;
  addressChanged(m_address);
}

QByteArray Port::saveData() const noexcept
{
  QByteArray arr;
  {
    QDataStream p{&arr, QIODevice::WriteOnly};
    DataStreamInput ip{p};
    ip << m_cables << m_address;
  }
  return arr;
}

void Port::loadData(const QByteArray& arr, PortLoadDataFlags flags) noexcept
{
  QDataStream p{arr};
  DataStreamOutput op{p};
  op >> m_cables >> m_address;
}

///////////////////////////////
/// Inlet
///////////////////////////////

Inlet::~Inlet() { }

void Inlet::setupExecution(ossia::inlet&, QObject* exec_context) const noexcept { }

Inlet::Inlet(const QString& name, Id<Process::Port> c, QObject* parent)
    : Port{std::move(c), QStringLiteral("Inlet"), parent}
{
  setName(name);
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

void Inlet::forChildInlets(const smallfun::function<void(Inlet&)>& f) const noexcept { }

void Inlet::mapExecution(
    ossia::inlet& exec,
    const smallfun::function<void(Inlet&, ossia::inlet&)>& f) const noexcept
{
}

ControlInlet::ControlInlet(const QString& name, Id<Process::Port> c, QObject* parent)
    : Inlet{name, std::move(c), parent}
{
}

ControlInlet::~ControlInlet() { }

ControlInlet::ControlInlet(
    DataStream::Deserializer& vis, QObject* parent, bool skip_this)
    : Inlet{vis, parent}
{
  if(!skip_this)
    vis.writeTo(*this);
}
ControlInlet::ControlInlet(
    JSONObject::Deserializer& vis, QObject* parent, bool skip_this)
    : Inlet{vis, parent}
{
  if(!skip_this)
    vis.writeTo(*this);
}
ControlInlet::ControlInlet(
    DataStream::Deserializer&& vis, QObject* parent, bool skip_this)
    : Inlet{vis, parent}
{
  if(!skip_this)
    vis.writeTo(*this);
}
ControlInlet::ControlInlet(
    JSONObject::Deserializer&& vis, QObject* parent, bool skip_this)
    : Inlet{vis, parent}
{
  if(!skip_this)
    vis.writeTo(*this);
}

QByteArray ControlInlet::saveData() const noexcept
{
  QByteArray arr;
  {
    QDataStream p{&arr, QIODevice::WriteOnly};
    DataStreamInput ip{p};
    ip << m_cables << m_address << true << m_value;
  }
  return arr;
}

void ControlInlet::loadData(const QByteArray& arr, PortLoadDataFlags flags) noexcept
{
  QDataStream p{arr};
  DataStreamOutput op{p};
  bool has_value{};

  op >> m_cables >> m_address >> has_value;
  if(!((uint32_t)flags & (uint32_t)PortLoadDataFlags::DontReloadValue))
    has_value = false;

  if(has_value)
  {
    op >> m_value;
  }

  if(has_value)
  {
    valueChanged(m_value);
  }
}

void ControlInlet::setValue(const ossia::value& value)
{
  if(value != m_value || m_value.get_type() == ossia::val_type::IMPULSE
     || value.get_type() == ossia::val_type::IMPULSE)
  {
    m_value = value;
    valueChanged(value);
  }
}

void ControlInlet::setInit(const ossia::value& value)
{
  if(value != m_init)
  {
    m_init = value;
    initChanged(value);
  }
}

Outlet::~Outlet() { }

void Outlet::setupExecution(ossia::outlet&, QObject* exec_context) const noexcept { }

Outlet::Outlet(const QString& name, Id<Process::Port> c, QObject* parent)
    : Port{std::move(c), QStringLiteral("Outlet"), parent}
{
  setName(name);
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

void Outlet::forChildInlets(const smallfun::function<void(Inlet&)>& f) const noexcept { }

void Outlet::mapExecution(
    ossia::outlet& exec,
    const smallfun::function<void(Inlet&, ossia::inlet&)>& f) const noexcept
{
}

AudioInlet::~AudioInlet() { }

AudioInlet::AudioInlet(const QString& name, Id<Process::Port> c, QObject* parent)
    : Inlet{name, std::move(c), parent}
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

AudioOutlet::~AudioOutlet() { }

AudioOutlet::AudioOutlet(const QString& name, Id<Process::Port> c, QObject* parent)
    : Outlet{name, std::move(c), parent}
    , gainInlet{std::make_unique<ControlInlet>(
          "Gain", Id<Process::Port>{(1 + c.val()) * 10000 + 0}, this)}
    , panInlet{std::make_unique<ControlInlet>(
          "Pan", Id<Process::Port>{(1 + c.val()) * 10000 + 1}, this)}
    , m_gain{1.}
    , m_pan{1., 1.}
{
  gainInlet->setDomain(ossia::domain{ossia::domain_base<float>{0., 1.}});
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

void AudioOutlet::forChildInlets(
    const smallfun::function<void(Inlet&)>& f) const noexcept
{
  f(*gainInlet);
  f(*panInlet);
}

void AudioOutlet::mapExecution(
    ossia::outlet& exec,
    const smallfun::function<void(Inlet&, ossia::inlet&)>& f) const noexcept
{
  auto audio_exec = safe_cast<ossia::audio_outlet*>(&exec);
  f(*gainInlet, audio_exec->gain_inlet);
  f(*panInlet, audio_exec->pan_inlet);
}

QByteArray AudioOutlet::saveData() const noexcept
{
  QByteArray arr;
  {
    QDataStream p{&arr, QIODevice::WriteOnly};
    DataStreamInput ip{p};
    ip << Port::saveData() << m_gain << m_pan << m_propagate;
  }
  return arr;
}

void AudioOutlet::loadData(const QByteArray& arr, PortLoadDataFlags flags) noexcept
{
  QByteArray pdata;
  QDataStream p{arr};
  DataStreamOutput op{p};
  op >> pdata >> m_gain >> m_pan >> m_propagate;
  Port::loadData(pdata, flags);
  propagateChanged(m_propagate);
}

bool AudioOutlet::propagate() const
{
  return m_propagate;
}

void AudioOutlet::setPropagate(bool propagate)
{
  if(m_propagate == propagate)
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
  if(m_gain == gain)
    return;

  m_gain = gain;
  gainChanged(m_gain);
}

Process::pan_weight AudioOutlet::pan() const
{
  return m_pan;
}

void AudioOutlet::setPan(Process::pan_weight pan)
{
  if(m_pan == pan)
    return;

  m_pan = pan;
  panChanged(m_pan);
}

MidiInlet::~MidiInlet() { }

MidiInlet::MidiInlet(const QString& name, Id<Process::Port> c, QObject* parent)
    : Inlet{name, std::move(c), parent}
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

MidiOutlet::~MidiOutlet() { }

MidiOutlet::MidiOutlet(const QString& name, Id<Process::Port> c, QObject* parent)
    : Outlet{name, std::move(c), parent}
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

ControlOutlet::ControlOutlet(const QString& qname, Id<Process::Port> c, QObject* parent)
    : Outlet{qname, std::move(c), parent}
{
}

ControlOutlet::~ControlOutlet() { }

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

QByteArray ControlOutlet::saveData() const noexcept
{
  return Port::saveData();
}

void ControlOutlet::loadData(const QByteArray& arr, PortLoadDataFlags flags) noexcept
{
  Port::loadData(arr, flags);
}

ValueInlet::~ValueInlet() { }

ValueInlet::ValueInlet(const QString& name, Id<Process::Port> c, QObject* parent)
    : Inlet{name, std::move(c), parent}
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

QByteArray ValueInlet::saveData() const noexcept
{
  QByteArray arr;
  {
    QDataStream p{&arr, QIODevice::WriteOnly};
    DataStreamInput ip{p};
    ip << m_cables << m_address << false;
    // Boolean indicates that we don't have a value. Otherwise if we change from ValueInlet to
    // ControlInlet in e.g. a JS script, then the ControlInlet tries to reload the value and crashes.
  }
  return arr;
}

void ValueInlet::loadData(const QByteArray& arr, PortLoadDataFlags flags) noexcept
{
  QDataStream p{arr};
  DataStreamOutput op{p};
  op >> m_cables >> m_address;
}
ValueOutlet::~ValueOutlet() { }

ValueOutlet::ValueOutlet(const QString& name, Id<Process::Port> c, QObject* parent)
    : Outlet{name, std::move(c), parent}
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

PortFactory::~PortFactory() { }

Dataflow::PortItem* PortFactory::makePortItem(
    Inlet& port, const Process::Context& ctx, QGraphicsItem* parent, QObject* context)
{
  return new Dataflow::PortItem{port, ctx, parent};
}

Dataflow::PortItem* PortFactory::makePortItem(
    Outlet& port, const Process::Context& ctx, QGraphicsItem* parent, QObject* context)
{
  return new Dataflow::PortItem{port, ctx, parent};
}

void PortFactory::setupInletInspector(
    const Inlet& port, const score::DocumentContext& ctx, QWidget* parent,
    Inspector::Layout& lay, QObject* context)
{
  PortWidgetSetup::setupAlone(port, ctx, lay, parent);
}

void PortFactory::setupOutletInspector(
    const Outlet& port, const score::DocumentContext& ctx, QWidget* parent,
    Inspector::Layout& lay, QObject* context)
{
  PortWidgetSetup::setupAlone(port, ctx, lay, parent);
}

PortItemLayout PortFactory::defaultLayout() const noexcept
{
  return PortItemLayout{
      .port = QPointF{0., 4.}, .label = QPointF{20., 2.}, .control = QPointF{18., 15.}};
}

QGraphicsItem* PortFactory::makeControlItem(
    ControlInlet& port, const score::DocumentContext& ctx, QGraphicsItem* parent,
    QObject* context)
{
  auto& dom = port.domain().get();
  if(bool(dom))
  {
    auto min = dom.convert_min<float>();
    auto max = dom.convert_max<float>();
    struct
    {
      float min, max, init;
      float getMin() const { return min; }
      float getMax() const { return max; }
      float getInit() const { return init; }
    } info{min, max, ossia::convert<float>(port.init())};
    return WidgetFactory::FloatSlider::make_item(info, port, ctx, nullptr, context);
  }
  else
  {
    struct SliderInfo
    {
      static float getMin() { return 0.; }
      static float getMax() { return 1.; }
      static float getInit() { return 0.; }
    };
    return WidgetFactory::FloatSlider::make_item(
        SliderInfo{}, port, ctx, nullptr, context);
  }
}

QGraphicsItem* PortFactory::makeControlItem(
    ControlOutlet& port, const score::DocumentContext& ctx, QGraphicsItem* parent,
    QObject* context)
{
  auto& dom = port.domain().get();
  if(bool(dom))
  {
    auto min = dom.convert_min<float>();
    auto max = dom.convert_max<float>();
    struct
    {
      float min, max;
      float getMin() const { return min; }
      float getMax() const { return max; }
      float getInit() const { return min; }
    } info{min, max};
    return WidgetFactory::FloatSlider::make_item(info, port, ctx, parent, context);
  }
  else
  {
    struct SliderInfo
    {
      static float getMin() { return 0.; }
      static float getMax() { return 1.; }
      static float getInit() { return 0.; }
    };
    return WidgetFactory::FloatSlider::make_item(
        SliderInfo{}, port, ctx, parent, context);
  }
}

static auto makeFullItemImpl(
    const Process::Port& portModel, const Process::PortItemLayout& layout,
    QGraphicsItem& port, QGraphicsItem& control, auto& item)
{
  score::SimpleTextItem* lab{};
  using namespace score;

  // Port
  port.setParentItem(&item);
  port.setPos(layout.port);
  port.setZValue(3);

  // Control
  control.setParentItem(&item);
  control.setPos(layout.control);
  control.setZValue(2);

  // Text
  if(layout.labelVisible)
  {
    lab = Dataflow::makePortLabel(portModel, &item);
    lab->setPos(layout.label);
    auto widg_r = control.boundingRect();
    auto lab_r = lab->boundingRect();
    auto lab_w = lab_r.width();
    auto widg_w = widg_r.width();

    if(lab_w < widg_w && layout.labelAlignment == Qt::AlignCenter)
    {
      lab->setX(control.pos().x() + (widg_w - lab_w) / 2.);
    }
    else if(layout.labelAlignment == Qt::AlignLeft)
    {
      lab->setX(control.pos().x());
    }
    else
    {
      if(layout.controlAlignment & Qt::AlignRight)
      {
        // Control outlets
        lab->setPos(0, layout.label.y());

        port.setPos(lab_w - 5., port.pos().y());
        control.setPos(port.x() - widg_w, control.pos().y());
      }
    }
  }

  item.fitChildrenRect();
  return lab;
}

static auto makeLabelItemImpl(
    const Process::Port& portModel, const Process::PortItemLayout& layout,
    QGraphicsItem& port, auto& item)
{
  score::SimpleTextItem* lab{};
  using namespace score;

  // Port
  port.setParentItem(&item);
  port.setPos(layout.port);
  port.setZValue(3);

  // Text
  if(layout.labelVisible)
  {
    lab = Dataflow::makePortLabel(portModel, &item);
    lab->setPos(layout.label);
    if(layout.labelAlignment == Qt::AlignLeft)
    {
      lab->setPos(QPointF{layout.port.x() + 12., 5.});
    }
    else
    {
      lab->setPos(0, 5.);
      port.setPos(lab->boundingRect().width() + 2., port.y());
      //lab->setPos(QPointF{layout.port.x() - lab->boundingRect().width(), 5.});
    }
  }
  item.fitChildrenRect();
  return lab;
}

Process::ControlLayout PortFactory::makeFullItem(
    ControlInlet& portModel, const Process::Context& ctx, QGraphicsItem* parent,
    QObject* context)
{
  Process::ControlLayout ret;
  using namespace score;
#if defined(SCORE_DEBUG_CONTROL_RECTS)
  auto item = new score::GraphicsLayout{parent};
  item->setBrush(score::Skin::instance().Light.main);
#else
  auto item = new score::EmptyRectItem{parent};
#endif
  ret.container = item;

  const auto& layout = defaultLayout();
  ret.port_item = makePortItem(portModel, ctx, ret.container, context);
  SCORE_SOFT_ASSERT(ret.port_item);

  if(ret.port_item)
  {
    ret.control = makeControlItem(portModel, ctx, ret.container, context);
    SCORE_SOFT_ASSERT(ret.control);

    if(ret.control)
    {
      ret.label
          = makeFullItemImpl(portModel, layout, *ret.port_item, *ret.control, *item);
    }
  }

  return ret;
}

Process::ControlLayout PortFactory::makeFullItem(
    ControlOutlet& portModel, const Process::Context& ctx, QGraphicsItem* parent,
    QObject* context)
{
  Process::ControlLayout ret;
  using namespace score;
#if defined(SCORE_DEBUG_CONTROL_RECTS)
  auto item = new score::GraphicsLayout{parent};
  item->setBrush(score::Skin::instance().Light.main);
#else
  auto item = new score::EmptyRectItem{parent};
#endif
  ret.container = item;

  const auto& layout = defaultLayout();
  ret.port_item = makePortItem(portModel, ctx, item, context);
  SCORE_SOFT_ASSERT(ret.port_item);

  if(ret.port_item)
  {
    ret.control = makeControlItem(portModel, ctx, item, context);
    SCORE_SOFT_ASSERT(ret.control);

    if(ret.control)
    {
      ret.label
          = makeFullItemImpl(portModel, layout, *ret.port_item, *ret.control, *item);
    }
  }
  return ret;
}

static constexpr auto defaultInletLabelLayout = Process::PortItemLayout{
    .port = QPointF{0., 3.5},
    .label = QPointF{5., 0.},
    .control = QPointF{0., 0.},
    .labelAlignment = Qt::AlignLeft};
static constexpr auto defaultOutletLabelLayout = Process::PortItemLayout{
    .port = QPointF{0., 3.5},
    .label = QPointF{5., 0.},
    .control = QPointF{0., 0.},
    .labelAlignment = Qt::AlignRight};
;
Process::ControlLayout PortFactory::makeLabelItem(
    Inlet& portModel, const Process::Context& ctx, QGraphicsItem* parent,
    QObject* context)
{
  Process::ControlLayout ret;

  using namespace score;
#if defined(SCORE_DEBUG_CONTROL_RECTS)
  auto item = new score::GraphicsLayout{parent};
  item->setBrush(score::Skin::instance().Light.main);
#else
  auto item = new score::EmptyRectItem{parent};
#endif
  ret.container = item;

  ret.port_item = makePortItem(portModel, ctx, ret.container, context);
  SCORE_SOFT_ASSERT(ret.port_item);

  if(ret.port_item)
  {
    ret.label
        = makeLabelItemImpl(portModel, defaultInletLabelLayout, *ret.port_item, *item);
  }

  return ret;
}

Process::ControlLayout PortFactory::makeLabelItem(
    Outlet& portModel, const Process::Context& ctx, QGraphicsItem* parent,
    QObject* context)
{
  Process::ControlLayout ret;
  using namespace score;
#if defined(SCORE_DEBUG_CONTROL_RECTS)
  auto item = new score::GraphicsLayout{parent};
  item->setBrush(score::Skin::instance().Light.main);
#else
  auto item = new score::EmptyRectItem{parent};
#endif
  ret.container = item;

  ret.port_item = makePortItem(portModel, ctx, item, context);
  SCORE_SOFT_ASSERT(ret.port_item);

  if(ret.port_item)
  {
    ret.label
        = makeLabelItemImpl(portModel, defaultOutletLabelLayout, *ret.port_item, *item);
  }
  return ret;
}

Port* PortFactoryList::loadMissing(const VisitorVariant& vis, QObject* parent) const
{
  return nullptr;
}

PortFactoryList::~PortFactoryList() { }

std::unique_ptr<Inlet> load_inlet(DataStreamWriter& wr, QObject* parent)
{
  static auto& il = score::AppComponents().interfaces<Process::PortFactoryList>();
  return std::unique_ptr<Process::Inlet>(
      (Process::Inlet*)deserialize_interface(il, wr, parent));
}

std::unique_ptr<Inlet> load_inlet(JSONWriter& wr, QObject* parent)
{
  static auto& il = score::AppComponents().interfaces<Process::PortFactoryList>();
  return std::unique_ptr<Process::Inlet>(
      (Process::Inlet*)deserialize_interface(il, wr, parent));
}

std::unique_ptr<Outlet> load_outlet(DataStreamWriter& wr, QObject* parent)
{
  static auto& il = score::AppComponents().interfaces<Process::PortFactoryList>();
  return std::unique_ptr<Process::Outlet>(
      (Process::Outlet*)deserialize_interface(il, wr, parent));
}

std::unique_ptr<Outlet> load_outlet(JSONWriter& wr, QObject* parent)
{
  static auto& il = score::AppComponents().interfaces<Process::PortFactoryList>();
  return std::unique_ptr<Process::Outlet>(
      (Process::Outlet*)deserialize_interface(il, wr, parent));
}

static auto copy_port(Port&& src, Port& dst)
{
  dst.displayHandledExplicitly = src.displayHandledExplicitly;
  dst.setName(src.name());
  dst.setAddress(src.address());
  dst.setExposed(src.exposed());
  dst.setDescription(src.description());
  dst.takeCables(std::move(src));
}

template <typename T, typename W>
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
    auto new_p = std::make_unique<T>("Port", out->id(), parent);
    copy_port(std::move(*out), *new_p);
    delete out;
    return new_p;
  }
  else
  {
    // This works with a specific id because it is only for pre 1.0 saves
    return std::make_unique<T>("Port", Id<Process::Port>(0), parent);
  }
}

std::unique_ptr<ValueInlet> load_value_inlet(DataStreamWriter& wr, QObject* parent)
{
  return load_port_t<ValueInlet>(wr, parent);
}

std::unique_ptr<ValueInlet> load_value_inlet(JSONWriter& wr, QObject* parent)
{
  return load_port_t<ValueInlet>(wr, parent);
}

std::unique_ptr<ValueOutlet> load_value_outlet(DataStreamWriter& wr, QObject* parent)
{
  return load_port_t<ValueOutlet>(wr, parent);
}

std::unique_ptr<ValueOutlet> load_value_outlet(JSONWriter& wr, QObject* parent)
{
  return load_port_t<ValueOutlet>(wr, parent);
}

std::unique_ptr<ControlInlet> load_control_inlet(DataStreamWriter& wr, QObject* parent)
{
  return load_port_t<ControlInlet>(wr, parent);
}

std::unique_ptr<ControlInlet> load_control_inlet(JSONWriter& wr, QObject* parent)
{
  return load_port_t<ControlInlet>(wr, parent);
}

std::unique_ptr<ControlOutlet> load_control_outlet(DataStreamWriter& wr, QObject* parent)
{
  return load_port_t<ControlOutlet>(wr, parent);
}

std::unique_ptr<ControlOutlet> load_control_outlet(JSONWriter& wr, QObject* parent)
{
  return load_port_t<ControlOutlet>(wr, parent);
}

std::unique_ptr<AudioInlet> load_audio_inlet(DataStreamWriter& wr, QObject* parent)
{
  return load_port_t<AudioInlet>(wr, parent);
}

std::unique_ptr<AudioInlet> load_audio_inlet(JSONWriter& wr, QObject* parent)
{
  return load_port_t<AudioInlet>(wr, parent);
}

std::unique_ptr<AudioOutlet> load_audio_outlet(DataStreamWriter& wr, QObject* parent)
{
  return load_port_t<AudioOutlet>(wr, parent);
}

std::unique_ptr<AudioOutlet> load_audio_outlet(JSONWriter& wr, QObject* parent)
{
  return load_port_t<AudioOutlet>(wr, parent);
}

std::unique_ptr<MidiInlet> load_midi_inlet(DataStreamWriter& wr, QObject* parent)
{
  return load_port_t<MidiInlet>(wr, parent);
}

std::unique_ptr<MidiInlet> load_midi_inlet(JSONWriter& wr, QObject* parent)
{
  return load_port_t<MidiInlet>(wr, parent);
}

std::unique_ptr<MidiOutlet> load_midi_outlet(DataStreamWriter& wr, QObject* parent)
{
  return load_port_t<MidiOutlet>(wr, parent);
}

std::unique_ptr<MidiOutlet> load_midi_outlet(JSONWriter& wr, QObject* parent)
{
  return load_port_t<MidiOutlet>(wr, parent);
}

QString
displayNameForPort(const Process::Port& outlet, const score::DocumentContext& doc)
{
  // Try to look for a cable
  if(const auto& cables = outlet.cables(); !cables.empty())
  {
    if(Process::Cable* cbl = cables.front().try_find(doc))
      if(Process::Port* inlet = cbl->sink().try_find(doc))
      {
        QString name = inlet->name();
        auto inlet_parent = inlet->parent();
        auto process = qobject_cast<Process::ProcessModel*>(inlet_parent);
        if(process)
        {
          name += " (" + process->prettyName() + ")";
        }
        else if(qobject_cast<Process::Port*>(inlet_parent))
        {
          if(auto process = qobject_cast<Process::ProcessModel*>(inlet_parent->parent()))
          {
            name += " (" + process->prettyName() + ")";
          }
        }
        return name;
      }
  }

  // If no cable, use the address instead
  auto res = outlet.address().toString_unsafe();
  if(!res.isEmpty())
    return res;
  return {};
}
}

template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read(const Process::Port& p)
{
  insertDelimiter();
  m_stream << p.displayHandledExplicitly << p.m_name << p.m_exposed << p.m_description << p.m_address;
  insertDelimiter();
}
template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write(Process::Port& p)
{
  checkDelimiter();
  m_stream >> p.displayHandledExplicitly >> p.m_name >> p.m_exposed >> p.m_description >> p.m_address;
  checkDelimiter();
}

template <>
SCORE_LIB_PROCESS_EXPORT void JSONReader::read(const Process::Port& p)
{
  obj["Hidden"] = (bool)p.displayHandledExplicitly;
  if(!p.m_name.isEmpty())
    obj["Custom"] = p.m_name;
  if(!p.m_exposed.isEmpty())
    obj["Exposed"] = p.m_exposed;
  if(!p.m_description.isEmpty())
    obj["Description"] = p.m_description;
  if(!(p.m_address.address.path.isEmpty() && p.m_address.address.device.isEmpty()
       && p.m_address.qualifiers.get() == ossia::destination_qualifiers{}))
    obj["Address"] = p.m_address;
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write(Process::Port& p)
{
  p.displayHandledExplicitly = obj["Hidden"].toBool();
  if(auto it = obj.tryGet("Custom"))
    p.m_name = it->toString();
  if(auto it = obj.tryGet("Exposed"))
    p.m_exposed = it->toString();
  if(auto it = obj.tryGet("Description"))
    p.m_description = it->toString();
  if(auto it = obj.tryGet("Address"))
    p.m_address <<= *it;
}

template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read(const Process::Inlet& p)
{
  read((Process::Port&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write(Process::Inlet& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void JSONReader::read(const Process::Inlet& p)
{
  read((Process::Port&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write(Process::Inlet& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read(const Process::AudioInlet& p)
{
  // read((Process::Outlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write(Process::AudioInlet& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void JSONReader::read(const Process::AudioInlet& p)
{
  // read((Process::Outlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write(Process::AudioInlet& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read(const Process::MidiInlet& p)
{
  // read((Process::Outlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write(Process::MidiInlet& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void JSONReader::read(const Process::MidiInlet& p)
{
  // read((Process::Outlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write(Process::MidiInlet& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read(const Process::ControlInlet& p)
{
  // read((Process::Inlet&)p);
  readFrom(p.m_value);
  readFrom(p.m_init);
  readFrom(p.m_domain);
}
template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write(Process::ControlInlet& p)
{
  writeTo(p.m_value);
  writeTo(p.m_init);
  writeTo(p.m_domain);
}

template <>
SCORE_LIB_PROCESS_EXPORT void JSONReader::read(const Process::ControlInlet& p)
{
  // read((Process::Inlet&)p);
  obj[strings.Value] = p.m_value;
  obj[strings.Init] = p.m_init;
  obj[strings.Domain] = p.m_domain;
}

template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write(Process::ControlInlet& p)
{
  p.m_value <<= obj[strings.Value];
  if(auto v = obj.tryGet(strings.Init))
    p.m_init <<= *v;
  p.m_domain <<= obj[strings.Domain];
}

template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read(const Process::Outlet& p)
{
  read((Process::Port&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write(Process::Outlet& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void JSONReader::read(const Process::Outlet& p)
{
  read((Process::Port&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write(Process::Outlet& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read(const Process::AudioOutlet& p)
{
  // read((Process::Outlet&)p);
  m_stream << *p.gainInlet << *p.panInlet << p.m_gain
           << static_cast<const ossia::small_vector<double, 2>&>(p.m_pan)
           << p.m_propagate;
}
template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write(Process::AudioOutlet& p)
{
  p.gainInlet = Process::load_control_inlet(*this, &p);
  p.panInlet = Process::load_control_inlet(*this, &p);
  m_stream >> p.m_gain >> static_cast<ossia::small_vector<double, 2>&>(p.m_pan)
      >> p.m_propagate;
}

template <>
SCORE_LIB_PROCESS_EXPORT void JSONReader::read(const Process::AudioOutlet& p)
{
  // read((Process::Outlet&)p);
  obj["GainInlet"] = *p.gainInlet;
  obj["PanInlet"] = *p.panInlet;

  obj["Gain"] = p.m_gain;
  obj["Pan"] = static_cast<const ossia::small_vector<double, 2>&>(p.m_pan);
  obj["Propagate"] = p.m_propagate;
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write(Process::AudioOutlet& p)
{
  if(auto it = obj.tryGet("GainInlet"))
  {
    JSONWriter writer{*it};
    p.gainInlet = Process::load_control_inlet(writer, &p);
  }
  else
  {
    p.gainInlet
        = std::make_unique<Process::ControlInlet>("Gain", Id<Process::Port>{0}, &p);
  }

  if(auto it = obj.tryGet("PanInlet"))
  {
    JSONWriter writer{*it};
    p.panInlet = Process::load_control_inlet(writer, &p);
  }
  else
  {
    p.panInlet
        = std::make_unique<Process::ControlInlet>("Pan", Id<Process::Port>{1}, &p);
  }

  p.m_gain = obj["Gain"].toDouble();
  static_cast<ossia::small_vector<double, 2>&>(p.m_pan) <<= obj["Pan"];
  p.m_propagate = obj["Propagate"].toBool();
}

template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read(const Process::MidiOutlet& p)
{
  // read((Process::Outlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write(Process::MidiOutlet& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void JSONReader::read(const Process::MidiOutlet& p)
{
  // read((Process::Outlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write(Process::MidiOutlet& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read(const Process::ControlOutlet& p)
{
  // read((Process::Outlet&)p);
  readFrom(p.m_value);
  readFrom(p.m_domain);
}
template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write(Process::ControlOutlet& p)
{
  writeTo(p.m_value);
  writeTo(p.m_domain);
}

template <>
SCORE_LIB_PROCESS_EXPORT void JSONReader::read(const Process::ControlOutlet& p)
{
  // read((Process::Outlet&)p);
  obj[strings.Value] = p.m_value;
  obj[strings.Domain] = p.m_domain;
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write(Process::ControlOutlet& p)
{
  p.m_value <<= obj[strings.Value];
  p.m_domain <<= obj[strings.Domain];
}

template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read(const Process::ValueInlet& p)
{
  // read((Process::Outlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write(Process::ValueInlet& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void JSONReader::read(const Process::ValueInlet& p)
{
  // read((Process::Outlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write(Process::ValueInlet& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read(const Process::ValueOutlet& p)
{
  // read((Process::Outlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write(Process::ValueOutlet& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void JSONReader::read(const Process::ValueOutlet& p)
{
  // read((Process::Outlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write(Process::ValueOutlet& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read(const Process::pan_weight& p)
{
  this->m_stream << static_cast<const ossia::small_vector<double, 2>&>(p);
  // read((Process::Outlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write(Process::pan_weight& p)
{
  this->m_stream >> static_cast<ossia::small_vector<double, 2>&>(p);
}
