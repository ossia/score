#include "LaneModel.hpp"

#include <score/model/EntitySerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

W_OBJECT_IMPL(ClipLauncher::LaneModel)

namespace ClipLauncher
{

LaneModel::LaneModel(const Id<LaneModel>& id, QObject* parent)
    : score::Entity<LaneModel>{id, "Lane", parent}
{
}

LaneModel::~LaneModel() { }

void LaneModel::setName(const QString& n)
{
  if(m_name != n)
  {
    m_name = n;
    nameChanged(n);
  }
}

void LaneModel::setExclusivityMode(ExclusivityMode m)
{
  if(m_exclusivityMode != m)
  {
    m_exclusivityMode = m;
    exclusivityModeChanged(m);
  }
}

void LaneModel::setTemporalMode(TemporalMode m)
{
  if(m_temporalMode != m)
  {
    m_temporalMode = m;
    temporalModeChanged(m);
  }
}

void LaneModel::setCrossfadeDuration(double d)
{
  if(m_crossfadeDuration != d)
  {
    m_crossfadeDuration = d;
    crossfadeDurationChanged(d);
  }
}

void LaneModel::setVolume(double v)
{
  if(m_volume != v)
  {
    m_volume = v;
    volumeChanged(v);
  }
}

} // namespace ClipLauncher

template <>
void DataStreamReader::read(const ClipLauncher::LaneModel& lane)
{
  m_stream << lane.m_name << lane.m_exclusivityMode << lane.m_temporalMode
           << lane.m_crossfadeDuration << lane.m_volume;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(ClipLauncher::LaneModel& lane)
{
  m_stream >> lane.m_name >> lane.m_exclusivityMode >> lane.m_temporalMode
      >> lane.m_crossfadeDuration >> lane.m_volume;
  checkDelimiter();
}

template <>
void JSONReader::read(const ClipLauncher::LaneModel& lane)
{
  obj["Name"] = lane.m_name;
  obj["ExclusivityMode"] = static_cast<int>(lane.m_exclusivityMode);
  obj["TemporalMode"] = static_cast<int>(lane.m_temporalMode);
  obj["CrossfadeDuration"] = lane.m_crossfadeDuration;
  obj["Volume"] = lane.m_volume;
}

template <>
void JSONWriter::write(ClipLauncher::LaneModel& lane)
{
  lane.m_name = obj["Name"].toString();
  lane.m_exclusivityMode
      = static_cast<ClipLauncher::ExclusivityMode>(obj["ExclusivityMode"].toInt());
  lane.m_temporalMode
      = static_cast<ClipLauncher::TemporalMode>(obj["TemporalMode"].toInt());
  lane.m_crossfadeDuration = obj["CrossfadeDuration"].toDouble();
  lane.m_volume = obj["Volume"].toDouble();
}
