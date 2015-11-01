#pragma once
#include <Scenario/Process/Temporal/StateMachines/ScenarioPoint.hpp>
#include "ScenarioActions.hpp"

#include <Scenario/DialogWidget/AddProcessDialog.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

class ObjectMenuActions : public ScenarioActions
{
    public:
        ObjectMenuActions(iscore::ToplevelMenuElement, ScenarioControl* parent);
        void fillMenuBar(iscore::MenubarManager *menu) override;
        void fillContextMenu(QMenu* menu, const Selection&, const TemporalScenarioPresenter& pres, const QPoint&, const QPointF&) override;
        bool populateToolBar(QToolBar*) override;
        void setEnabled(bool) override;

        QList<QAction*> actions() const override;
    private:
        QJsonObject copySelectedElementsToJson();
        QJsonObject cutSelectedElementsToJson();
        void pasteElements(const QJsonObject& obj, const ScenarioPoint& origin);
        void writeJsonToSelectedElements(const QJsonObject &obj);
        void addProcessInConstraint(QString);
        void addTriggerToTimeNode();
        void removeTriggerFromTimeNode();

        CommandDispatcher<> dispatcher();


        QAction* m_removeElements{};
        QAction *m_clearElements{};
        QAction *m_copyContent{};
        QAction *m_cutContent{};
        QAction *m_pasteContent{};
        QAction *m_elementsToJson{};

        // TODO separate classes for these
        // Constraint
        QAction *m_addProcess{};
        QAction *m_interp{};

        // Event
        QAction *m_addTrigger{};
        QAction *m_removeTrigger{};

        // State
        QAction* m_updateStates{};

        AddProcessDialog* m_addProcessDialog{};
};
