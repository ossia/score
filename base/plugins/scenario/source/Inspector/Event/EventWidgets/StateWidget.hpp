#pragma once
#include <QWidget>

class State;
class StateWidget : public QWidget
{
        Q_OBJECT
    public:
        StateWidget(const State& s, QWidget* parent);
};
