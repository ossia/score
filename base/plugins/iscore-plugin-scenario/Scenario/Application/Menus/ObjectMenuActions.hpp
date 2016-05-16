#pragma once
#include <QJsonObject>
#include <QList>
#include <QPoint>


#include "ScenarioActions.hpp"
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <iscore/selection/Selection.hpp>

class QAction;
class QMenu;
class QToolBar;

namespace iscore {
class MenubarManager;
}  // namespace iscore

namespace Scenario
{
struct Point;
class ScenarioApplicationPlugin;
class TemporalScenarioPresenter;
class EventActions;
class ConstraintActions;
class StateActions;
class ObjectMenuActions final : public ScenarioActions
{
    public:
        ObjectMenuActions(iscore::ToplevelMenuElement, ScenarioApplicationPlugin* parent);
        void fillMenuBar(iscore::MenubarManager *menu) override;
        void fillContextMenu(QMenu* menu, const Selection&, const TemporalScenarioPresenter& pres, const QPoint&, const QPointF&) override;
        void setEnabled(bool) override;
        bool populateToolBar(QToolBar*) override;


        QList<QAction*> actions() const override;
        CommandDispatcher<> dispatcher() const;

        EventActions* eventActions() const
        { return m_eventActions; }
        ConstraintActions* constraintActions() const
        { return m_cstrActions; }
        StateActions* stateActions() const
        { return m_stateActions; }

        QAction* clearContent()
        { return m_clearElements; }
        QAction* copyContent()
        { return m_copyContent; }
        QAction* pasteContent()
        { return m_pasteContent; }
        QAction* elementsToJson()
        { return m_elementsToJson; }

    private:
        QJsonObject copySelectedElementsToJson();
        QJsonObject cutSelectedElementsToJson();
        void pasteElements(const QJsonObject& obj, const Scenario::Point& origin);
        void writeJsonToSelectedElements(const QJsonObject &obj);


        EventActions* m_eventActions{};
        ConstraintActions* m_cstrActions{};
        StateActions* m_stateActions{};

        QAction* m_removeElements{};
        QAction *m_clearElements{};
        QAction *m_copyContent{};
        QAction *m_cutContent{};
        QAction *m_pasteContent{};
        QAction *m_elementsToJson{};
};
}
