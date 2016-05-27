#pragma once
#include <QPoint>

#include <Scenario/Application/Menus/ScenarioActions.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <iscore/actions/Action.hpp>
class QAction;
class QMenu;
namespace Process
{
class LayerContextMenuManager;
}

namespace Scenario
{
class ScenarioApplicationPlugin;
class TemporalScenarioPresenter;
}
class OSSIAApplicationPlugin;
namespace RecreateOnPlay
{
class PlayContextMenu final : public QObject
{
    public:
        PlayContextMenu(
                OSSIAApplicationPlugin& plug,
                const iscore::ApplicationContext& ctx);
        void fillContextMenu(QMenu* menu, const Selection&, const Scenario::TemporalScenarioPresenter& pres, const QPoint&, const QPointF&) ;
        void setupContextMenu(Process::LayerContextMenuManager& ctxm);

        void setEnabled(bool);

    private:
        const iscore::ApplicationContext& m_ctx;

        QAction* m_recordAutomations{};
        QAction* m_recordMessages{};

        QAction *m_playStates{};
        QAction *m_playEvents{};
        QAction *m_playConstraints{};

        QAction* m_playFromHere{};
};
}
