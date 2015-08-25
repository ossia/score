#pragma once
#include <QTreeView>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

class StateModel;
class DeviceExplorerModel;

class StateTreeView : public QTreeView
{
    public:
        StateTreeView(
                const StateModel& model,
                DeviceExplorerModel* devexplorer,
                QWidget* parent);

    protected:
        void mouseDoubleClickEvent(QMouseEvent* ev) override;

        StateModel* m_model{};
        DeviceExplorerModel* m_devExplorer{};

        CommandDispatcher<> m_dispatcher;
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
