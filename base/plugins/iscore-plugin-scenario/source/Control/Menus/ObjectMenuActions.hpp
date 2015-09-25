#pragma once

#include "AbstractMenuActions.hpp"

#include "DialogWidget/AddProcessDialog.hpp"
#include "Process/ScenarioModel.hpp"

class ObjectMenuActions : public ScenarioActions
{
    public:
        ObjectMenuActions(iscore::ToplevelMenuElement, ScenarioControl* parent);
        void fillMenuBar(iscore::MenubarManager *menu) override;
        void fillContextMenu(QMenu* menu, const Selection&, LayerPresenter* pres, const QPoint&, const QPointF&) override;
        void makeToolBar(QToolBar*) override;
        void setEnabled(bool) override;

        QList<QAction*> actions() const override;
    private:
        QJsonObject copySelectedElementsToJson();
        QJsonObject cutSelectedElementsToJson();
        void writeJsonToSelectedElements(const QJsonObject &obj);
        void addProcessInConstraint(QString);
        void addTriggerToTimeNode();
        void removeTriggerFromTimeNode();

        CommandDispatcher<> dispatcher();


        QAction* m_removeElements;
        QAction *m_clearElements;
        QAction *m_copyContent;
        QAction *m_cutContent;
        QAction *m_pasteContent;
        QAction *m_elementsToJson;
        QAction *m_addProcess;
        QAction *m_addTrigger;
        QAction *m_removeTrigger;

        AddProcessDialog* m_addProcessDialog;
};
