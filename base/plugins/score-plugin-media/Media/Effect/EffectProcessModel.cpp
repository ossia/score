#include <Media/Effect/EffectProcessModel.hpp>
#include <Effect/EffectModel.hpp>
#include <Effect/EffectFactory.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

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


ProcessModel::~ProcessModel()
{
  m_effects.clear();
}

void ProcessModel::insertEffect(
    Process::EffectModel* eff,
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
  clamp(pos, 0, int(m_effects.size()));

  if(pos > 0)
  {
    if(inlets[0]->type != m_effects.at_pos(pos - 1).outlets()[0]->type)
    {
      qDebug() << "invalid effect! (bad chaining before)";
      return;
    }
  }
  if(m_effects.size() > 0 && pos < m_effects.size())
  {
    if(outlets[0]->type != m_effects.at_pos(pos).inlets()[0]->type)
    {
      qDebug() << "invalid effect! (bad chaining after)";
      return;
    }
  }

  m_effects.insert_at(pos, eff);


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

void ProcessModel::removeEffect(const Id<Process::EffectModel>& e)
{
  m_effects.remove(e);
  // TODO adjust and check ports
  // TODO introduce a dummy effect if the ports don't match
  emit effectsChanged();
}

void ProcessModel::moveEffect(const Id<Process::EffectModel>& e, int new_pos)
{
  new_pos = clamp(new_pos, 0, m_effects.size() - 1);
  auto old_pos = effectPosition(e);
  if(old_pos != -1)
  {
    m_effects.move(e, new_pos);
    emit effectsChanged();
  }
}

int ProcessModel::effectPosition(const Id<Process::EffectModel>& e) const
{
  return m_effects.index(e);
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
  {
    readFrom(eff);
  }
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Media::Effect::ProcessModel& proc)
{
  proc.inlet = Process::make_inlet(*this, &proc);
  proc.outlet = Process::make_outlet(*this, &proc);

  int32_t n = 0;
  m_stream >> n;

  auto& fxs = components.interfaces<Process::EffectFactoryList>();
  for(int i = 0; i < n ; i++)
  {
    auto fx = deserialize_interface(fxs, *this, &proc);
    if(fx)
      proc.insertEffect(fx, i);
    else
      SCORE_TODO;
  }

  checkDelimiter();
}

template <>
void JSONObjectReader::read(const Media::Effect::ProcessModel& proc)
{
  obj["Inlet"] = toJsonObject(*proc.inlet);
  obj["Outlet"] = toJsonObject(*proc.outlet);
  obj["Effects"] = toJsonArray(proc.effects());
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

  const QJsonArray fx_array = obj["Effects"].toArray();
  auto& fxs = components.interfaces<Process::EffectFactoryList>();
  int i = 0;
  for(const auto& json_vref : fx_array)
  {
    JSONObject::Deserializer deserializer{json_vref.toObject()};
    Process::EffectModel* fx = deserialize_interface(fxs, deserializer, &proc);
    if(fx)
    {
      auto pos = i++;

      proc.m_effects.add(fx);

      if(pos == 0)
      {
        if(fx->inlets()[0]->type != proc.inlet->type)
        {
          proc.inlet->type = fx->inlets()[0]->type;
          emit proc.inletsChanged();
        }
      }
      if(pos == proc.m_effects.size() - 1)
      {
        if(fx->outlets()[0]->type != proc.outlet->type)
        {
          proc.outlet->type = fx->outlets()[0]->type;
          emit proc.outletsChanged();
        }
      }

      emit proc.effectsChanged();
    }
    else
      SCORE_TODO;
  }
}


Selection Media::Effect::ProcessModel::selectableChildren() const
{
  Selection s;
  for(auto& c : effects())
    s.append(&c);
  return s;
}

Selection Media::Effect::ProcessModel::selectedChildren() const
{
  Selection s;
  for(Process::EffectModel& c : effects())
    if(c.selection.get())
      s.append(&c);
  return s;
}

void Media::Effect::ProcessModel::setSelection(const Selection& s) const
{
  for(Process::EffectModel& c : effects())
  {
    c.selection.set(s.contains(&c));
  }
}
