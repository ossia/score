#pragma once
#include <QTreeView>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

class StateModel;
class DeviceExplorerModel;
class MessageItemModel;
class MessageTreeView final : public QTreeView
{
    public:
        MessageTreeView(
                const StateModel& model,
                QWidget* parent);

        MessageItemModel& model() const;

        void removeNodes();

    protected:
        void resizeEvent(QResizeEvent* ev) override;

    private:
        //void mouseDoubleClickEvent(QMouseEvent* ev) override;
        void contextMenuEvent(QContextMenuEvent*) override;

        QAction* m_removeNodesAction{};
        const StateModel& m_model;

        CommandDispatcher<> m_dispatcher;
        float m_valueColumnSize{0.15};
};
