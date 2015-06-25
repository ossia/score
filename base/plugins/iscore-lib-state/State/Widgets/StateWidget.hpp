#pragma once
#include <QFrame>

namespace iscore{
class State;
}
class StateWidget : public QFrame
{
        Q_OBJECT
    public:
        StateWidget(const iscore::State& s,
                    QWidget* parent);

    signals:
        void removeMe();
};
