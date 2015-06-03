#pragma once

#include "Control/ScenarioControl.hpp"
#include <QObject>

class ObjectMenuActions : public QObject
{
    Q_OBJECT
    public:
        ObjectMenuActions(ScenarioControl* parent);
        void fillMenuBar(iscore::MenubarManager *menu);
        void fillContextMenu(QMenu* menu);

        QList<QAction*> actions();

    private:
        ScenarioControl* m_parent;

        QAction* m_removeElements;
        QAction *m_clearElements;
        QAction *m_copyContent;
        QAction *m_cutContent;
        QAction *m_pasteContent;
        QAction *m_elementsToJson;

};
