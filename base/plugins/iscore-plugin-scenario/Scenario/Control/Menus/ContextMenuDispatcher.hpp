#pragma once
#include <QObject>

class SlotPresenter;
class QPoint;
class QPointF;
class LayerPresenter;
class ScenarioControl;
class TemporalScenarioPresenter;
class QMenu;
namespace iscore
{
struct DocumentContext;
}

// TODO rename file
class ScenarioContextMenuManager final : public QObject
{
        ScenarioControl& m_control;
    public:
        ScenarioContextMenuManager(ScenarioControl& control):
            m_control{control}
        {

        }

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

        void createLayerContextMenu(
                QMenu& menu,
                const QPoint& pos,
                const QPointF& scenepos,
                const LayerPresenter& pres);
};
