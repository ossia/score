#include "SceneModel.hpp"

#include <score/model/EntitySerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

W_OBJECT_IMPL(ClipLauncher::SceneModel)

namespace ClipLauncher
{

SceneModel::SceneModel(const Id<SceneModel>& id, QObject* parent)
    : score::Entity<SceneModel>{id, "Scene", parent}
{
}

SceneModel::~SceneModel() { }

void SceneModel::setName(const QString& n)
{
  if(m_name != n)
  {
    m_name = n;
    nameChanged(n);
  }
}

} // namespace ClipLauncher

template <>
void DataStreamReader::read(const ClipLauncher::SceneModel& scene)
{
  m_stream << scene.m_name;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(ClipLauncher::SceneModel& scene)
{
  m_stream >> scene.m_name;
  checkDelimiter();
}

template <>
void JSONReader::read(const ClipLauncher::SceneModel& scene)
{
  obj["Name"] = scene.m_name;
}

template <>
void JSONWriter::write(ClipLauncher::SceneModel& scene)
{
  scene.m_name = obj["Name"].toString();
}
