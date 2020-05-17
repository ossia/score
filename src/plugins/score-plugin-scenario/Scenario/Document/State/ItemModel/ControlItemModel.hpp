#pragma once
#include <QAbstractItemModel>

#include <score_plugin_scenario_export.h>
namespace Process
{
struct ControlMessage;
}

namespace Scenario
{
class StateModel;
class SCORE_PLUGIN_SCENARIO_EXPORT ControlItemModel final : public QAbstractItemModel
{
public:
  ControlItemModel(Scenario::StateModel& ctx, QObject* parent);
  ~ControlItemModel();

  static void addMessages(
      std::vector<Process::ControlMessage>& existing,
      std::vector<Process::ControlMessage>&& added);
  void replaceWith(const std::vector<Process::ControlMessage>&);

  const std::vector<Process::ControlMessage>& messages() const noexcept { return m_msgs; }

private:
  QModelIndex index(int row, int column, const QModelIndex& parent) const override;

  QModelIndex parent(const QModelIndex& child) const override;
  int rowCount(const QModelIndex& parent) const override;
  int columnCount(const QModelIndex& parent) const override;
  QVariant data(const QModelIndex& index, int role) const override;

  Qt::DropActions supportedDragActions() const override;
  Qt::DropActions supportedDropActions() const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;

  Scenario::StateModel& m_state;
  std::vector<Process::ControlMessage> m_msgs;
};

}
