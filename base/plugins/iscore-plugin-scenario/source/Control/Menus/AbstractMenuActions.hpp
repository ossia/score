#pragma once

#include <QObject>
#include "Control/ScenarioControl.hpp"

class AbstractMenuActions : public QObject
{
    Q_OBJECT

    public:
    explicit AbstractMenuActions(iscore::ToplevelMenuElement, ScenarioControl *);
    virtual void fillMenuBar(iscore::MenubarManager*) = 0;
    virtual void fillContextMenu(QMenu*) = 0;
    virtual QList<QAction*> actions() = 0;

    protected:
    ScenarioControl* m_parent;
    iscore::ToplevelMenuElement m_menuElt;

};

