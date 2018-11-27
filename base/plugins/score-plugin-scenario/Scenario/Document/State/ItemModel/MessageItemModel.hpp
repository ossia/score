#pragma once
#include <Process/State/MessageNode.hpp>

#include <score/model/tree/TreeNodeItemModel.hpp>

#include <QAbstractItemModel>
#include <QStringList>
#include <QVariant>
#include <qnamespace.h>

#include <score_plugin_scenario_export.h>
#include <wobjectdefs.h>

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
    : public QAbstractItemModel
{
  W_OBJECT(MessageItemModel)

public:
  enum class Column : int
  {
    Name = 0,
    Value,
    Count
  };

  const State::MessageList& rootNode() const noexcept { return m_rootNode; }
  State::MessageList& rootNode() noexcept { return m_rootNode; }

  MessageItemModel(const StateModel&, QObject* parent);
  MessageItemModel& operator=(const MessageItemModel&);
  MessageItemModel& operator=(const State::MessageList&);
  MessageItemModel& operator=(State::MessageList&&);

  // AbstractItemModel interface
  int rowCount(const QModelIndex& parent) const override;
  int columnCount(const QModelIndex& parent) const override;
  QModelIndex index(int row, int column, const QModelIndex& parent) const override;
  QModelIndex parent(const QModelIndex& child) const override;

  QVariant data(const QModelIndex& index, int role) const override;
  bool
  setData(const QModelIndex& index, const QVariant& value, int role) override;

  QVariant headerData(
      int section, Qt::Orientation orientation, int role) const override;
  bool setHeaderData(
      int section, Qt::Orientation orientation, const QVariant& value,
      int role) override;

  QStringList mimeTypes() const override;
  QMimeData* mimeData(const QModelIndexList& indexes) const override;
  bool canDropMimeData(
      const QMimeData* data, Qt::DropAction action, int row, int column,
      const QModelIndex& parent) const override;
  bool dropMimeData(
      const QMimeData* data, Qt::DropAction action, int row, int column,
      const QModelIndex& parent) override;

  Qt::DropActions supportedDragActions() const override;
  Qt::DropActions supportedDropActions() const override;

  Qt::ItemFlags flags(const QModelIndex& index) const override;

  const StateModel& stateModel;

private:
  State::MessageList m_rootNode;

};

QVariant valueColumnData(const State::Message& node, int role);
}

DEFAULT_MODEL_METADATA(Scenario::MessageItemModel, "Message item model")
