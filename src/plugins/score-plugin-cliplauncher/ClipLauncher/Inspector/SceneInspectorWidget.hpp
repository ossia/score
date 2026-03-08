#pragma once
#include <Inspector/InspectorWidgetBase.hpp>

namespace ClipLauncher
{
class SceneModel;
class ProcessModel;

class SceneInspectorWidget final : public Inspector::InspectorWidgetBase
{
public:
  SceneInspectorWidget(
      const SceneModel& scene, const score::DocumentContext& ctx, QWidget* parent);
  ~SceneInspectorWidget();

private:
  const ProcessModel& parentProcess() const;
  const SceneModel& m_scene;
  const score::DocumentContext& m_ctx;
};

} // namespace ClipLauncher
