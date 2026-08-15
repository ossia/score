#include "DeviceExplorerDelegate.hpp"

#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>

#include <Explorer/Explorer/Column.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/Explorer/ValueEditors.hpp>

#include <QAbstractProxyModel>

#include <algorithm>

namespace Explorer
{
DeviceExplorerDelegate::DeviceExplorerDelegate(QObject* parent)
    : QStyledItemDelegate{parent}
{
}

DeviceExplorerDelegate::~DeviceExplorerDelegate() = default;

const Device::AddressSettings*
DeviceExplorerDelegate::addressAt(const QModelIndex& index) noexcept
{
  // The view may be looking at the tree through the search filter.
  QModelIndex idx = index;
  while(auto* proxy = qobject_cast<const QAbstractProxyModel*>(idx.model()))
    idx = proxy->mapToSource(idx);

  if(!qobject_cast<const DeviceExplorerModel*>(idx.model()))
    return nullptr;
  if(!idx.isValid() || idx.column() != (int)Column::Value)
    return nullptr;

  auto* node = static_cast<Device::Node*>(idx.internalPointer());
  if(!node || !node->is<Device::AddressSettings>())
    return nullptr;

  return &node->get<Device::AddressSettings>();
}

QWidget* DeviceExplorerDelegate::createEditor(
    QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
  if(auto* addr = addressAt(index))
  {
    if(auto* w = make_value_widget(*addr, parent, ValueEditorSize::Compact))
    {
      if(w->commitsImmediately())
      {
        auto* self = const_cast<DeviceExplorerDelegate*>(this);
        connect(w, &AddressValueWidget::changed, self, [self, w](const ossia::value&) {
          self->commitData(w);
        });
      }
      return w;
    }
  }

  return QStyledItemDelegate::createEditor(parent, option, index);
}

void DeviceExplorerDelegate::setEditorData(
    QWidget* editor, const QModelIndex& index) const
{
  if(auto* w = qobject_cast<AddressValueWidget*>(editor))
  {
    if(auto* addr = addressAt(index))
    {
      w->set(addr->value);
      return;
    }
  }

  QStyledItemDelegate::setEditorData(editor, index);
}

void DeviceExplorerDelegate::setModelData(
    QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
  if(auto* w = qobject_cast<AddressValueWidget*>(editor))
  {
    if(!w->edited())
      return;

    if(auto v = w->get(); v.valid())
      model->setData(index, QVariant::fromValue(v), Qt::EditRole);
    return;
  }

  QStyledItemDelegate::setModelData(editor, model, index);
}

void DeviceExplorerDelegate::updateEditorGeometry(
    QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
  QStyledItemDelegate::updateEditorGeometry(editor, option, index);

  // A tree row is shorter than the editors: do not squash them into it.
  if(qobject_cast<AddressValueWidget*>(editor))
  {
    const auto hint = editor->minimumSizeHint();
    editor->resize(
        std::max(editor->width(), hint.width()),
        std::max(editor->height(), hint.height()));
  }
}
}
