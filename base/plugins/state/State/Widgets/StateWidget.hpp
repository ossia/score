#pragma once
#include <QFrame>

class State;
class StateWidget : public QFrame
{
        Q_OBJECT
    public:
        StateWidget(const State& s,
                    QWidget* parent);

    signals:
        void removeMe();
};
