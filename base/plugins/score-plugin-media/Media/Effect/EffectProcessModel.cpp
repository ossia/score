#include <Media/Effect/EffectProcessModel.hpp>
#include <Media/Effect/Effect/EffectModel.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
//#include <Media/MediaStreamEngine/MediaDocumentPlugin.hpp>

#include <score/tools/Clamp.hpp>
#include <QFile>
#include <Process/Dataflow/Port.hpp>

namespace Media
{
namespace Effect
{

ProcessModel::ProcessModel(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent):
  Process::ProcessModel{duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
, inlet{Process::make_inlet(Id<Process::Port>(0), this)}
, outlet{Process::make_outlet(Id<Process::Port>(0), this)}
{
  metadata().setInstanceName(*this);
  inlet->type = Process::PortType::Audio;

  outlet->setPropagate(true);
  outlet->type = Process::PortType::Audio;
}

ProcessModel::ProcessModel(
    const ProcessModel& source,
    const Id<Process::ProcessModel>& id,
    QObject* parent):
  Process::ProcessModel{
    source,
    id,
    Metadata<ObjectKey_k, ProcessModel>::get(),
    parent}
, inlet{Process::clone_inlet(*source.inlet, this)}
, outlet{Process::clone_outlet(*source.outlet, this)}
{
  metadata().setInstanceName(*this);
  for(const auto& fx : source.effects())
  {
    auto eff = fx.clone(fx.id(), this);
    m_effects.add(eff);
  }

  m_effectOrder = source.m_effectOrder;
}

ProcessModel::~ProcessModel()
{
  // TODO delete components
}

void ProcessModel::insertEffect(
    EffectModel* eff,
    int pos)
{
  // Check that the effect order makes sense.
  const Process::Inlets& inlets = eff->inlets();
  const Process::Outlets& outlets = eff->outlets();
  if(inlets.empty() || outlets.empty())
  {
    qDebug() << "invalid effect! no inlets or outlets";
    delete eff;
    return;
  }
  clamp(pos, 0, int(m_effectOrder.size()));

  m_effects.add(eff);
  m_effectOrder.insert(pos, eff->id());

  if(pos > 0)
  {
    if(inlets[0]->type != m_effects.at(m_effectOrder[pos - 1]).outlets()[0]->type)
    {
      qDebug() << "invalid effect! (bad chaining before)";
      m_effects.remove(eff);
      m_effectOrder.removeAt(pos);
      return;
    }
  }
  if(m_effects.size() > 0 && pos < m_effects.size() - 1)
  {
    if(outlets[0]->type != m_effects.at(m_effectOrder[pos + 1]).inlets()[0]->type)
    {
      qDebug() << "invalid effect! (bad chaining after)";
      m_effects.remove(eff);
      m_effectOrder.removeAt(pos);
      return;
    }

  }

  if(pos == 0)
  {
    if(inlets[0]->type != this->inlet->type)
    {
      this->inlet->type = inlets[0]->type;
      emit inletsChanged();
    }
  }
  if(pos == m_effects.size() - 1)
  {
    if(outlets[0]->type != this->outlet->type)
    {
      this->outlet->type = outlets[0]->type;
      emit outletsChanged();
    }
  }

  emit effectsChanged();
}

void ProcessModel::removeEffect(const Id<EffectModel>& e)
{
  m_effectOrder.removeAll(e);

  m_effects.remove(e);

  emit effectsChanged();
}

void ProcessModel::moveEffect(const Id<EffectModel>& e, int new_pos)
{
  new_pos = clamp(new_pos, 0, m_effectOrder.size() - 1);
  auto old_pos = m_effectOrder.indexOf(e);
  if(old_pos != -1)
  {
    m_effectOrder.move(old_pos, new_pos);
    emit effectsChanged();
  }
}

int ProcessModel::effectPosition(const Id<EffectModel>& e) const
{
  return m_effectOrder.indexOf(e);
}

}

}

template <>
void DataStreamReader::read(const Media::Effect::ProcessModel& proc)
{
  m_stream << *proc.inlet << *proc.outlet;

  int32_t n = proc.effects().size();
  m_stream << n;
  for(auto& eff : proc.effects())
    readFrom(eff);

  m_stream << proc.effectsOrder();
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Media::Effect::ProcessModel& proc)
{
  proc.inlet = Process::make_inlet(*this, &proc);
  proc.outlet = Process::make_outlet(*this, &proc);

  int32_t n = 0;
  m_stream >> n;

  auto& fxs = components.interfaces<Media::Effect::EffectFactoryList>();
  for(int i = 0; i < n ; i++)
  {
    auto fx = deserialize_interface(fxs, *this, &proc);
    if(fx)
      proc.insertEffect(fx, i++);
    else
      SCORE_TODO;
  }

  proc.m_effectOrder.clear();
  m_stream >> proc.m_effectOrder;

  checkDelimiter();
}

template <>
void JSONObjectReader::read(const Media::Effect::ProcessModel& proc)
{
  obj["Inlet"] = toJsonObject(*proc.inlet);
  obj["Outlet"] = toJsonObject(*proc.outlet);
  obj["Effects"] = toJsonArray(proc.effects());
  obj["Order"] = toJsonArray(proc.effectsOrder());
}

template <>
void JSONObjectWriter::write(Media::Effect::ProcessModel& proc)
{
  {
    JSONObjectWriter writer{obj["Inlet"].toObject()};
    proc.inlet = Process::make_inlet(writer, &proc);
  }
  {
    JSONObjectWriter writer{obj["Outlet"].toObject()};
    proc.outlet = Process::make_outlet(writer, &proc);
  }
  QJsonArray fx_array = obj["Effects"].toArray();
  auto& fxs = components.interfaces<Media::Effect::EffectFactoryList>();
  int i = 0;
  for(const auto& json_vref : fx_array)
  {
    JSONObject::Deserializer deserializer{json_vref.toObject()};
    auto fx = deserialize_interface(fxs, deserializer, &proc);
    if(fx)
      proc.insertEffect(fx, i++);
    else
      SCORE_TODO;
  }

  proc.m_effectOrder.clear();
  fromJsonValueArray(obj["Order"].toArray(), proc.m_effectOrder);
}

