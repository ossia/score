#pragma once
#include <QObject>

class SlotPresenter;
class QPoint;
class QPointF;
class LayerPresenter;
class ScenarioApplicationPlugin;
class TemporalScenarioPresenter;
class QMenu;
namespace iscore
{
struct DocumentContext;
}

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
                const QPoint& pos,
                const QPointF& scenepos,
                const LayerPresenter& pres);
};
