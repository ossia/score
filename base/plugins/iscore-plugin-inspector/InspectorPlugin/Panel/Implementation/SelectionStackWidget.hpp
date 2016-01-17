#pragma once
#include <iscore/selection/Selection.hpp>
#include <QWidget>

namespace iscore{
    class SelectionStack;
}

class QToolButton;

namespace InspectorPanel
{
class SelectionStackWidget : public QWidget
{
    public:
        SelectionStackWidget(iscore::SelectionStack& s, QWidget* parent);

    public slots:
        void selectionChanged(const Selection& s);

    private:
        QToolButton* m_prev{};
        QToolButton* m_next{};
        iscore::SelectionStack& m_stack;
};
}
