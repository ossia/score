#include <Media/Effect/EffectProcessModel.hpp>
#include <Process/Process.hpp>
#include <Effect/EffectFactory.hpp>

#include <score/tools/Clamp.hpp>
#include <QFile>
#include <Process/Dataflow/Port.hpp>
#include <Process/ProcessList.hpp>

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
  init();
}


ProcessModel::~ProcessModel()
{
  m_effects.clear();
}

void ProcessModel::insertEffect(
    Process::ProcessModel* eff,
    int pos)
{
  bool bad_effect = false;
  // Check that the effect order makes sense.
  const Process::Inlets& inlets = eff->inlets();
  const Process::Outlets& outlets = eff->outlets();
  if(inlets.empty() || outlets.empty())
  {
    bad_effect = true;
  }
  clamp(pos, 0, int(m_effects.size()));

  if(pos > 0)
  {
    if(inlets[0]->type != m_effects.at_pos(pos - 1).outlets()[0]->type)
    {
      bad_effect = true;
    }
  }
  if(m_effects.size() > 0 && pos < (int)m_effects.size())
  {
    if(outlets[0]->type != m_effects.at_pos(pos).inlets()[0]->type)
    {
      bad_effect = true;
    }
  }

  m_effects.insert_at(pos, eff);
  connect(eff, &Process::ProcessModel::inletsChanged,
          this, &ProcessModel::checkChaining);
  connect(eff, &Process::ProcessModel::outletsChanged,
          this, &ProcessModel::checkChaining);

  if(pos == 0)
  {
    if(inlets[0]->type != this->inlet->type)
    {
      this->inlet->type = inlets[0]->type;
      inletsChanged();
    }
  }
  if(pos == (int)m_effects.size() - 1)
  {
    if(outlets[0]->type != this->outlet->type)
    {
      this->outlet->type = outlets[0]->type;
      outletsChanged();
    }
  }

  setBadChaining(bad_effect);

  effectsChanged();
}

void ProcessModel::removeEffect(const Id<Process::ProcessModel>& e)
{
  m_effects.remove(e);
  // TODO adjust and check ports
  // TODO introduce a dummy effect if the ports don't match
  effectsChanged();
}

void ProcessModel::moveEffect(const Id<Process::ProcessModel>& e, int new_pos)
{
  if(m_effects.size() == 0)
    new_pos = 0;
  else
    new_pos = clamp(new_pos, 0, (int)m_effects.size() - 1);

  auto old_pos = effectPosition(e);
  if(old_pos != -1)
  {
    m_effects.move(e, new_pos);
    effectsChanged();
  }
}

int ProcessModel::effectPosition(const Id<Process::ProcessModel>& e) const
{
  return (int)m_effects.index(e);
}

void ProcessModel::checkChaining()
{
  int pos = 0;
  bool bad_effect = false;
  for(auto& eff : m_effects)
  {
    const Process::Inlets& inlets = eff.inlets();
    const Process::Outlets& outlets = eff.outlets();

    if(inlets.empty() || outlets.empty())
    {
      bad_effect = true;
    }

    if(pos > 0)
    {
      auto& prev_outlets = m_effects.at_pos(pos - 1).outlets();
      if(!inlets.empty() && !prev_outlets.empty())
      {
        if(inlets[0]->type != prev_outlets[0]->type)
        {
          bad_effect = true;
        }
      }

    }
    if(m_effects.size() > 0 && (pos + 1) < (int)m_effects.size())
    {
      auto& next_inlets = m_effects.at_pos(pos + 1).inlets();
      if(!outlets.empty() && !next_inlets.empty())
      {
        if(outlets[0]->type != next_inlets[0]->type)
        {
          bad_effect = true;
        }
      }
    }

    if(pos == 0)
    {
      if(!inlets.empty() && inlets[0]->type != this->inlet->type)
      {
        this->inlet->type = inlets[0]->type;
        inletsChanged();
      }
    }
    if(pos == (int)m_effects.size() - 1)
    {
      if(!outlets.empty() && outlets[0]->type != this->outlet->type)
      {
        this->outlet->type = outlets[0]->type;
        outletsChanged();
      }
    }

    pos++;
  }

  setBadChaining(bad_effect);

}

}

}

template <>
void DataStreamReader::read(const Media::Effect::ProcessModel& proc)
{
  m_stream << *proc.inlet << *proc.outlet;

  int32_t n = (int)proc.effects().size();
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

  auto& fxs = components.interfaces<Process::ProcessFactoryList>();
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
  auto& fxs = components.interfaces<Process::ProcessFactoryList>();
  int i = 0;
  for(const auto& json_vref : fx_array)
  {
    JSONObject::Deserializer deserializer{json_vref.toObject()};
    auto fx = deserialize_interface(fxs, deserializer, &proc);
    if(fx)
    {
      std::size_t pos = i++;

      proc.m_effects.add(fx);

      if(pos == 0)
      {
        if(fx->inlets()[0]->type != proc.inlet->type)
        {
          proc.inlet->type = fx->inlets()[0]->type;
          proc.inletsChanged();
        }
      }
      if(pos == proc.m_effects.size() - 1)
      {
        if(fx->outlets()[0]->type != proc.outlet->type)
        {
          proc.outlet->type = fx->outlets()[0]->type;
          proc.outletsChanged();
        }
      }

      proc.effectsChanged();
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
  for(auto& c : effects())
    if(c.selection.get())
      s.append(&c);
  return s;
}

void Media::Effect::ProcessModel::setSelection(const Selection& s) const
{
  for(auto& c : effects())
  {
    c.selection.set(s.contains(&c));
  }
}
