#pragma once

#include "Control/ScenarioControl.hpp"
#include <QObject>

class EditMenuActions : public QObject
{
    Q_OBJECT
    public:
        EditMenuActions(ScenarioControl* parent);
        void fillMenuBar(iscore::MenubarManager *menu);

        QList<QAction*> actions();

    private:
        ScenarioControl* m_parent;

        QAction* m_removeElements;
        QAction *m_clearElements;
        QAction *m_copyConstraintContent;
        QAction *m_cutConstraintContent;
        QAction *m_pasteConstraintContent;
};

