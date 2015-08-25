#pragma once
#include <QTreeView>
#include <State/StateItemModel.hpp>
class StateItemModel;
class StateTreeView : public QTreeView
{
    public:
        using QTreeView::QTreeView;

    protected:
        void mouseDoubleClickEvent(QMouseEvent* ev) override;
};


class StateTreeWidget : public QWidget
{
    public:
        StateTreeWidget(QWidget* parent):
            QWidget{parent}
        {

        }

    private:
        StateTreeView* m_view{};

};
