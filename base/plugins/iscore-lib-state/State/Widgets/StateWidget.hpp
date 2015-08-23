#pragma once
#include <QFrame>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
namespace iscore{
class State;
}
class StateWidget : public QFrame
{
        Q_OBJECT
    public:
        StateWidget(
                const iscore::State& s,
                const CommandDispatcher<> &,
                QWidget* parent);

    signals:
        void removeMe();
};
