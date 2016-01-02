#pragma once
#include <Process/State/MessageNode.hpp>
#include <iscore/tools/TreeNodeItemModel.hpp>
#include <QAbstractItemModel>
#include <qnamespace.h>
#include <QStringList>
#include <QVariant>
#include <iscore_plugin_scenario_export.h>

class QMimeData;
class QObject;
class StateModel;
namespace iscore {
class CommandStackFacade;
}  // namespace iscore
namespace State
{
struct Message;
}

/**
 * @brief The MessageItemModel class
 *
 * Used as a wrapper with trees of node_type, to represent them
 * the Qt way.
 *
 */
class ISCORE_PLUGIN_SCENARIO_EXPORT MessageItemModel final : public TreeNodeBasedItemModel<MessageNode>
{
        Q_OBJECT

    public:
        using node_type = TreeNodeBasedItemModel<MessageNode>::node_type;

        enum class Column : int
        {
            Name = 0,
            Value,
            Count
        };

        MessageItemModel(
                iscore::CommandStackFacade& stack,
                const StateModel&,
                QObject* parent);
        MessageItemModel& operator=(const MessageItemModel&);
        MessageItemModel& operator=(const node_type&);
        MessageItemModel& operator=(node_type&&);

        const MessageNode& rootNode() const override
        { return m_rootNode; }
        MessageNode& rootNode() override
        { return m_rootNode; }

        // AbstractItemModel interface
        int columnCount(const QModelIndex &parent) const override;

        QVariant data(const QModelIndex &index, int role) const override;
        bool setData(const QModelIndex &index, const QVariant &value, int role) override;

        QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
        bool setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role) override;

        QStringList mimeTypes() const override;
        QMimeData *mimeData(const QModelIndexList &indexes) const override;
        bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const override;
        bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;

        Qt::DropActions supportedDragActions() const override;
        Qt::DropActions supportedDropActions() const override;

        Qt::ItemFlags flags(const QModelIndex &index) const override;

        const StateModel& stateModel;

    signals:
        void userMessage(const State::Message&);

    private:
        node_type m_rootNode;

        iscore::CommandStackFacade& m_stack;
};
