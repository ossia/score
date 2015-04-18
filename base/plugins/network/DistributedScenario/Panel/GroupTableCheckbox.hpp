#pragma once
#include <QWidget>

class GroupTableCheckbox : public QWidget
{
        Q_OBJECT
    public:
        GroupTableCheckbox();

    signals:
        void stateChanged(int);
};
