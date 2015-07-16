#pragma once

#include "AbstractMenuActions.hpp"

#include "DialogWidget/AddProcessDialog.hpp"

class ObjectMenuActions : public AbstractMenuActions
{
    public:
        ObjectMenuActions(iscore::ToplevelMenuElement, ScenarioControl* parent);
        void fillMenuBar(iscore::MenubarManager *menu) override;
        void fillContextMenu(QMenu* menu, const Selection&) override;

    private:
        QJsonObject copySelectedElementsToJson();
        QJsonObject cutSelectedElementsToJson();
        void writeJsonToSelectedElements(const QJsonObject &obj);
        void addProcessInConstraint(QString);

        QAction* m_removeElements;
        QAction *m_clearElements;
        QAction *m_copyContent;
        QAction *m_cutContent;
        QAction *m_pasteContent;
        QAction *m_elementsToJson;
        QAction *m_addProcess;

        AddProcessDialog* m_addProcessDialog;
};
