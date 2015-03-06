#pragma once
#include <QWidget>

namespace iscore{
    class SelectionStack;
}
class QToolButton;
class SelectionStackWidget : public QWidget
{
    public:
        SelectionStackWidget(iscore::SelectionStack* s, QWidget* parent);

    private:
        QToolButton* m_prev{};
        QToolButton* m_next{};
        iscore::SelectionStack* m_stack{};
};
