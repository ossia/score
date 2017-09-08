#pragma once
#include <QObject>

#include <Process/Layer/LayerContextMenu.hpp>
namespace Process
{
class LayerContextMenu;
class LayerPresenter;
}
class QMenu;
class QPoint;
class QPointF;
class QAction;

namespace iscore
{
struct DocumentContext;
}

namespace Scenario
{
class SlotPresenter;
class TemporalScenarioPresenter;
class TemporalIntervalPresenter;
class FullViewIntervalPresenter;
class ScenarioContextMenuManager final : public QObject
{
public:
  static void createSlotContextMenu(
      const iscore::DocumentContext& docContext,
      QMenu& menu,
      const FullViewIntervalPresenter& slotp,
      int slot_index);
  static void createSlotContextMenu(
      const iscore::DocumentContext& docContext,
      QMenu& menu,
      const TemporalIntervalPresenter& slotp,
      int slot_index);

  static void createLayerContextMenu(
      QMenu& menu,
      QPoint pos,
      QPointF scenepos,
      const Process::LayerContextMenuManager&,
      Process::LayerPresenter& pres);
};
}
