#pragma once
#include <QTreeView>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

class StateModel;
class DeviceExplorerModel;
/*
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
*/
// TODO MOVEME
namespace iscore
{
class MessageItemModel;
}
class MessageTreeView :  public QTreeView
{
    public:
        MessageTreeView(
                const StateModel& model,
                DeviceExplorerModel* devexplorer,
                QWidget* parent);

        iscore::MessageItemModel& model() const;

        void removeNodes();

    private:
        //void mouseDoubleClickEvent(QMouseEvent* ev) override;
        void contextMenuEvent(QContextMenuEvent*) override;

        QAction* m_removeNodesAction{};
        StateModel* m_model{};
        DeviceExplorerModel* m_devExplorer{};

        CommandDispatcher<> m_dispatcher;
};


/*
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
*/
