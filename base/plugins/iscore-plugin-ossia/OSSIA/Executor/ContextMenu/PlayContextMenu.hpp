#pragma once
#include <QPoint>

#include <Scenario/Application/Menus/ScenarioActions.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/menu/MenuInterface.hpp>
class QAction;
class QMenu;
namespace Scenario
{
class ScenarioApplicationPlugin;
class TemporalScenarioPresenter;
}
namespace iscore
{
class MenubarManager;
}  // namespace iscore

namespace RecreateOnPlay
{
class PlayContextMenu final : public QObject
{
    public:
        PlayContextMenu(Scenario::ScenarioApplicationPlugin* parent);
        void fillMenuBar(iscore::MenubarManager *menu) ;
        void fillContextMenu(QMenu* menu, const Selection&, const Scenario::TemporalScenarioPresenter& pres, const QPoint&, const QPointF&) ;

        void setEnabled(bool);

        const QAction& playFromHereAction() { return *m_playFromHere;}

    private:
        Scenario::ScenarioApplicationPlugin* m_parent{};
        iscore::ToplevelMenuElement m_menuElt;

        QAction* m_recordAutomations{};
        QAction* m_recordMessages{};

        QAction *m_playStates{};
        QAction *m_playEvents{};
        QAction *m_playConstraints{};

        QAction* m_playFromHere{};
};
}
