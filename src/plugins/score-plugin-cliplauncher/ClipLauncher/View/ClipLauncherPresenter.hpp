#pragma once
#include <Process/LayerPresenter.hpp>

#include <verdigris>

class QTimer;

namespace ClipLauncher
{
class CellModel;
class LaneModel;
class SceneModel;
class ProcessModel;
class ClipLauncherView;

namespace Execution
{
class ClipLauncherComponent;
}

class ClipLauncherPresenter final : public Process::LayerPresenter
{
  W_OBJECT(ClipLauncherPresenter)
public:
  explicit ClipLauncherPresenter(
      const ProcessModel& model, ClipLauncherView* view,
      const Process::Context& ctx, QObject* parent);
  ~ClipLauncherPresenter() override;

  void setWidth(qreal width, qreal defaultWidth) override;
  void setHeight(qreal height) override;
  void putToFront() override;
  void putBehind() override;
  void on_zoomRatioChanged(ZoomRatio val) override;
  void parentGeometryChanged() override;
  void fillContextMenu(
      QMenu& menu, QPoint pos, QPointF scenepos,
      const Process::LayerContextMenuManager& cm) override;

  const ProcessModel& model() const noexcept;

private:
  Execution::ClipLauncherComponent* executionComponent() const;

  void on_cellClicked(int lane, int scene);
  void on_cellDoubleClicked(int lane, int scene);
  void on_sceneLaunchClicked(int scene);
  void on_dropOnCell(int lane, int scene, const QMimeData& mime);
  void on_laneHeaderClicked(int lane);
  void on_sceneHeaderClicked(int scene);
  void on_cellChanged(const CellModel&);
  void on_laneChanged(const LaneModel&);
  void on_sceneChanged(const SceneModel&);
  void updateView();

  const ProcessModel& m_model;
  ClipLauncherView* m_view;
  QTimer* m_progressTimer{};
};

} // namespace ClipLauncher
