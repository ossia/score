#pragma once
#include <Scenario/Application/Menus/ScenarioActions.hpp>
#include <QList>
#include <QPoint>

#include <iscore/menu/MenuInterface.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/actions/Action.hpp>

class QAction;
class QMenu;
class QToolBar;
namespace iscore {
class MenubarManager;
}  // namespace iscore

namespace Scenario
{

class ScenarioApplicationPlugin;
class TemporalScenarioPresenter;
class TransportActions : public QObject
{
    public:
        TransportActions(
                iscore::ToplevelMenuElement menuElt,
                ScenarioApplicationPlugin* parent);

        void makeGUIElements(iscore::GUIElements& ref);

    private:
        iscore::ToplevelMenuElement m_menuElt;
        ScenarioApplicationPlugin* m_parent{};

        QAction* m_play{};
        QAction* m_pause{};
        QAction* m_stop{};
        QAction* m_goToStart{};
        QAction* m_goToEnd{};
        QAction* m_stopAndInit{};
        QAction* m_record{};
};

}
