#pragma once
#include <Process/State/MessageNode.hpp>

#include <score/model/tree/TreeNodeItemModel.hpp>

#include <QStringList>
#include <QVariant>
#include <qnamespace.h>

#include <score_plugin_scenario_export.h>

#include <verdigris>

class QMimeData;
class QObject;
namespace score
{
class CommandStackFacade;
} // namespace score
namespace State
{
struct Message;
}

namespace Scenario
{
class StateModel;
/**
 * @brief The MessageItemModel class
 *
 * Used as a wrapper with trees of node_type, to represent them
 * the Qt way.
 *
 */
class SCORE_PLUGIN_SCENARIO_EXPORT MessageItemModel final
    : public TreeNodeBasedItemModel<Process::MessageNode>
{
  W_OBJECT(MessageItemModel)

public:
  using node_type = TreeNodeBasedItemModel<Process::MessageNode>::node_type;

  enum class Column : int
  {
    Name = 0,
    Value,
    Count
  };

  MessageItemModel(const StateModel&, QObject* parent);
  MessageItemModel& operator=(const MessageItemModel&);
  MessageItemModel& operator=(const node_type&);
  MessageItemModel& operator=(node_type&&);

  const Process::MessageNode& rootNode() const override { return m_rootNode; }

  Process::MessageNode& rootNode() override { return m_rootNode; }

  // AbstractItemModel interface
  int columnCount(const QModelIndex& parent) const override;

  QVariant data(const QModelIndex& index, int role) const override;
  bool setData(const QModelIndex& index, const QVariant& value, int role) override;

  QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
  bool setHeaderData(int section, Qt::Orientation orientation, const QVariant& value, int role)
      override;

  QStringList mimeTypes() const override;
  QMimeData* mimeData(const QModelIndexList& indexes) const override;
  bool canDropMimeData(
      const QMimeData* data,
      Qt::DropAction action,
      int row,
      int column,
      const QModelIndex& parent) const override;
  bool dropMimeData(
      const QMimeData* data,
      Qt::DropAction action,
      int row,
      int column,
      const QModelIndex& parent) override;

  Qt::DropActions supportedDragActions() const override;
  Qt::DropActions supportedDropActions() const override;

  Qt::ItemFlags flags(const QModelIndex& index) const override;

  const StateModel& stateModel;

public:
  void userMessage(const State::Message& arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, userMessage, arg_1)

private:
  node_type m_rootNode;
};

QVariant valueColumnData(const MessageItemModel::node_type& node, int role);
}

DEFAULT_MODEL_METADATA(Scenario::MessageItemModel, "Message item model")
