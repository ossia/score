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
class ScenarioContextMenuManager final : public QObject
{
    public:
        static void createSlotContextMenu(
                const iscore::DocumentContext& docContext,
                QMenu& menu,
                const SlotPresenter& slotp);

        static void createScenarioContextMenu(
                const iscore::DocumentContext& ctx,
                QMenu& menu,
                const QPoint& pos,
                const QPointF& scenepos,
                const TemporalScenarioPresenter& pres);

        static void createLayerContextMenu(
                QMenu& menu,
                QPoint pos,
                QPointF scenepos,
                const Process::LayerContextMenuManager&,
                const Process::LayerPresenter& pres);

};
}
