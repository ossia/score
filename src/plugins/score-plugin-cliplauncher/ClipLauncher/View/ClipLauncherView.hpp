#pragma once
#include <Process/LayerView.hpp>

#include <score_plugin_cliplauncher_export.h>

#include <verdigris>

class QMimeData;
namespace ClipLauncher
{
class ProcessModel;

class ClipLauncherView final : public Process::LayerView
{
  W_OBJECT(ClipLauncherView)
public:
  static constexpr double LaneHeaderHeight = 25.0;
  static constexpr double SceneHeaderWidth = 80.0;
  static constexpr double MaxCellWidth = 140.0;
  static constexpr double MaxCellHeight = 70.0;
  static constexpr double CellPadding = 2.0;
  static constexpr double SceneLaunchButtonWidth = 25.0;
  static constexpr double MinCellWidth = 40.0;
  static constexpr double MinCellHeight = 25.0;

  explicit ClipLauncherView(QGraphicsItem* parent);
  ~ClipLauncherView() override;

  void setModel(const ProcessModel* model);

  void paint_impl(QPainter* painter) const override;

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

  void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragMoveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

  // Signals
  void cellClicked(int lane, int scene)
      E_SIGNAL(SCORE_PLUGIN_CLIPLAUNCHER_EXPORT, cellClicked, lane, scene)
  void cellDoubleClicked(int lane, int scene)
      E_SIGNAL(SCORE_PLUGIN_CLIPLAUNCHER_EXPORT, cellDoubleClicked, lane, scene)
  void sceneLaunchClicked(int scene)
      E_SIGNAL(SCORE_PLUGIN_CLIPLAUNCHER_EXPORT, sceneLaunchClicked, scene)
  void dropOnCell(int lane, int scene, const QMimeData& mime)
      E_SIGNAL(SCORE_PLUGIN_CLIPLAUNCHER_EXPORT, dropOnCell, lane, scene, mime)
  void laneHeaderClicked(int lane)
      E_SIGNAL(SCORE_PLUGIN_CLIPLAUNCHER_EXPORT, laneHeaderClicked, lane)
  void sceneHeaderClicked(int scene)
      E_SIGNAL(SCORE_PLUGIN_CLIPLAUNCHER_EXPORT, sceneHeaderClicked, scene)

  std::optional<std::pair<int, int>> cellAtPos(QPointF pos) const;
  std::optional<int> sceneLaunchAtPos(QPointF pos) const;
  std::optional<int> laneHeaderAtPos(QPointF pos) const;
  std::optional<int> sceneHeaderAtPos(QPointF pos) const;

  // Dynamic cell sizing
  double cellWidth() const;
  double cellHeight() const;

private:
  QRectF cellRect(int lane, int scene) const;

  void paintGrid(QPainter* painter) const;
  void paintLaneHeaders(QPainter* painter) const;
  void paintSceneHeaders(QPainter* painter) const;
  void paintCell(QPainter* painter, int lane, int scene, const QRectF& rect) const;
  void paintSceneLaunchButton(QPainter* painter, int scene, const QRectF& rect) const;

  const ProcessModel* m_model{};
};

} // namespace ClipLauncher
