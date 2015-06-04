#pragma once

#include "AbstractMenuActions.hpp"

class ObjectMenuActions : public AbstractMenuActions
{
    public:
        ObjectMenuActions(iscore::ToplevelMenuElement,ScenarioControl* parent);
        void fillMenuBar(iscore::MenubarManager *menu);
        void fillContextMenu(QMenu* menu);

        QList<QAction*> actions();

    private:
        QJsonObject copySelectedElementsToJson();
        QJsonObject cutSelectedElementsToJson();
        void writeJsonToSelectedElements(const QJsonObject &obj);

        QAction* m_removeElements;
        QAction *m_clearElements;
        QAction *m_copyContent;
        QAction *m_cutContent;
        QAction *m_pasteContent;
        QAction *m_elementsToJson;

};
