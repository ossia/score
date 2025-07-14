// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Process.hpp"

#include <Process/CodeWriter.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/ExpandMode.hpp>
#include <Process/PresetHelpers.hpp>
#include <Process/TimeValue.hpp>

#include <LocalTree/ProcessComponent.hpp>

#include <score/model/ComponentUtils.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/SetIcons.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/disable_fpe.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>

#include <QObject>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Process::ProcessModel)

#if !defined(SCORE_ALL_UNITY)
template class IdentifiedObject<Process::ProcessModel>;
template class score::SerializableInterface<Process::ProcessModelFactory>;
#endif
namespace Process
{

const QIcon& getCategoryIcon(const QString& category) noexcept
{
  static const ossia::flat_map<QString, QIcon> categoryIcon{
      {"AI", makeIcon(QStringLiteral(":/icons/ai.png"))},
      {"Analysis", makeIcon(QStringLiteral(":/icons/analysis.png"))},
      {"Audio", makeIcon(QStringLiteral(":/icons/audio.png"))},
      {"Plugins", makeIcon(QStringLiteral(":/icons/filter.png"))},
      {"Midi", makeIcon(QStringLiteral(":/icons/midi.png"))},
      {"Control", makeIcon(QStringLiteral(":/icons/controls.png"))},
      {"Visuals", makeIcon(QStringLiteral(":/icons/gfx.png"))},
      {"Automations", makeIcon(QStringLiteral(":/icons/automation.png"))},
      {"Impro", makeIcon(QStringLiteral(":/icons/controls.png"))},
      {"Script", makeIcon(QStringLiteral(":/icons/script.png"))},
      {"Structure", makeIcon(QStringLiteral(":/icons/structure.png"))},
      {"Network", makeIcon(QStringLiteral(":/icons/sync.png"))},
      {"Monitoring", makeIcon(QStringLiteral(":/icons/ui.png"))},
      {"Spatial", makeIcon(QStringLiteral(":/icons/spatial.png"))},
  };
  static const QIcon invalid;
  if(auto it = categoryIcon.find(category); it != categoryIcon.end())
  {
    return it->second;
  }
  return invalid;
}
ProcessModel::ProcessModel(
    TimeVal duration, const Id<ProcessModel>& id, const QString& name, QObject* parent)
    : Entity{id, name, parent}
    , m_duration{std::move(duration)}
    , m_slotHeight{300}
    , m_loopDuration{m_duration}
    , m_size{200, 100}
    , m_loops{false}
{
  ossia::disable_fpe();
  con(metadata(), &score::ModelMetadata::NameChanged, this,
      [this] { prettyNameChanged(); });
  connect(this, &Process::ProcessModel::resetExecution, this, [this] {
    for(auto& p : this->m_inlets)
      p->executionReset();
    for(auto& p : this->m_outlets)
      p->executionReset();
  });
  // metadata().setInstanceName(*this);
}

ProcessModel::~ProcessModel()
{
  identified_object_destroying(this);
}

QString ProcessModel::effect() const noexcept
{
  return {};
}

void ProcessModel::setDurationAndScale(const TimeVal& newDuration) noexcept
{
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

ProcessModel::ProcessModel(DataStream::Deserializer& vis, QObject* parent)
    : Entity(vis, parent)
{
  ossia::disable_fpe();
  vis.writeTo(*this);
  con(metadata(), &score::ModelMetadata::NameChanged, this,
      [this] { prettyNameChanged(); });
}

ProcessModel::ProcessModel(JSONObject::Deserializer& vis, QObject* parent)
    : Entity(vis, parent)
{
  ossia::disable_fpe();
  vis.writeTo(*this);
  con(metadata(), &score::ModelMetadata::NameChanged, this,
      [this] { prettyNameChanged(); });
}

QString ProcessModel::prettyName() const noexcept
{
  return metadata().getName();
}

std::unique_ptr<CodeWriter> ProcessModel::codeWriter(CodeFormat) const noexcept
{
  return std::make_unique<DummyCodeWriter>(*this);
}

void ProcessModel::setParentDuration(ExpandMode mode, const TimeVal& t) noexcept
{
  switch(mode)
  {
    case ExpandMode::Scale:
      setDurationAndScale(t);
      break;
    case ExpandMode::GrowShrink: {
      if(duration() < t)
        setDurationAndGrow(t);
      else
        setDurationAndShrink(t);
      break;
    }
    case ExpandMode::ForceGrow: {
      if(duration() < t)
        setDurationAndGrow(t);
      break;
    }
    case ExpandMode::CannotExpand:
    default:
      break;
  }
}

TimeVal ProcessModel::contentDuration() const noexcept
{
  return TimeVal::zero();
}

void ProcessModel::setDuration(const TimeVal& other) noexcept
{
  m_duration = other;
  durationChanged(m_duration);
}

const TimeVal& ProcessModel::duration() const noexcept
{
  return m_duration;
}

ProcessStateDataInterface* ProcessModel::startStateData() const noexcept
{
  return nullptr;
}

ProcessStateDataInterface* ProcessModel::endStateData() const noexcept
{
  return nullptr;
}

Selection ProcessModel::selectableChildren() const noexcept
{
  return {};
}

Selection ProcessModel::selectedChildren() const noexcept
{
  return {};
}

void ProcessModel::setSelection(const Selection& s) const noexcept
{
  // OPTIMIZEME
  auto cld = this->findChildren<Selectable*>();
  for(Selectable* child : cld)
  {
    child->set(s.contains(child->parent()));
  }
}

Process::Inlet* ProcessModel::inlet(const Id<Process::Port>& p) const noexcept
{
  for(auto e : m_inlets)
    if(e->id() == p)
      return e;
  return nullptr;
}

Process::Outlet* ProcessModel::outlet(const Id<Process::Port>& p) const noexcept
{
  for(auto e : m_outlets)
    if(e->id() == p)
      return e;
  return nullptr;
}

void ProcessModel::loadPreset(const Preset& preset)
{
  const rapidjson::Document doc = readJson(preset.data);
  loadFixedControls(doc.GetArray(), *this);
}

Preset ProcessModel::savePreset() const noexcept
{
  Preset p;
  p.name = this->metadata().getName();
  p.key.key = this->concreteKey();

  JSONReader r;
  saveFixedControls(r, *this);
  p.data = r.toByteArray();
  return p;
}

std::vector<Preset> ProcessModel::builtinPresets() const noexcept
{
  return {};
}

void ProcessModel::ancestorStartDateChanged() { }

void ProcessModel::ancestorTempoChanged() { }

void ProcessModel::forEachControl(
    smallfun::function<void(ControlInlet&, const ossia::value&)> f) const
{
  for(const auto& inlet : m_inlets)
  {
    if(auto ctrl = qobject_cast<Process::ControlInlet*>(inlet))
    {
      f(*ctrl, ctrl->value());
    }
  }
}

void ProcessModel::setCreatingControls(bool ok) { }

bool ProcessModel::creatingControls() const noexcept
{
  return this->flags() & ProcessFlags::CreateControls;
}

void ProcessModel::setLoops(bool b)
{
  if(b != m_loops)
  {
    m_loops = b;
    loopsChanged(b);
  }
}

void ProcessModel::setStartOffset(TimeVal b)
{
  if(b != m_startOffset)
  {
    m_startOffset = b;
    startOffsetChanged(b);
  }
}

void ProcessModel::setLoopDuration(TimeVal b)
{
  if(b.msec() < 0.1)
    b = TimeVal::fromMsecs(0.1);

  if(b != m_loopDuration)
  {
    m_loopDuration = b;
    loopDurationChanged(b);
  }
}

QPointF ProcessModel::position() const noexcept
{
  return m_position;
}

QSizeF ProcessModel::size() const noexcept
{
  return m_size;
}

void ProcessModel::setPosition(const QPointF& v)
{
  if(v != m_position)
  {
    m_position = v;
    positionChanged(v);
  }
}

void ProcessModel::setSize(const QSizeF& v)
{
  if(v != m_size)
  {
    m_size = v;
    sizeChanged(v);
  }
}

double ProcessModel::getSlotHeight() const noexcept
{
  return m_slotHeight;
}

void ProcessModel::setSlotHeight(double v) noexcept
{
  m_slotHeight = v;
  slotHeightChanged(v);
}

std::optional<Process::MagneticInfo>
ProcessModel::magneticPosition(const QObject* o, const TimeVal t) const noexcept
{
  return {};
}

ProcessModel* parentProcess(QObject* obj) noexcept
{
  if(obj)
    obj = obj->parent();

  while(obj && !qobject_cast<ProcessModel*>(obj))
  {
    obj = obj->parent();
  }

  if(obj)
    return static_cast<ProcessModel*>(obj);
  return nullptr;
}

const ProcessModel* parentProcess(const QObject* obj) noexcept
{
  if(obj)
    obj = obj->parent();

  while(obj && !qobject_cast<const ProcessModel*>(obj))
  {
    obj = obj->parent();
  }

  if(obj)
    return static_cast<const ProcessModel*>(obj);
  return nullptr;
}

QString processLocalTreeAddress(const ProcessModel& proc)
{
  if(auto lt = findComponent<LocalTree::ProcessComponent>(proc.components()))
  {
    return QString::fromStdString(
        lt->node().get_device().get_name() + ":" + lt->node().osc_address());
  }
  return {};
}
}
