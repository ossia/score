#pragma once

#include <QList>
#include <QPoint>

#include <iscore/menu/MenuInterface.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/actions/Action.hpp>

namespace iscore {
class GUIApplicationContext;
}
class QAction;
class QMenu;
class QToolBar;

namespace Scenario
{
class TransportActions : public QObject
{
    public:
        TransportActions(
                const iscore::GUIApplicationContext&);

        void makeGUIElements(iscore::GUIElements& ref);

    private:
        const iscore::ApplicationContext& m_context;

        QAction* m_play{};
        QAction* m_stop{};
        QAction* m_goToStart{};
        QAction* m_goToEnd{};
        QAction* m_stopAndInit{};
        QAction* m_record{};
};

}
