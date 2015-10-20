#pragma once
#include <iscore/selection/Selection.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <core/presenter/MenubarManager.hpp>
#include <ProcessInterface/LayerPresenter.hpp>

#include <QToolBar>

class QAction;
class ScenarioControl;
class ScenarioActions : public QObject
{
        Q_OBJECT

    public:
        explicit ScenarioActions(iscore::ToplevelMenuElement, ScenarioControl *);

        virtual void fillMenuBar(iscore::MenubarManager*) = 0;
        virtual void fillContextMenu(QMenu*, const Selection& s, LayerPresenter* pres, const QPoint&, const QPointF&) = 0;
        virtual bool populateToolBar(QToolBar* ) { return false; }
        virtual void setEnabled(bool) = 0;

        // TODO reimplement me for all the classes
        virtual QList<QAction*> actions() const;

    protected:
        ScenarioControl* m_parent{};
        iscore::ToplevelMenuElement m_menuElt;
};

