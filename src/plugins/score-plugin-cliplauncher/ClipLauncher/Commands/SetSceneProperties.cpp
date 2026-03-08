#include "SetSceneProperties.hpp"

#include <score/model/path/PathSerialization.hpp>

#include <ClipLauncher/ProcessModel.hpp>
#include <ClipLauncher/SceneModel.hpp>

namespace ClipLauncher
{

SetSceneName::SetSceneName(
    const ProcessModel& proc, const SceneModel& scene, QString newName)
    : m_path{proc}
    , m_sceneId{scene.id()}
    , m_old{scene.name()}
    , m_new{std::move(newName)}
{
}

void SetSceneName::undo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).scenes.at(m_sceneId).setName(m_old);
}

void SetSceneName::redo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).scenes.at(m_sceneId).setName(m_new);
}

void SetSceneName::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_sceneId << m_old << m_new;
}

void SetSceneName::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_sceneId >> m_old >> m_new;
}

} // namespace ClipLauncher
