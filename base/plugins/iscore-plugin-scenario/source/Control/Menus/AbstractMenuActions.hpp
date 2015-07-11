#pragma once
#include <iscore/selection/Selection.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <core/presenter/MenubarManager.hpp>
class QAction;
class ScenarioControl;

class AbstractMenuActions : public QObject
{
        Q_OBJECT

    public:
        explicit AbstractMenuActions(iscore::ToplevelMenuElement, ScenarioControl *);
        virtual void fillMenuBar(iscore::MenubarManager*) = 0;
        virtual void fillContextMenu(QMenu*, const Selection& s) = 0;

    protected:
        ScenarioControl* m_parent;
        iscore::ToplevelMenuElement m_menuElt;
};

