#pragma once
#include <QObject>

class SlotPresenter;
class QPoint;
class QPointF;
class LayerPresenter;
class ScenarioControl;
class TemporalScenarioPresenter;
class QMenu;

// TODO rename file
class ScenarioContextMenuManager : public QObject
{
        ScenarioControl& m_control;
    public:
        ScenarioContextMenuManager(ScenarioControl& control):
            m_control{control}
        {

        }

        void createSlotContextMenu(
                QMenu& menu,
                const SlotPresenter& slotp);

        void createScenarioContextMenu(
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
