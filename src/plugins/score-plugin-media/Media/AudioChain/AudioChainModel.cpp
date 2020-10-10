#include <Media/AudioChain/AudioChainModel.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>

#include <score/model/EntityMapSerialization.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>

#include <Effect/EffectFactory.hpp>
#include <wobjectimpl.h>

W_OBJECT_IMPL(Media::AudioChain::ProcessModel)

namespace Media::AudioChain
{
ProcessModel::ProcessModel(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    const score::DocumentContext& ctx,
    QObject* parent)
    : ChainProcess{duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), ctx, parent}
    , inlet{Process::make_audio_inlet(Id<Process::Port>(0), this)}
    , outlet{Process::make_audio_outlet(Id<Process::Port>(0), this)}
{
  metadata().setInstanceName(*this);

  outlet->setPropagate(true);
  init();
}

ProcessModel::~ProcessModel() { }

}

template <>
void DataStreamReader::read(const Media::AudioChain::ProcessModel& proc)
{
  m_stream << *proc.inlet << *proc.outlet;

  int32_t n = (int)proc.effects().size();
  m_stream << n;
  for (auto& eff : proc.effects())
  {
    readFrom(eff);
  }
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Media::AudioChain::ProcessModel& proc)
{
  proc.inlet = Process::load_audio_inlet(*this, &proc);
  proc.outlet = Process::load_audio_outlet(*this, &proc);

  int32_t n = 0;
  m_stream >> n;

  auto& fxs = components.interfaces<Process::ProcessFactoryList>();
  for (int i = 0; i < n; i++)
  {
    auto fx = deserialize_interface(fxs, *this, proc.context(), &proc);
    if (fx)
      proc.insertEffect(fx, i);
    else
      SCORE_TODO;
  }

  checkDelimiter();
}

template <>
void JSONReader::read(const Media::AudioChain::ProcessModel& proc)
{
  obj["Inlet"] = *proc.inlet;
  obj["Outlet"] = *proc.outlet;
  obj["Effects"] = proc.effects();
}

template <>
void JSONWriter::write(Media::AudioChain::ProcessModel& proc)
{
  {
    JSONWriter writer{obj["Inlet"]};
    proc.inlet = Process::load_audio_inlet(writer, &proc);
  }
  {
    JSONWriter writer{obj["Outlet"]};
    proc.outlet = Process::load_audio_outlet(writer, &proc);
  }

  const auto& fx_array = obj["Effects"].toArray();
  auto& fxs = components.interfaces<Process::ProcessFactoryList>();

  for (const auto& json_vref : fx_array)
  {
    JSONObject::Deserializer deserializer{json_vref};
    auto fx = deserialize_interface(fxs, deserializer, proc.context(), &proc);
    if (fx)
    {
      proc.m_effects.add(fx);
    }
    else
      SCORE_TODO;
  }
}
