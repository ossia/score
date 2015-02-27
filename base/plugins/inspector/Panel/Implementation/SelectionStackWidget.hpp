#pragma once
#include <QWidget>

class SelectionStack;
class QToolButton;
class SelectionStackWidget : public QWidget
{
    public:
        SelectionStackWidget(SelectionStack* s, QWidget* parent);

    private:
        QToolButton* m_prev{};
        QToolButton* m_next{};
        SelectionStack* m_stack{};
};
