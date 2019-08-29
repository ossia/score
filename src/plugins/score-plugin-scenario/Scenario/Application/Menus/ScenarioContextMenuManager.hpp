#pragma once
#include <Process/Layer/LayerContextMenu.hpp>

#include <QObject>
namespace Process
{
class LayerContextMenu;
class LayerPresenter;
}
class QMenu;
class QPoint;
class QPointF;
class QAction;

namespace score
{
struct DocumentContext;
}

namespace Scenario
{
struct SlotPresenter;
class ScenarioPresenter;
class TemporalIntervalPresenter;
class FullViewIntervalPresenter;
class ScenarioContextMenuManager final : public QObject
{
public:
  static void createProcessSelectorContextMenu(
      const score::DocumentContext& docContext,
      QMenu& menu,
      const TemporalIntervalPresenter& slotp,
      int slot_index);
};
}
