#pragma once

#include "Control/ScenarioControl.hpp"
#include <QObject>

class EditMenuActions : public QObject
{
    Q_OBJECT
    public:
        EditMenuActions(ScenarioControl* parent);
        QActionGroup* editActions();

    private:
        ScenarioControl* m_parent;
        QActionGroup* m_editActions{};
};

