#pragma once
#include <Process/Preset.hpp>

#include <QAbstractItemModel>
#include <QSortFilterProxyModel>

#include <map>

namespace score
{
struct GUIApplicationContext;
}

namespace Library
{
class PresetFilterProxy;
class PresetItemModel final : public QAbstractItemModel
{
public:
  PresetItemModel(const score::GUIApplicationContext& ctx, QObject* parent);

  void registerPreset(const Process::ProcessFactoryList& procs, const QString& path);

private:
  QModelIndex index(int row, int column, const QModelIndex& parent) const override;

  QModelIndex parent(const QModelIndex& child) const override;
  int rowCount(const QModelIndex& parent) const override;
  int columnCount(const QModelIndex& parent) const override;
  QVariant data(const QModelIndex& index, int role) const override;

  bool setData(const QModelIndex& index, const QVariant& value, int role) override;

  bool dropMimeData(
      const QMimeData* data,
      Qt::DropAction act,
      int row,
      int col,
      const QModelIndex& parent) override;
  bool canDropMimeData(
      const QMimeData* data,
      Qt::DropAction act,
      int row,
      int col,
      const QModelIndex& parent) const override;

  QMimeData* mimeData(const QModelIndexList& indexes) const override;
  Qt::DropActions supportedDragActions() const override;
  Qt::DropActions supportedDropActions() const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;

  std::vector<Process::Preset> presets;

  friend class PresetFilterProxy;
};

class PresetFilterProxy final : public QSortFilterProxyModel
{
public:
  using QSortFilterProxyModel::invalidate;
  using QSortFilterProxyModel::QSortFilterProxyModel;

  UuidKey<Process::ProcessModel> currentFilter{};

private:
  bool filterAcceptsRow(int srcRow, const QModelIndex& srcParent) const override;
};
}
