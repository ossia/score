#pragma once
#include <QTreeView>
#include <State/StateItemModel.hpp>
namespace iscore {
class StateItemModel;
}
class DeviceExplorerModel;
class StateTreeView : public QTreeView
{
    public:
        StateTreeView(
                iscore::StateItemModel* model,
                DeviceExplorerModel* devexplorer,
                QWidget* parent);

    protected:
        void mouseDoubleClickEvent(QMouseEvent* ev) override;
        DeviceExplorerModel* m_devExplorer{};
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
